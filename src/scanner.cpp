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

    void Scanner::render_output_group(std::string_view content, int match_ln, 
            size_t line_start, size_t line_end, 
            size_t& last_printed_pos, const std::regex& re, 
            const std::string& filename) {
    
        size_t group_start = find_backward_nl(content, line_start, options_.context_before);
        
        if (last_printed_pos > 0 && group_start > last_printed_pos) {
            if (options_.context_before > 0 || options_.context_after > 0) {
                if (options_.use_color) std::cout << Colors::CYAN;
                std::cout << "--\n";
                if (options_.use_color) std::cout << Colors::RESET;
            }
        }

        size_t current_pos = std::max(group_start, last_printed_pos);
        
        int current_ln = match_ln - (int)std::count(content.data() + group_start, 
                                                content.data() + line_start, '\n');
        if (current_pos > group_start) {
            current_ln += (int)std::count(content.data() + group_start, 
                                        content.data() + current_pos, '\n');
        }

        size_t group_end = find_forward_nl(content, line_end, options_.context_after + 1);

        while (current_pos < group_end) {
            size_t next_nl = content.find('\n', current_pos);
            if (next_nl == std::string_view::npos) next_nl = content.size();
            
            std::string_view line_view = content.substr(current_pos, next_nl - current_pos);
            bool is_match_line = (current_ln == match_ln);
            
            print_line(current_ln, line_view, re, 4, filename, is_match_line);
            
            current_pos = next_nl + 1;
            current_ln++;
            if (next_nl == content.size()) break;
        }

        last_printed_pos = current_pos;
    }

    int Scanner::scan(std::string_view content, const std::string& pattern, const std::string& filename) {
        if (is_binary(content)) return 0;

        std::regex_constants::syntax_option_type flags = std::regex::optimize;
        if (options_.ignore_case) flags |= std::regex::icase;
        std::regex re(pattern, flags);

        int total_matches = 0;
        int current_line_no = 1;
        size_t last_line_start = 0;
        size_t last_printed_pos = 0;

        auto it = std::cregex_iterator(content.data(), content.data() + content.size(), re);
        auto end = std::cregex_iterator();

        while (it != end) {
            size_t match_pos = it->position();
            

            size_t line_start = content.find_last_of('\n', match_pos);
            line_start = (line_start == std::string_view::npos) ? 0 : line_start + 1;
            
            size_t line_end = content.find('\n', match_pos);
            if (line_end == std::string_view::npos) line_end = content.size();

            current_line_no += std::count(content.data() + last_line_start, 
                                        content.data() + line_start, '\n');
            last_line_start = line_start;

            if (!options_.count_only) {
                render_output_group(content, current_line_no, line_start, line_end, 
                                last_printed_pos, re, filename);
            }

            while (it != end && (size_t)it->position() < line_end) {
                total_matches++;
                ++it;
            }
        }

        if (options_.count_only) {
            if (options_.print_filename) std::cout << filename << ":";
            std::cout << total_matches << "\n";
        }

        return total_matches;
    }

    void Scanner::print_line(int line_no, std::string_view line, const std::regex& re, 
            int padding, const std::string& filename, bool is_match) {
        auto& out = std::cout;

        if (options_.print_line_numbers) {
            if (options_.use_color) out << Colors::YELLOW;
            out << std::setw(padding) << std::left << line_no;
            if (options_.use_color) out << Colors::RESET;
            
            out << (is_match ? " : " : " - ");
        }

        if (!is_match) {
            out << line;
        } else {
            size_t last_pos = 0;
            std::cregex_iterator it(line.data(), line.data() + line.size(), re), end;
            for (; it != end; ++it) {
                out << line.substr(last_pos, it->position() - last_pos);
                if (options_.use_color) out << Colors::BOLD << Colors::CYAN;
                out << it->str();
                if (options_.use_color) out << Colors::RESET;
                last_pos = it->position() + it->length();
            }
            out << line.substr(last_pos);
        }

        if (options_.print_filename) {
            out << " | ";
            if (options_.use_color) out << Colors::CYAN;
            out << filename;
            if (options_.use_color) out << Colors::RESET;
        }

        out << "\n";
    }
}