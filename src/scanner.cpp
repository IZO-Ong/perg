#include "perg/colors.hpp"
#include "perg/exceptions.hpp"
#include "perg/scanner.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <regex>
#include <algorithm>

namespace Perg {

    bool is_literal(const std::string& p) {
        return p.find_first_of(".+*?^$()[]{}|\\") == std::string::npos;
    }

    bool is_binary(std::string_view content) {
        size_t check_limit = std::min(content.size(), (size_t)1024);
        for (size_t i = 0; i < check_limit; ++i) {
            if (content[i] == '\0') return true;
        }
        return false;
    }

    size_t find_backward_nl(std::string_view content, size_t pos, int n) {
        if (n <= 0 || pos == 0) return pos;
        size_t current = pos - 1;
        int found = 0;
        while (current > 0) {
            if (content[current] == '\n') {
                found++;
                if (found > n) return current + 1;
            }
            current--;
        }
        return 0;
    }

    size_t find_forward_nl(std::string_view content, size_t pos, int n) {
        if (n <= 0 || pos >= content.size()) return pos;
        size_t current = pos;
        int found = 0;
        while (current < content.size()) {
            if (content[current] == '\n') {
                found++;
                if (found >= n) return current;
            }
            current++;
        }
        return content.size();
    }

    int Scanner::scan(std::string_view content, const std::string& pattern, const std::string& filename) {
        if (is_binary(content)) return 0;

        int total_matches = 0;
        int line_number = 1;
        size_t last_line_start_pos = 0;
        size_t last_newline_count_pos = 0;
        size_t last_printed_pos = 0; 

        int padding = (content.size() > 0) ? static_cast<int>(std::log10(content.size())) + 1 : 4;
        if (padding < 4) padding = 4;

        std::regex_constants::syntax_option_type flags = std::regex::optimize;
        if (options_.ignore_case) flags |= std::regex::icase;

        std::regex re;
        try {
            re = std::regex(pattern, flags);
        } catch (const std::regex_error& e) {
            throw RegexError(e.what());
        }

        auto update_line_info = [&](size_t up_to_pos) {
            const char* start_ptr = content.data() + last_newline_count_pos;
            const char* end_ptr = content.data() + up_to_pos;
            while (start_ptr < end_ptr) {
                const char* next_nl = static_cast<const char*>(std::memchr(start_ptr, '\n', end_ptr - start_ptr));
                if (!next_nl) break;
                line_number++;
                last_line_start_pos = (next_nl - content.data()) + 1;
                start_ptr = next_nl + 1;
            }
            last_newline_count_pos = up_to_pos;
        };

        auto handle_output = [&](int match_ln, size_t match_line_start) {
            if (options_.count_only) return;

            size_t context_start = find_backward_nl(content, match_line_start, options_.context_before);

            if (last_printed_pos > 0 && context_start > last_printed_pos) {
                if (options_.context_before > 0 || options_.context_after > 0) {
                    if (options_.use_color) std::cout << Colors::CYAN;
                    std::cout << "--";
                    if (options_.use_color) std::cout << Colors::RESET;
                    std::cout << "\n";
                }
            }

            size_t current_pos = std::max(context_start, last_printed_pos);
            
            // 3. Anchor the current line number to the match line
            int current_ln = match_ln - (int)std::count(content.data() + context_start, 
                                                    content.data() + match_line_start, '\n');
            
            // Adjust line number if we skipped already-printed lines
            if (current_pos > context_start) {
                current_ln += (int)std::count(content.data() + context_start, 
                                            content.data() + current_pos, '\n');
            }

            // 4. Calculate trailing context end
            size_t match_eol = content.find('\n', match_line_start);
            if (match_eol == std::string_view::npos) match_eol = content.size();
            
            // Find the boundary after the Nth context line
            size_t context_end = find_forward_nl(content, match_eol, options_.context_after + 1);

            while (current_pos < context_end) {
                size_t next_nl = content.find('\n', current_pos);
                if (next_nl == std::string_view::npos) next_nl = content.size();
                
                std::string_view current_view = content.substr(current_pos, next_nl - current_pos);
                bool is_match = (current_ln == match_ln);
                
                print_line(current_ln, current_view, re, padding, filename, is_match);
                
                current_pos = next_nl + 1;
                current_ln++;
                if (next_nl == content.size()) break;
            }
            last_printed_pos = current_pos;
        };

        bool use_literal_path = is_literal(pattern) && !options_.ignore_case;

        if (use_literal_path) {
            size_t pos = 0;
            while ((pos = content.find(pattern, pos)) != std::string_view::npos) {
                update_line_info(pos);
                size_t line_end = content.find('\n', pos);
                if (line_end == std::string_view::npos) line_end = content.size();

                handle_output(line_number, last_line_start_pos);

                size_t internal_pos = 0;
                std::string_view line_content = content.substr(last_line_start_pos, line_end - last_line_start_pos);
                while ((internal_pos = line_content.find(pattern, internal_pos)) != std::string_view::npos) {
                    total_matches++;
                    internal_pos += pattern.length();
                }
                pos = line_end;
            }
        } else {
            auto s_start = content.data();
            auto s_end = content.data() + content.size();
            std::cregex_iterator iter(s_start, s_end, re), end;

            while (iter != end) {
                size_t match_pos = static_cast<size_t>(iter->position());
                update_line_info(match_pos);
                size_t line_end = content.find('\n', match_pos);
                if (line_end == std::string_view::npos) line_end = content.size();

                handle_output(line_number, last_line_start_pos);

                while (iter != end && static_cast<size_t>(iter->position()) < line_end) {
                    total_matches++;
                    ++iter;
                }
                last_newline_count_pos = line_end;
            }
        }

        if (options_.count_only && !options_.print_filename) std::cout << total_matches << "\n";
        return total_matches;
    }

    void Scanner::print_line(int line_no, std::string_view line, const std::regex& re, int padding, const std::string& filename, bool is_match) {
        if (options_.print_filename) {
            if (options_.use_color) std::cout << Colors::CYAN;
            std::cout << filename;
            if (options_.use_color) std::cout << Colors::RESET;
            std::cout << (is_match ? ":" : "-");
        }

        if (options_.use_color) std::cout << Colors::YELLOW;
        std::cout << std::setw(padding) << std::left << line_no;
        if (options_.use_color) std::cout << Colors::RESET;
        
        // Grep standard: ':' for match, '-' for context, followed by exactly one space
        std::cout << (is_match ? ": " : "- ");

        if (!is_match) {
            std::cout << line << "\n";
            return;
        }

        // Highlight matching substrings within the line
        size_t last_pos = 0;
        std::cregex_iterator it(line.data(), line.data() + line.size(), re), end;
        for (; it != end; ++it) {
            std::cout << line.substr(last_pos, it->position() - last_pos);
            if (options_.use_color) std::cout << Colors::BOLD << Colors::CYAN;
            std::cout << it->str();
            if (options_.use_color) std::cout << Colors::RESET;
            last_pos = it->position() + it->length();
        }
        std::cout << line.substr(last_pos) << "\n";
    }

}