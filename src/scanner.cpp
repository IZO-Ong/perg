#include "perg/scanner.hpp"

#include <algorithm>
#include <cstring>
#include <functional>
#include <regex>

namespace Perg {

/**
 * @brief Rapidly counts newline characters in a memory block using SIMD-optimized memchr.
 * @return The total number of '\n' characters found.
 */
inline int count_newlines(const char* start, const char* end) {
    int count = 0;
    const char* p = start;
    while (p < end) {
        const char* next_nl = static_cast<const char*>(std::memchr(p, '\n', end - p));
        if (!next_nl) break;
        count++;
        p = next_nl + 1;
    }
    return count;
}

/**
 * @brief Heuristically determines if a file is binary by checking for null bytes.
 * @note Only scans the first 1024 bytes for performance; typical of grep-like tools.
 */
bool is_binary(std::string_view content) {
    size_t check_limit = std::min(content.size(), (size_t)1024);
    if (check_limit == 0) return false;
    return std::memchr(content.data(), '\0', check_limit) != nullptr;
}

/**
 * @brief Searches backwards for the Nth newline character.
 * @details Used to "bleed" into previous chunks to retrieve leading context 
 * when a match occurs at the very start of a thread's assigned range.
 */
size_t find_backward_nl(std::string_view content, size_t pos, int n) {
    if (n <= 0 || pos == 0 || pos >= content.size()) return 0;
    size_t current = pos;
    int found = 0;
    while (current > 0) {
        if (content[current] == '\n') {
            if (++found > n) return current + 1;
        }
        current--;
    }
    return 0;
}

/**
 * @brief Identifies if a string is a literal or contains regex meta-characters.
 * @return True if the string can be searched using fast literal-matching algorithms.
 */
bool is_literal(const std::string& p) {
    return p.find_first_of(".+*?^$()[]{}|\\") == std::string::npos;
}

/**
 * @brief Core scanning logic for a specific memory chunk.
 * * This function implements a "Bleed-Both-Ways" strategy to ensure context is 
 * captured even across thread boundaries.
 * * @param full_content The entire memory-mapped file view.
 * @param pattern The search string or regex.
 * @param filename The name of the file (for result labeling).
 * @param start_line The 1-based line number where this chunk theoretically starts.
 * @param range_start The byte offset where this thread's responsibility begins.
 * @param range_end The byte offset where this thread's responsibility ends.
 * @return A FileResult containing all matches found within the responsibility zone.
 */
FileResult Scanner::scan_chunk(std::string_view full_content, const std::string& pattern, 
                               const std::string& filename, int start_line, 
                               size_t range_start, size_t range_end) {
    FileResult file_res;
    file_res.filename = filename;
    if (range_end == std::string_view::npos) range_end = full_content.size();

    // Establish a search area that includes context padding outside the 
    // thread's main responsibility range.
    size_t scan_start = range_start;
    if (!options_.count_only && options_.context_before > 0 && range_start > 0) {
        scan_start = find_backward_nl(full_content, range_start - 1, options_.context_before);
    }

    size_t scan_end = range_end;
    if (!options_.count_only && options_.context_after > 0 && range_end < full_content.size()) {
        size_t found_pos = range_end;
        for (int i = 0; i < options_.context_after; ++i) {
            size_t next_nl = full_content.find('\n', found_pos);
            if (next_nl == std::string_view::npos) { found_pos = full_content.size(); break; }
            found_pos = next_nl + 1;
        }
        scan_end = found_pos;
    }

    std::string_view search_view = full_content.substr(scan_start, scan_end - scan_start);
    if (is_binary(search_view)) return file_res;

    // Recalculate line numbers to account for the 'before' bleed area
    int current_line_no = start_line - count_newlines(full_content.data() + scan_start, 
                                                           full_content.data() + range_start);
    
    const char* current = search_view.data();
    const char* end = search_view.data() + search_view.size();
    int last_added_line_no = -1;
    int after_context_remaining = 0;

    std::vector<std::pair<int, std::string_view>> before_buffer;
    if (!options_.count_only && options_.context_before > 0) before_buffer.reserve(options_.context_before);

    bool use_fast_path = is_literal(pattern) && !options_.ignore_case;
    
    // thread_local regex objects avoid the massive overhead of re-compiling 
    // the regex for every chunk while remaining thread-safe
    static thread_local std::regex re; 
    static thread_local std::string last_pattern;
    if (!use_fast_path && last_pattern != pattern) {
        re = std::regex(pattern, std::regex::ECMAScript | std::regex::optimize | 
                       (options_.ignore_case ? std::regex::icase : (std::regex_constants::syntax_option_type)0));
        last_pattern = pattern;
    }

    while (current < end) {
        const char* next_nl = static_cast<const char*>(std::memchr(current, '\n', end - current));
        const char* line_end = next_nl ? next_nl : end;
        std::string_view current_line(current, line_end - current);

        // is_in_zone ensures that if a match occurs in a bleed area, only 
        // the thread actually 'owning' that byte range counts it. 
        size_t current_offset = current - full_content.data();
        bool is_in_zone = (current_offset >= range_start && current_offset < range_end);

        size_t occurrences_on_this_line = 0;
        if (use_fast_path) {
            size_t pos = current_line.find(pattern, 0);
            while (pos != std::string_view::npos) {
                occurrences_on_this_line++;
                pos = current_line.find(pattern, pos + pattern.length());
            }
        } else {
            auto words_begin = std::cregex_iterator(current_line.begin(), current_line.end(), re);
            auto words_end = std::cregex_iterator();
            occurrences_on_this_line = std::distance(words_begin, words_end);
        }

        if (occurrences_on_this_line > 0 && is_in_zone) {
            file_res.total_matches += occurrences_on_this_line;

            if (!options_.count_only) {
                for (const auto& b : before_buffer) {
                    if (b.first > last_added_line_no) {
                        file_res.matches.push_back({b.first, b.second, true});
                        last_added_line_no = b.first;
                    }
                }
                before_buffer.clear();

                if (current_line_no > last_added_line_no) {
                    file_res.matches.push_back({current_line_no, current_line, false});
                    last_added_line_no = current_line_no;
                } else if (!file_res.matches.empty() && file_res.matches.back().line_no == current_line_no) {
                    file_res.matches.back().is_context = false; 
                }
                after_context_remaining = options_.context_after;
            }
        } 
        else if (!options_.count_only) {
            if (after_context_remaining > 0) {
                if (current_line_no > last_added_line_no) {
                    file_res.matches.push_back({current_line_no, current_line, true});
                    last_added_line_no = current_line_no;
                }
                after_context_remaining--;
            } 
            else if (options_.context_before > 0) {
                if ((int)before_buffer.size() >= options_.context_before) before_buffer.erase(before_buffer.begin());
                before_buffer.push_back({current_line_no, current_line});
            }
        }

        if (!next_nl) break;
        current = next_nl + 1;
        current_line_no++;
    }

    return file_res;
}

/**
 * @brief Scans an entire file view as a single chunk.
 */
FileResult Scanner::scan(std::string_view content, const std::string& pattern, const std::string& filename) {
    return scan_chunk(content, pattern, filename, 1, 0, content.size());
}

} // namespace Perg