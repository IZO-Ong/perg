#include "perg/exceptions.hpp"
#include "perg/mmap_file.hpp"
#include "perg/scanner.hpp"
#include "perg/search_engine.hpp"
#include "perg/tree_renderer.hpp"
#include "perg/colors.hpp"

#include <filesystem>
#include <getopt.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <unistd.h>
#include <vector>
#include <future>
#include <mutex>
#include <algorithm>
#include <regex>

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
              << "  -f,     --with-filename    Force prefixing of filename on output\n"
              << "  -F,     --no-filename      Suppress prefixing of filename on output\n"
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

void print_results(std::vector<Perg::FileResult>& results, const Perg::ScanOptions& options, const std::string& pattern) {
    if (results.empty()) return;

    std::map<std::string, Perg::FileResult> merged_map;
    for (auto& res : results) {
        auto& entry = merged_map[res.filename];
        entry.filename = res.filename;
        entry.total_matches += res.total_matches;
        entry.matches.insert(entry.matches.end(), 
                             std::make_move_iterator(res.matches.begin()), 
                             std::make_move_iterator(res.matches.end()));
    }

    std::regex re(pattern, options.ignore_case ? std::regex::icase : std::regex::ECMAScript);

    for (auto& [filename, res] : merged_map) {
        
        if (options.count_only) {
            if (options.print_filename) {
                if (options.use_color) std::cout << Perg::Colors::MAGENTA << filename << Perg::Colors::RESET << ":";
                else std::cout << filename << ":";
            }
            std::cout << res.total_matches << "\n";
            continue;
        }

        std::sort(res.matches.begin(), res.matches.end(), [](const auto& a, const auto& b) {
            return a.line_no < b.line_no;
        });

        int last_line_no = -1;
        for (const auto& match : res.matches) {
            // Context Separator "--"
            if (last_line_no != -1 && match.line_no > last_line_no + 1) {
                if (options.context_before > 0 || options.context_after > 0) {
                    if (options.use_color) std::cout << Perg::Colors::CYAN << "--" << Perg::Colors::RESET << "\n";
                    else std::cout << "--\n";
                }
            }

            if (options.print_line_numbers) {
                if (options.use_color) std::cout << Perg::Colors::YELLOW;
                std::cout << std::left << std::setw(4) << match.line_no;
                if (options.use_color) std::cout << Perg::Colors::RESET;
                std::cout << (match.is_context ? " - " : " : ");
            }

            if (!match.is_context && options.use_color) {
                std::string content_str(match.content);
                std::cout << std::regex_replace(content_str, re, 
                             std::string(Perg::Colors::BOLD) + std::string(Perg::Colors::CYAN) + "$&" + std::string(Perg::Colors::RESET));
            } else {
                std::cout << match.content;
            }

            if (options.print_filename) {
                std::cout << " | ";
                if (options.use_color) std::cout << Perg::Colors::MAGENTA << filename << Perg::Colors::RESET;
                else std::cout << filename;
            }
            
            std::cout << "\n";
            last_line_no = match.line_no;
        }
    }
}

