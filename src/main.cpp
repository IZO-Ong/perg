#include <iostream>
#include <string>
#include <unistd.h>
#include "perg/mmap_file.hpp"
#include "perg/scanner.hpp" // Our new class

void print_help() {
    std::cout << "perg - A high-performance, zero-copy regex pattern scanner\n\n"
              << "Usage:\n"
              << "  perg <regex_pattern> <file>\n" << std::endl;
}

int main(int argc, char* argv[]) {
    Perg::ScanOptions options;
    options.use_color = isatty(STDOUT_FILENO);

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
        Perg::Scanner scanner(options);

        scanner.scan(file.view(), pattern);

    } catch (const std::regex_error& e) {
        std::cerr << "Regex Error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}