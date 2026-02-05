#include "perg/scanner.hpp"
#include <algorithm>
#include <regex>
#include <cstring>

namespace Perg {

    // Helper: Optimized newline counting using SIMD-accelerated memchr
    inline int count_newlines_fast(const char* start, const char* end) {
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

    bool is_binary(std::string_view content) {
        size_t check_limit = std::min(content.size(), (size_t)1024);
        if (check_limit == 0) return false;
        // check using memchr to find null bytes
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

    FileResult Scanner::scan_chunk(std::string_view full_content, const std::string& pattern, 
                                   const std::string& filename, int start_line, 
                                   size_t range_start, size_t range_end) {
        if (range_end == std::string_view::npos) range_end = full_content.size();
        
        FileResult file_res;
        file_res.filename = filename;
        
        std::string_view search_view = full_content.substr(range_start, range_end - range_start);
        if (is_binary(search_view)) return file_res;

        file_res.matches.reserve(128);

        size_t last_line_count_pos = 0; 
        int current_line_no = start_line;
        bool use_fast_path = is_literal(pattern) && !options_.ignore_case;

        // Optimization: Pre-compile Regex once per chunk instead of inside the match loop
        std::regex re;
        if (!use_fast_path) {
            std::regex_constants::syntax_option_type flags = std::regex::ECMAScript | std::regex::optimize;
            if (options_.ignore_case) flags |= std::regex::icase;
            re = std::regex(pattern, flags);
        }

        auto add_match_context = [&](size_t pos_in_chunk) {
            // Optimization: Incremental line counting using SIMD fast-path
            current_line_no += count_newlines_fast(search_view.data() + last_line_count_pos, 
                                                   search_view.data() + pos_in_chunk);
            last_line_count_pos = pos_in_chunk;

            size_t global_pos = range_start + pos_in_chunk;
            size_t line_start = full_content.find_last_of('\n', global_pos);
            line_start = (line_start == std::string_view::npos) ? 0 : line_start + 1;

            size_t line_end = full_content.find('\n', global_pos);
            if (line_end == std::string_view::npos) line_end = full_content.size();

            // 1. Before Context
            for (int i = options_.context_before; i > 0; --i) {
                size_t c_start = find_backward_nl(full_content, line_start > 0 ? line_start - 1 : 0, i);
                size_t c_end = full_content.find('\n', c_start);
                if (c_end != std::string_view::npos && c_end < line_start) {
                    file_res.matches.push_back({current_line_no - i, full_content.substr(c_start, c_end - c_start), true});
                }
            }

            // 2. The Match
            file_res.matches.push_back({current_line_no, full_content.substr(line_start, line_end - line_start), false});

            // 3. After Context
            size_t next_line = line_end + 1;
            for (int i = 1; i <= options_.context_after; ++i) {
                if (next_line >= full_content.size()) break;
                size_t c_end = full_content.find('\n', next_line);
                if (c_end == std::string_view::npos) c_end = full_content.size();
                file_res.matches.push_back({current_line_no + i, full_content.substr(next_line, c_end - next_line), true});
                next_line = c_end + 1;
            }

            return line_end - range_start;
        };

        if (use_fast_path) {
            // Optimization: Literal search uses compiler-optimized memmem/memchr
            size_t pos = search_view.find(pattern, 0);
            while (pos != std::string_view::npos) {
                size_t line_end_in_chunk = add_match_context(pos);
                
                // Count all occurrences on the current line efficiently
                size_t sub_pos = pos;
                size_t line_end_global = range_start + line_end_in_chunk;
                while (sub_pos != std::string_view::npos && (range_start + sub_pos) < line_end_global) {
                    file_res.total_matches++;
                    sub_pos = search_view.find(pattern, sub_pos + pattern.length());
                }
                pos = search_view.find(pattern, line_end_in_chunk);
            }
        } else {
            auto it = std::cregex_iterator(search_view.begin(), search_view.end(), re);
            auto end = std::cregex_iterator();
            while (it != end) {
                size_t line_end_in_chunk = add_match_context(it->position());
                file_res.total_matches++;
                size_t next_it_pos = it->position();
                while (it != end && (size_t)it->position() < line_end_in_chunk) {
                    if ((size_t)it->position() > next_it_pos) file_res.total_matches++;
                    ++it;
                }
            }
        }

        if (file_res.matches.size() > 1) {
            std::sort(file_res.matches.begin(), file_res.matches.end(), [](const auto& a, const auto& b) {
                if (a.line_no != b.line_no) return a.line_no < b.line_no;
                return a.is_context > b.is_context; 
            });
            file_res.matches.erase(std::unique(file_res.matches.begin(), file_res.matches.end(), [](const auto& a, const auto& b) {
                return a.line_no == b.line_no;
            }), file_res.matches.end());
        }

        return file_res;
    }

    FileResult Scanner::scan(std::string_view content, const std::string& pattern, const std::string& filename) {
        return scan_chunk(content, pattern, filename, 1, 0, content.size());
    }
}