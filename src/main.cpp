#include <iostream>
#include <string>
#include <unistd.h>
#include "perg/mmap_file.hpp"
#include "perg/scanner.hpp"

void print_help() {
    std::cout << "perg - A high-performance, zero-copy regex pattern scanner\n\n"
              << "Usage:\n"
              << "  perg [options] <pattern> <file>\n\n"
              << "Options:\n"
              << "  -c    Only print a count of matching lines\n"
              << "  -h    Show this help message\n" << std::endl;
}

int main(int argc, char* argv[]) {
    Perg::ScanOptions options;
    options.use_color = isatty(STDOUT_FILENO);

    int opt;
    while ((opt = getopt(argc, argv, "ch")) != -1) {
        switch (opt) {
            case 'c':
                options.count_only = true;
                break;
            case 'h':
                print_help();
                return 0;
            default:
                print_help();
                return 1;
        }
    }

    if (argc - optind < 2) {
        std::cerr << "Error: Missing pattern or filename.\n";
        print_help();
        return 1;
    }

    std::string pattern = argv[optind];
    std::string filename = argv[optind + 1];

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