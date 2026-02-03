#include "perg/colors.hpp"
#include "perg/exceptions.hpp"
#include "perg/scanner.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <regex>

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

    int Scanner::scan(std::string_view content, const std::string& pattern, const std::string& filename) {
        if (is_binary(content)) return 0;

        int total_matches = 0;
        int line_number = 1;
        size_t last_line_start_pos = 0;
        size_t last_newline_count_pos = 0;

        int padding = (content.size() > 0) ? static_cast<int>(std::log10(content.size())) + 1 : 4;
        if (padding < 4) padding = 4;

        std::regex re;
        try {
            re = std::regex(pattern, std::regex::optimize);
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

        if (is_literal(pattern)) {
            size_t pos = 0;
            while ((pos = content.find(pattern, pos)) != std::string_view::npos) {
                update_line_info(pos);
                size_t line_end = content.find('\n', pos);
                if (line_end == std::string_view::npos) line_end = content.size();

                std::string_view line_content = content.substr(last_line_start_pos, line_end - last_line_start_pos);

                if (!options_.count_only) {
                    print_line(line_number, line_content, re, padding, filename);
                }

                size_t internal_pos = 0;
                while ((internal_pos = line_content.find(pattern, internal_pos)) != std::string_view::npos) {
                    total_matches++;
                    internal_pos += pattern.length();
                }
                pos = line_end;
                last_newline_count_pos = line_end; 
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

                if (!options_.count_only) {
                    std::string_view line_content = content.substr(last_line_start_pos, line_end - last_line_start_pos);
                    print_line(line_number, line_content, re, padding, filename);
                }

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

    void Scanner::print_line(int line_no, std::string_view line, const std::regex& re, int padding, const std::string& filename) {
        // conditional filename
        if (options_.print_filename) {
            if (options_.use_color) std::cout << Colors::CYAN;
            std::cout << filename << ":";
            if (options_.use_color) std::cout << Colors::RESET;
        }

        if (options_.use_color) std::cout << Colors::YELLOW;
        std::cout << std::setw(padding) << std::left << line_no;
        if (options_.use_color) std::cout << Colors::RESET;
        std::cout << " | ";

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