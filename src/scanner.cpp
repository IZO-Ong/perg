#include "perg/scanner.hpp"

#include <algorithm>
#include <cstring>
#include <functional>
#include <regex>

namespace Perg {

/**
 * @brief Rapidly counts newline characters in a memory block using SIMD-optimized memchr.
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
 * @brief Finds the start of the current line by scanning backwards for a newline.
 */
inline const char* find_line_start(const char* begin, const char* match) {
    if (begin == match) return begin;
    
    const char* last_nl = static_cast<const char*>(
        memrchr(begin, '\n', match - begin)
    );
    
    return last_nl ? last_nl + 1 : begin;
}

/**
 * @brief Finds the end of the current line by scanning forwards for a newline.
 */
inline const char* find_line_end(const char* match, const char* end) {
    const char* next_nl = static_cast<const char*>(std::memchr(match, '\n', end - match));
    return next_nl ? next_nl : end;
}

bool is_binary(std::string_view content) {
    size_t check_limit = std::min(content.size(), (size_t)1024);
    if (check_limit == 0) return false;
    return std::memchr(content.data(), '\0', check_limit) != nullptr;
}

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

bool is_literal(const std::string& p) {
    return p.find_first_of(".+*?^$()[]{}|\\") == std::string::npos;
}

/**
 * @brief Core scanning logic using a Search-First optimization.
 * * Instead of scanning every line for a newline character, we scan for the pattern
 * directly in the memory block. We only resolve line numbers and boundaries 
 * on-demand when a match is found.
 */
FileResult Scanner::scan_chunk(std::string_view full_content, const std::string& pattern, 
                               const std::string& filename, int start_line, 
                               size_t range_start, size_t range_end) {
    FileResult file_res;
    file_res.filename = filename;
    if (range_end == std::string_view::npos) range_end = full_content.size();

    // Bleed calculation for context and cross-thread safety
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

    // Line number synchronization
    const char* chunk_begin = search_view.data();
    const char* chunk_end = chunk_begin + search_view.size();
    const char* last_newline_counted_ptr = chunk_begin;
    int current_line_no = start_line - count_newlines(full_content.data() + scan_start, 
                                                      full_content.data() + range_start);

    bool use_fast_path = is_literal(pattern) && !options_.ignore_case;
    static thread_local std::regex re; 
    static thread_local std::string last_pattern;
    if (!use_fast_path && last_pattern != pattern) {
        re = std::regex(pattern, std::regex::ECMAScript | std::regex::optimize | 
                       (options_.ignore_case ? std::regex::icase : (std::regex_constants::syntax_option_type)0));
        last_pattern = pattern;
    }

    const char* current_search_ptr = chunk_begin;
    int last_added_line_no = -1;

    // --- MAIN SEARCH LOOP ---
    while (current_search_ptr < chunk_end) {
        const char* match_ptr = nullptr;

        if (use_fast_path) {
            // High-speed literal search
            match_ptr = (const char*)memmem(current_search_ptr, chunk_end - current_search_ptr, 
                                            pattern.data(), pattern.size());
        } else {
            // Regex search (remains line-oriented due to std::regex constraints)
            const char* next_nl = (const char*)std::memchr(current_search_ptr, '\n', chunk_end - current_search_ptr);
            const char* eol = next_nl ? next_nl : chunk_end;
            if (std::regex_search(current_search_ptr, eol, re)) {
                match_ptr = current_search_ptr;
            }
            if (!match_ptr) {
                if (!next_nl) break;
                current_search_ptr = next_nl + 1;
                continue;
            }
        }

        if (!match_ptr) break;

        // Resolve line context
        const char* line_start = find_line_start(chunk_begin, match_ptr);
        const char* line_end = find_line_end(match_ptr, chunk_end);
        std::string_view current_line(line_start, line_end - line_start);

        // Sync line number to current match
        current_line_no += count_newlines(last_newline_counted_ptr, line_start);
        last_newline_counted_ptr = line_start;

        // Responsibility check: Match must start within the thread's assigned range
        size_t match_offset = line_start - full_content.data();
        if (match_offset >= range_start && match_offset < range_end) {
            
            // Count all occurrences on this line
            size_t occurrences = 1;
            if (use_fast_path) {
                size_t p = current_line.find(pattern, (match_ptr - line_start) + pattern.size());
                while (p != std::string_view::npos) {
                    occurrences++;
                    p = current_line.find(pattern, p + pattern.size());
                }
            }
            file_res.total_matches += occurrences;

            if (!options_.count_only) {
                // Before-Context
                if (options_.context_before > 0) {
                    const char* ctx_ptr = line_start - 1;
                    std::vector<MatchRecord> before_ctx;
                    for (int i = 0; i < options_.context_before && ctx_ptr >= chunk_begin; ++i) {
                        const char* cs = find_line_start(chunk_begin, ctx_ptr);
                        int clno = current_line_no - (i + 1);
                        if (clno <= last_added_line_no) break;
                        before_ctx.push_back({clno, std::string_view(cs, ctx_ptr - cs + 1), true});
                        ctx_ptr = cs - 1;
                    }
                    for (auto it = before_ctx.rbegin(); it != before_ctx.rend(); ++it) {
                        file_res.matches.push_back(*it);
                        last_added_line_no = it->line_no;
                    }
                }

                // The Match Line
                if (current_line_no > last_added_line_no) {
                    file_res.matches.push_back({current_line_no, current_line, false});
                    last_added_line_no = current_line_no;
                } else if (!file_res.matches.empty() && file_res.matches.back().line_no == current_line_no) {
                    file_res.matches.back().is_context = false;
                }

                // After-Context
                if (options_.context_after > 0) {
                    const char* ctx_ptr = line_end + 1;
                    for (int i = 0; i < options_.context_after && ctx_ptr < chunk_end; ++i) {
                        const char* ce = find_line_end(ctx_ptr, chunk_end);
                        int clno = current_line_no + (i + 1);
                        file_res.matches.push_back({clno, std::string_view(ctx_ptr, ce - ctx_ptr), true});
                        last_added_line_no = clno;
                        ctx_ptr = ce + 1;
                    }
                }
            }
        }

        // Advance to next line
        current_search_ptr = line_end + (line_end < chunk_end ? 1 : 0);
    }

    return file_res;
}

FileResult Scanner::scan(std::string_view content, const std::string& pattern, const std::string& filename) {
    return scan_chunk(content, pattern, filename, 1, 0, content.size());
}

} // namespace Perg