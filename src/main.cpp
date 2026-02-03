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
              << "Generic Options:\n"
              << "  -h, --help            Show this help message\n"
              << "  -c, --count           Only print the total count of pattern occurrences\n"
              << "  -i, --ignore-case     Perform case-insensitive matching\n\n"
              << "Context Control:\n"
              << "  -A <n>                Print <n> lines of trailing context after matches\n"
              << "  -B <n>                Print <n> lines of leading context before matches\n"
              << "  -C <n>                Print <n> lines of output context (Before and After)\n\n"
              << "Output Formatting:\n"
              << "  --color               Use colors to highlight matches (default if terminal)\n"
              << "  --no-color            Disable colors in output\n\n"
              << "Examples:\n"
              << "  perg \"ERROR\" /var/log           Recursive scan of a directory\n"
              << "  perg -i -C 2 \"main\" src/       Case-insensitive with 2 lines of context\n"
              << std::endl;
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
        {"ignore-case", no_argument, 0, 'i'},
        {"before-context", required_argument, 0, 'B'},
        {"after-context", required_argument, 0, 'A'},
        {"context", required_argument, 0, 'C'},
        {"help", no_argument, 0, 'h'},
        {"color", no_argument, 0, 1},
        {"no-color", no_argument, 0, 2},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "chiA:B:C:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c': options.count_only = true; break;
            case 'h': print_help(); return 0;
            case 'i': options.ignore_case = true; break;
            case 'A': options.context_after = std::stoi(optarg); break;
            case 'B': options.context_before = std::stoi(optarg); break;
            case 'C': 
                options.context_before = std::stoi(optarg);
                options.context_after = std::stoi(optarg);
                break;
            case 1: options.use_color = true; break;
            case 2: options.use_color = false; break;
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