#include <iostream>
#include <iomanip>
#include <string>
#include <cmath>
#include <regex>
#include <unistd.h>
#include "perg/mmap_file.hpp"
#include "perg/colors.hpp"

void print_help() {
    std::cout << "perg - A high-performance, zero-copy regex pattern scanner\n\n"
              << "Usage:\n"
              << "  perg <regex_pattern> <file>\n" << std::endl;
}

int main(int argc, char* argv[]) {
    bool use_color = isatty(STDOUT_FILENO);

    if (argc < 3) {
        if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
            print_help();
            return 0;
        }
        std::cerr << "Error: Missing arguments.\n";
        print_help();
        return 1;
    }

    std::string pattern = argv[1];
    std::string filename = argv[2];

    try {
        Perg::MmapFile file(filename);
        std::string_view content = file.view();
        std::regex search_regex(pattern, std::regex::optimize);

        int padding = (file.size() > 0) ? static_cast<int>(std::log10(file.size())) + 1 : 4;
        if (padding < 4) padding = 4;

        int line_number = 1;
        size_t current_search_start = 0;
        
        const char* s_start = content.data();
        const char* s_end = content.data() + content.size();
        std::cregex_iterator iter(s_start, s_end, search_regex);
        std::cregex_iterator end;

        while (iter != end) {
            std::cmatch match = *iter;
            size_t pos = match.position();

            for (size_t i = current_search_start; i < pos; ++i) {
                if (content[i] == '\n') line_number++;
            }

            size_t line_start = content.rfind('\n', pos);
            line_start = (line_start == std::string_view::npos) ? 0 : line_start + 1;
            size_t line_end = content.find('\n', pos);
            if (line_end == std::string_view::npos) line_end = content.size();

            std::string_view line_content = content.substr(line_start, line_end - line_start);

            // only use colour if in terminal
            if (use_color) std::cout << Perg::Colors::YELLOW;
            std::cout << std::setw(padding) << std::left << line_number;
            if (use_color) std::cout << Perg::Colors::RESET;
            std::cout << " | ";

            size_t last_printed_pos = 0;
            auto line_ptr = line_content.data();
            std::cregex_iterator line_iter(line_ptr, line_ptr + line_content.size(), search_regex);

            for (; line_iter != end; ++line_iter) {
                std::cmatch line_match = *line_iter;
                size_t line_internal_pos = line_match.position();

                std::cout << line_content.substr(last_printed_pos, line_internal_pos - last_printed_pos);
                
                if (use_color) std::cout << Perg::Colors::BOLD << Perg::Colors::CYAN;
                std::cout << line_match.str();
                if (use_color) std::cout << Perg::Colors::RESET;
                
                last_printed_pos = line_internal_pos + line_match.length();
            }
            std::cout << line_content.substr(last_printed_pos) << "\n";

            current_search_start = line_end;
            while (iter != end && (size_t)iter->position() < line_end) {
                ++iter;
            }
        }
        
    } catch (const std::regex_error& e) {
        std::cerr << "Regex Error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}