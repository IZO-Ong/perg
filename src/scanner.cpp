#include "perg/scanner.hpp"
#include "perg/colors.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

namespace Perg {
    int Scanner::scan(std::string_view content, const std::string& pattern) {
        std::regex re(pattern, std::regex::optimize);
        int total_matches = 0;

        int padding = (content.size() > 0) ? static_cast<int>(std::log10(content.size())) + 1 : 4;
        if (padding < 4) padding = 4;

        int line_number = 1;
        size_t current_search_start = 0;
        
        auto s_start = content.data();
        auto s_end = content.data() + content.size();
        std::cregex_iterator iter(s_start, s_end, re), end;

        while (iter != end) {
            if (options_.count_only) {
                total_matches++;
                size_t line_end = content.find('\n', iter->position());
                if (line_end == std::string_view::npos) break;

                while (iter != end && (size_t)iter->position() < line_end) ++iter;
            } else {
                size_t pos = iter->position();

                for (size_t i = current_search_start; i < pos; ++i) {
                    if (content[i] == '\n') line_number++;
                }

                size_t line_start = content.rfind('\n', pos);
                line_start = (line_start == std::string_view::npos) ? 0 : line_start + 1;
                size_t line_end = content.find('\n', pos);
                if (line_end == std::string_view::npos) line_end = content.size();

                print_line(line_number, content.substr(line_start, line_end - line_start), re, padding);

                current_search_start = line_end;
                while (iter != end && (size_t)iter->position() < line_end) ++iter;
            }
        }

        if (options_.count_only) {
            std::cout << total_matches << "\n";
        }

        return total_matches;
    }

    void Scanner::print_line(int line_no, std::string_view line, const std::regex& re, int padding) {
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