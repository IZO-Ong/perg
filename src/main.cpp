#include <iostream>
#include <string>
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

        size_t pos = content.find(pattern);
        int count = 0;

        while (pos != std::string_view::npos) {
            count++;

            size_t line_start = content.rfind('\n', pos);
            if (line_start == std::string_view::npos) line_start = 0;
            else line_start++;

            size_t line_end = content.find('\n', pos);
            if (line_end == std::string_view::npos) line_end = content.size();

            std::string_view prefix = content.substr(line_start, pos - line_start);
            size_t suffix_start = pos + pattern.size();
            std::string_view suffix = content.substr(suffix_start, line_end - suffix_start);

            std::cout << prefix 
                    << Perg::Colors::BOLD << Perg::Colors::RED << pattern << Perg::Colors::RESET 
                    << suffix << "\n";

            pos = content.find(pattern, pos + pattern.size());
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}