int main(int argc, char* argv[]) {
    Perg::ScanOptions options;
    bool stdout_is_tty = isatty(STDOUT_FILENO);
    options.use_color = stdout_is_tty;
    options.print_line_numbers = stdout_is_tty;

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
        {"recursive",        no_argument,       0, 'r'},
        {"color",            no_argument,       0, 1},
        {"no-color",         no_argument,       0, 2},
        {0, 0, 0, 0}
    };

    int opt;
    bool color_explicitly_set = false;
    while ((opt = getopt_long(argc, argv, "A:B:C:ce:fFghinr", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'A': options.context_after = std::stoi(optarg); break;
            case 'B': options.context_before = std::stoi(optarg); break;
            case 'C': options.context_before = options.context_after = std::stoi(optarg); break;
            case 'c': options.count_only = true; break;
            case 'e': options.file_filter = optarg; break;
            case 'f': options.print_filename = true; break;
            case 'F': options.print_filename = false; break;
            case 'g': options.visualize_graph = true; break;
            case 'h': print_help(); return 0;
            case 'i': options.ignore_case = true; break;
            case 'n': options.print_line_numbers = true; break;
            case 'r': options.recursive = true; break;
            case 1:   options.use_color = true; color_explicitly_set = true; break;
            case 2:   options.use_color = false; color_explicitly_set = true; break;
        }
    }

    // disable color if output is piped, unless specifically requested
    if (!stdout_is_tty && !color_explicitly_set) options.use_color = false;

    if (argc - optind < 1) return 1;
    std::string pattern = argv[optind];
    fs::path target_path = (optind + 1 < argc) ? argv[optind + 1] : ".";

    try {
        Perg::Scanner scanner(options);
        Perg::SearchEngine engine(options);
        std::vector<Perg::FileResult> results;
        std::vector<std::unique_ptr<Perg::MmapFile>> mmap_cache;
        std::mutex results_mutex;
        unsigned int max_threads = std::thread::hardware_concurrency();

        // case 1: Single File Parallelism (Chunk-based)
        if (!options.recursive && fs::is_regular_file(target_path)) {
            auto file = std::make_unique<Perg::MmapFile>(target_path.string());
            std::string_view content = file->view();
            size_t total_size = content.size();
            size_t chunk_size = (total_size / max_threads) + 1;
            std::vector<std::future<Perg::FileResult>> futures;

            for (unsigned int i = 0; i < max_threads; ++i) {
                size_t start = i * chunk_size;
                if (start >= total_size) break;
                size_t end = std::min(total_size, (i + 1) * chunk_size);
                
                if (i > 0) {
                    size_t next_nl = content.find('\n', start);
                    start = (next_nl == std::string_view::npos) ? total_size : next_nl + 1;
                }
                if (i < max_threads - 1) {
                    size_t next_nl = content.find('\n', end);
                    end = (next_nl == std::string_view::npos) ? total_size : next_nl + 1;
                }
                if (start >= end) continue;

                int start_line = 1 + std::count(content.begin(), content.begin() + start, '\n');
                futures.push_back(std::async(std::launch::async, [&scanner, content, pattern, target_path, start_line, start, end]() {
                    return scanner.scan_chunk(content, pattern, target_path.string(), start_line, start, end);
                }));
            }
            for (auto& f : futures) results.push_back(f.get());
            mmap_cache.push_back(std::move(file));
        } 
        // CASE 2: Recursive Parallelism (File-based)
        else {
            std::vector<std::future<void>> walk_futures;
            engine.walk(target_path, [&](const fs::path& p) {
                if (!options.file_filter.empty() && p.extension() != options.file_filter) return;

                walk_futures.push_back(std::async(std::launch::async, [&, p]() {
                    try {
                        auto file = std::make_unique<Perg::MmapFile>(p.string());
                        auto res = scanner.scan_chunk(file->view(), pattern, p.string(), 1, 0, file->view().size());
                        if (!res.matches.empty() || options.count_only) {
                            std::lock_guard<std::mutex> lock(results_mutex);
                            results.push_back(std::move(res));
                            mmap_cache.push_back(std::move(file));
                        }
                    } catch (...) {}
                }));

                // Simple throttling to prevent OS thread exhaustion
                if (walk_futures.size() >= max_threads * 2) {
                    for (auto it = walk_futures.begin(); it != walk_futures.end(); ) {
                        if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                            it->get(); it = walk_futures.erase(it);
                        } else ++it;
                    }
                }
            });
            for (auto& f : walk_futures) if (f.valid()) f.get();
        }

        // Final sort to ensure deterministic regression logs
        std::sort(results.begin(), results.end(), [](const auto& a, const auto& b){ return a.filename < b.filename; });

        if (options.visualize_graph && !results.empty()) {
            Perg::TreeRenderer renderer(options);
            renderer.render(results, pattern);
        } else {
            print_results(results, options, pattern);
        }

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}