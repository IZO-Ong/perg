#include <iostream>
#include <string>
#include "perg/mmap_file.hpp"

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
            pos = content.find(pattern, pos + pattern.size());
        }

        std::cout << "Found '" << pattern << "' " << count << " times." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}