#include "perg/colors.hpp"
#include "perg/exceptions.hpp"
#include "perg/scanner.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <regex>
#include <algorithm>

namespace Perg {

    // Helper: Check for null bytes to avoid scanning binary blobs
    bool is_binary(std::string_view content) {
        size_t check_limit = std::min(content.size(), (size_t)1024);
        for (size_t i = 0; i < check_limit; ++i) {
            if (content[i] == '\0') return true;
        }
        return false;
    }

    // Navigation Helper: Find position of newline N steps backward
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

    // Navigation Helper: Find position of newline N steps forward
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

    FileResult Scanner::scan(std::string_view content, const std::string& pattern, const std::string& filename) {
        FileResult file_res;
        file_res.filename = filename;
        if (is_binary(content)) return file_res;

        std::regex_constants::syntax_option_type flags = std::regex::optimize;
        if (options_.ignore_case) flags |= std::regex::icase;
        
        std::regex re;
        try {
            re = std::regex(pattern, flags);
        } catch (const std::regex_error& e) {
            throw RegexError(e.what());
        }

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

            // Increment line counter based on newlines between last match and current
            current_line_no += std::count(content.data() + last_line_start, content.data() + line_start, '\n');
            last_line_start = line_start;

            std::string_view line_view = content.substr(line_start, line_end - line_start);
            
            // Add unique line to result list
            file_res.matches.push_back({current_line_no, std::string(line_view)});

            // Count every occurrence on this line, but skip the iterator to the next line
            while (it != end && (size_t)it->position() < line_end) {
                file_res.total_matches++;
                ++it;
            }

            // Stream output immediately if we are not buffering for the graph
            if (!options_.visualize_graph && !options_.count_only) {
                render_output_group(content, current_line_no, line_start, line_end, last_printed_pos, re, filename);
            }
        }

        if (options_.count_only && !options_.visualize_graph) {
            if (options_.print_filename) std::cout << filename << ":";
            std::cout << file_res.total_matches << "\n";
        }

        return file_res;
    }

    void Scanner::render_output_group(std::string_view content, int match_ln, 
            size_t line_start, size_t line_end, 
            size_t& last_printed_pos, const std::regex& re, 
            const std::string& filename) {
    
        size_t group_start = find_backward_nl(content, line_start, options_.context_before);
        
        // Print context separator "--" if there is a gap between matches
        if (last_printed_pos > 0 && group_start > last_printed_pos) {
            if (options_.context_before > 0 || options_.context_after > 0) {
                if (options_.use_color) std::cout << Colors::CYAN;
                std::cout << "--\n";
                if (options_.use_color) std::cout << Colors::RESET;
            }
        }

        size_t current_pos = std::max(group_start, last_printed_pos);
        int current_ln = match_ln - (int)std::count(content.data() + group_start, content.data() + line_start, '\n');
        
        if (current_pos > group_start) {
            current_ln += (int)std::count(content.data() + group_start, content.data() + current_pos, '\n');
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

    void Scanner::highlight_content(std::string_view line, const std::regex& re) {
        auto& out = std::cout;
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

    void Scanner::print_line(int line_no, std::string_view line, const std::regex& re, 
                        int padding, const std::string& filename, bool is_match) {
        auto& out = std::cout;

        size_t first_content = line.find_first_not_of(" \t\r\n");
        if (first_content != std::string_view::npos) {
            line.remove_prefix(first_content);
        } else {
            line = "";
        }

        if (options_.print_line_numbers) {
            if (options_.use_color) out << Colors::YELLOW;
            out << std::setw(padding) << std::left << line_no;
            if (options_.use_color) out << Colors::RESET;
            
            out << (is_match ? " : " : " - ");
        }

        if (!is_match) {
            out << line;
        } else {
            highlight_content(line, re);
        }

        if (options_.print_filename) {
            out << " | " << (options_.use_color ? Colors::CYAN : "") << filename 
                << (options_.use_color ? Colors::RESET : "");
        }

        out << "\n";
    }
}