#include <iostream>
#include <iomanip>
#include <string>
#include <cmath>
#include "perg/mmap_file.hpp"
#include "perg/colors.hpp"

void print_help() {
    std::cout << "perg - A high-performance, zero-copy pattern scanner\n\n"
              << "Usage:\n"
              << "  perg <pattern> <file>\n"
              << "  perg [options]\n\n"
              << "Options:\n"
              << "  -h, --help     Show this help message\n\n"
              << "Example:\n"
              << "  perg \"error\" system.log\n" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    std::string first_arg = argv[1];
    if (first_arg == "-h" || first_arg == "--help") {
        print_help();
        return 0;
    }

    if (argc < 3) {
        std::cerr << "Error: Missing file argument.\n";
        print_help();
        return 1;
    }

    std::string pattern = argv[1];
    std::string filename = argv[2];

    try {
        Perg::MmapFile file(filename);
        std::string_view content = file.view();

        int padding = (file.size() > 0) ? static_cast<int>(std::log10(file.size())) + 1 : 1;
        if (padding < 4) padding = 4;

        size_t pos = content.find(pattern);
        int match_count = 0;
        int line_number = 1;
        size_t last_line_start = std::string_view::npos;
        size_t current_search_start = 0;

        while (pos != std::string_view::npos) {
            match_count++;

            // calculate line number
            for (size_t i = current_search_start; i < pos; ++i) {
                if (content[i] == '\n') line_number++;
            }
            current_search_start = pos;

            // line boundaries
            size_t line_start = content.rfind('\n', pos);
            if (line_start == std::string_view::npos) line_start = 0;
            else line_start++;

            size_t line_end = content.find('\n', pos);
            if (line_end == std::string_view::npos) line_end = content.size();

            // duplicate line handling & Aligned Printing
            if (line_start != last_line_start) {
                // print aligned Line Number
                std::cout << Perg::Colors::YELLOW 
                          << std::setw(padding) << std::left << line_number 
                          << Perg::Colors::RESET << " | ";
                
                std::string_view prefix = content.substr(line_start, pos - line_start);
                size_t suffix_start = pos + pattern.size();
                std::string_view suffix = content.substr(suffix_start, line_end - suffix_start);

                std::cout << prefix 
                          << Perg::Colors::BOLD << Perg::Colors::CYAN << pattern << Perg::Colors::RESET 
                          << suffix << "\n";
                
                last_line_start = line_start;
            }
            
            pos = content.find(pattern, pos + pattern.size());
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}