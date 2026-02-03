#include "perg/exceptions.hpp"
#include "perg/mmap_file.hpp"
#include "perg/scanner.hpp"

#include <iostream>
#include <getopt.h>
#include <string>
#include <unistd.h>

void print_help() {
    std::cout << "perg - A high-performance, zero-copy regex pattern scanner\n\n"
              << "Usage:\n"
              << "  perg [options] <pattern> <file>\n\n"
              << "Options:\n"
              << "  -c    Only print the total count of pattern occurrences\n"
              << "  -h    Show this help message\n" << std::endl;
}

int main(int argc, char* argv[]) {
    Perg::ScanOptions options;
    options.use_color = isatty(STDOUT_FILENO);

    static struct option long_options[] = {
        {"count", no_argument, 0, 'c'},
        {"help",  no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "ch", long_options, &option_index)) != -1) {
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
    } catch (const Perg::RegexError& e) {
        std::cerr << "Invalid Regex: " << e.what() << std::endl;
        return 1;
    } catch (const Perg::FileError& e) {
        std::cerr << "FileSystem Error: " << e.what() << "\n";
        return 1;
    } 
    catch (const std::exception& e) {
        std::cerr << "Unexpected Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}