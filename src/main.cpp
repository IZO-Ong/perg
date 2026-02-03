#include "perg/exceptions.hpp"
#include "perg/mmap_file.hpp"
#include "perg/scanner.hpp"

#include <filesystem>
#include <getopt.h>
#include <iostream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

void print_help() {
    std::cout << "perg - A high-performance, zero-copy regex pattern scanner\n\n"
              << "Usage:\n"
              << "  perg [options] <pattern> <file_or_dir>\n\n"
              << "Options:\n"
              << "  -c    Only print the total count of pattern occurrences\n"
              << "  -h    Show this help message\n" << std::endl;
}

void process_path(const fs::path& path, const std::string& pattern, Perg::Scanner& scanner) {
    try {
        if (fs::is_directory(path)) {
            for (const auto& entry : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied)) {
                if (fs::is_regular_file(entry.path())) {
                    Perg::MmapFile file(entry.path().string());
                    scanner.scan(file.view(), pattern, entry.path().string());
                }
            }
        } else if (fs::is_regular_file(path)) {
            Perg::MmapFile file(path.string());
            scanner.scan(file.view(), pattern, path.string());
        }
    } catch (const Perg::PergException& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
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
            case 'c': options.count_only = true; break;
            case 'h': print_help(); return 0;
            default:  print_help(); return 1;
        }
    }

    if (argc - optind < 2) {
        std::cerr << "Error: Missing pattern or path.\n";
        print_help();
        return 1;
    }

    std::string pattern = argv[optind];
    fs::path target_path = argv[optind + 1];

    if (!fs::exists(target_path)) {
        std::cerr << "FileSystem Error: Path does not exist: " << target_path << "\n";
        return 1; 
    }

    if (fs::is_directory(target_path)) {
        options.print_filename = true;
    }

    try {
        Perg::Scanner scanner(options);
        process_path(target_path, pattern, scanner);
    } catch (const Perg::RegexError& e) {
        std::cerr << e.what() << "\n";
        return 1;
    } catch (const Perg::FileError& e) {
        std::cerr << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}