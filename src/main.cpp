#include "perg/exceptions.hpp"
#include "perg/mmap_file.hpp"
#include "perg/scanner.hpp"

#include <filesystem>
#include <getopt.h>
#include <iostream>
#include <mutex>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

void print_help() {
    std::cout << "PERG - Pattern Enumeration & Regex Generator\n"
              << "A high-performance, zero-copy regex pattern scanner using memory mapping.\n\n"
              << "Usage: perg [OPTIONS] PATTERN [PATH]\n\n"
              << "Options (Lexicographical):\n"
              << "  -A <n>, --after-context    Print <n> lines of trailing context\n"
              << "  -B <n>, --before-context   Print <n> lines of leading context\n"
              << "  -C <n>, --context          Print <n> lines of output context (Before & After)\n"
              << "  -c,     --count            Only print total match count per file\n"
              << "  -e,     --filter <ext>     Only scan files with specific extension (e.g., .cpp)\n"
              << "  -F,     --with-filename    Force prefixing of filename on output\n"
              << "  -f,     --no-filename      Suppress prefixing of filename on output\n"
              << "  -g,     --graph            Visualize results in a directory tree\n"
              << "  -h,     --help             Show this detailed help message\n"
              << "  -i,     --ignore-case      Case-insensitive matching\n"
              << "  -n,     --line-number      Prefix output with 1-based line numbers\n"
              << "  -N,     --no-line-number   Suppress line numbers\n"
              << "  -r,     --recursive        Recursively scan directories\n"
              << "          --color/no-color   Toggle ANSI color highlighting\n\n"
              << "Examples:\n"
              << "  perg -r \"TODO:\" ./src         Recursive scan for TODOs in src directory\n"
              << "  perg -g \"Scanner\" include     Visualize where 'Scanner' appears in headers\n"
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
                if (!options.file_filter.empty() && entry.path().extension() != options.file_filter) continue;
                
                std::error_code ec;
                if (fs::file_size(entry.path(), ec) == 0 || ec) continue;

                Perg::MmapFile file(entry.path().string());
                scanner.scan(file.view(), pattern, entry.path().string());
            }
        }
    } else if (fs::is_regular_file(path)) {
        Perg::MmapFile file(path.string());
        scanner.scan(file.view(), pattern, path.string());
    }
}

void process_path_and_collect(const fs::path& path, const std::string& pattern, 
                             const Perg::ScanOptions& options, Perg::Scanner& scanner, 
                             std::vector<Perg::FileResult>& all_results) {
    if (fs::is_directory(path)) {
        if (!options.recursive) {
            throw Perg::FileError("perg: " + path.string() + ": Is a directory (use -r to recurse)");
        }
        for (const auto& entry : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied)) {
            if (fs::is_regular_file(entry.path())) {
                if (!options.file_filter.empty() && entry.path().extension() != options.file_filter) continue;
                
                std::error_code ec;
                if (fs::file_size(entry.path(), ec) == 0 || ec) continue;

                Perg::MmapFile file(entry.path().string());
                auto res = scanner.scan(file.view(), pattern, entry.path().string());
                if (!res.matches.empty()) all_results.push_back(std::move(res));
            }
        }
    } else if (fs::is_regular_file(path)) {
        Perg::MmapFile file(path.string());
        auto res = scanner.scan(file.view(), pattern, path.string());
        if (!res.matches.empty()) all_results.push_back(std::move(res));
    }
}

int main(int argc, char* argv[]) {
    Perg::ScanOptions options;
    bool stdout_is_tty = isatty(STDOUT_FILENO);
    options.print_line_numbers = stdout_is_tty;
    options.use_color = stdout_is_tty;

    static struct option long_options[] = {
        {"after-context",    required_argument, 0, 'A'},
        {"before-context",   required_argument, 0, 'B'},
        {"context",          required_argument, 0, 'C'},
        {"count",            no_argument,       0, 'c'},
        {"filter",           required_argument, 0, 'e'},
        {"no-filename",      no_argument,       0, 'f'},
        {"with-filename",    no_argument,       0, 'F'},
        {"graph",            no_argument,       0, 'g'},
        {"help",             no_argument,       0, 'h'},
        {"ignore-case",      no_argument,       0, 'i'},
        {"line-number",      no_argument,       0, 'n'},
        {"no-line-number",   no_argument,       0, 'N'},
        {"recursive",        no_argument,       0, 'r'},
        {"color",            no_argument,       0, 1},
        {"no-color",         no_argument,       0, 2},
        {0, 0, 0, 0}
    };

    int opt;
    // Alphabetical order for the short-string where possible: "A:B:C:ce:fFghinNr"
    while ((opt = getopt_long(argc, argv, "A:B:C:ce:fFghinNr", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'A': options.context_after = std::stoi(optarg); break;
            case 'B': options.context_before = std::stoi(optarg); break;
            case 'C': options.context_before = options.context_after = std::stoi(optarg); break;
            case 'c': options.count_only = true; break;
            case 'e': options.file_filter = optarg; break;
            case 'f': options.print_filename = false; break;
            case 'F': options.print_filename = true; break;
            case 'g': options.visualize_graph = true; break;
            case 'h': print_help(); return 0;
            case 'i': options.ignore_case = true; break;
            case 'n': options.print_line_numbers = true; break;
            case 'N': options.print_line_numbers = false; break;
            case 'r': options.recursive = true; break;
            case 1:   options.use_color = true; break;
            case 2:   options.use_color = false; break;
            default:  return 1;
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
        if (!options.print_filename) options.print_filename = true;
    }

    try {
        Perg::Scanner scanner(options);

        if (options.visualize_graph) {
            std::vector<Perg::FileResult> all_results;
            process_path_and_collect(target_path, pattern, options, scanner, all_results);
            scanner.print_tree_graph(all_results, pattern);
        } else {
            process_path(target_path, pattern, options, scanner);
        }
    } catch (const Perg::RegexError& e) {
        std::cerr << "Regex Error: " << e.what() << "\n";
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