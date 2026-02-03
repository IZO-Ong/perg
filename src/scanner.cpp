#include "perg/colors.hpp"
#include "perg/exceptions.hpp"
#include "perg/scanner.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <regex>

namespace Perg {

int Scanner::scan(std::string_view content, const std::string& pattern) {
    std::regex re;
    try {
        re = std::regex(pattern, std::regex::optimize);
    } catch (const Perg::RegexError& e) {
        throw RegexError(e.what());
    }

    int total_matches = 0;
    
    int padding = (content.size() > 0) ? static_cast<int>(std::log10(content.size())) + 1 : 4;
    if (padding < 4) padding = 4;

    int line_number = 1;
    size_t last_line_break_pos = 0;
    size_t last_newline_search_pos = 0;
    
    auto s_start = content.data();
    auto s_end = content.data() + content.size();
    std::cregex_iterator iter(s_start, s_end, re), end;

    while (iter != end) {
        size_t match_pos = iter->position();

        for (size_t i = last_newline_search_pos; i < match_pos; ++i) {
            if (content[i] == '\n') {
                line_number++;
                last_line_break_pos = i + 1;
            }
        }
        last_newline_search_pos = match_pos;

        size_t line_end = content.find('\n', match_pos);
        if (line_end == std::string_view::npos) line_end = content.size();

        std::string_view line_content = content.substr(last_line_break_pos, line_end - last_line_break_pos);

        if (!options_.count_only) {
            print_line(line_number, line_content, re, padding);
        }

        while (iter != end && (size_t)iter->position() < line_end) {
            total_matches++;
            last_newline_search_pos = iter->position() + iter->length();
            ++iter;
        }

        last_newline_search_pos = line_end;
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