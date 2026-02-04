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
    std::cout << "PERG - Pattern Enumeration & Regex Generator\n"
              << "A high-performance, zero-copy regex pattern scanner using memory mapping.\n\n"
              << "Usage: perg [OPTIONS] PATTERN [PATH]\n\n"
              << "Search Options:\n"
              << "  -i, --ignore-case         Case-insensitive matching\n"
              << "  -r, --recursive           Recursively scan directories\n"
              << "  -f, --filter <ext>        Only scan files with specific extension (e.g., .cpp)\n\n"
              << "Context Control:\n"
              << "  -A <n>                    Print <n> lines of trailing context\n"
              << "  -B <n>                    Print <n> lines of leading context\n"
              << "  -C <n>                    Print <n> lines of output context (Before & After)\n\n"
              << "Output Formatting:\n"
              << "  -c, --count               Only print total match count per file\n"
              << "  -n, --line-number         Prefix output with 1-based line numbers\n"
              << "  -H, --with-filename       Force prefixing of filename on output\n"
              << "  -h, --no-filename         Suppress prefixing of filename on output\n"
              << "  --color / --no-color      Toggle ANSI color highlighting\n"
              << "  --help                    Show this detailed help message\n\n"
              << "Examples:\n"
              << "  perg -r \"TODO:\" ./src         Recursive scan for TODOs in src directory\n"
              << "  perg -n -i -f \".log\" \"err\"  Line numbered, case-insensitive log scan\n"
              << std::endl;
}

void process_path(const fs::path& path, const std::string& pattern, 
                  const Perg::ScanOptions& options, Perg::Scanner& scanner) {

    if (fs::is_directory(path)) {
        if (!options.recursive) {
            throw Perg::FileError("perg: " + path.string() + ": Is a directory (use -r to recurse)");
        }

        for (const auto& entry : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied)) {
            if (fs::is_regular_file(entry.path())) {
                if (!options.file_filter.empty() && entry.path().extension() != options.file_filter) {
                    continue;
                }

                std::error_code ec;
                auto size = fs::file_size(entry.path(), ec);
                if (ec || size == 0) continue; 

                Perg::MmapFile file(entry.path().string());
                scanner.scan(file.view(), pattern, entry.path().string());
            }
        }
    } else if (fs::is_regular_file(path)) {
        if (fs::file_size(path) > 0) {
            Perg::MmapFile file(path.string());
            scanner.scan(file.view(), pattern, path.string());
        }
    }
}

int main(int argc, char* argv[]) {
    Perg::ScanOptions options;
    bool stdout_is_tty = isatty(STDOUT_FILENO);
    options.print_line_numbers = stdout_is_tty;
    options.use_color = stdout_is_tty;

    static struct option long_options[] = {
        {"line-number", no_argument, 0, 'n'},
        {"no-line-number", no_argument, 0, 'N'},
        {"count", no_argument, 0, 'c'},
        {"ignore-case", no_argument, 0, 'i'},
        {"recursive", no_argument, 0, 'r'},
        {"filter", required_argument, 0, 'f'},
        {"before-context", required_argument, 0, 'B'},
        {"after-context", required_argument, 0, 'A'},
        {"context", required_argument, 0, 'C'},
        {"with-filename", no_argument, 0, 'H'},
        {"no-filename", no_argument, 0, 'h'},
        {"help", no_argument, 0, 10},
        {"color", no_argument, 0, 1},
        {"no-color", no_argument, 0, 2},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "ciA:B:C:nNrf:Hh", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'r': options.recursive = true; break;
            case 'f': options.file_filter = optarg; break;
            case 'n': options.print_line_numbers = true; break;
            case 'N': options.print_line_numbers = false; break;
            case 'c': options.count_only = true; break;
            case 'i': options.ignore_case = true; break;
            case 'H': options.print_filename = true; break;
            case 'h': options.print_filename = false; break;
            case 'A': options.context_after = std::stoi(optarg); break;
            case 'B': options.context_before = std::stoi(optarg); break;
            case 'C': 
                options.context_before = options.context_after = std::stoi(optarg);
                break;
            case 1: options.use_color = true; break;
            case 2: options.use_color = false; break;
            case 10: print_help(); return 0;
            default: return 1;
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

    if (stdout_is_tty && (options.recursive || fs::is_directory(target_path))) {
        options.print_filename = true;
    }

    try {
        Perg::Scanner scanner(options);
        process_path(target_path, pattern, options, scanner);
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