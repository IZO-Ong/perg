#pragma once
#include <string>
#include <vector>
#include <string_view>
#include <regex>

namespace Perg {

    struct ScanOptions {
        bool visualize_graph = false;
        bool print_line_numbers = true;
        bool use_color = false;
        bool count_only = false;
        bool print_filename = false;
        bool ignore_case = false;
        bool recursive = false;
        std::string file_filter = "";
        int context_before = 0;
        int context_after = 0;
    };

    struct MatchRecord {
        int line_no;
        std::string_view content;
        bool is_context;
        
        bool operator==(const MatchRecord& other) const {
            return line_no == other.line_no;
        }
    };

    struct FileResult {
        std::string filename;
        std::vector<MatchRecord> matches;
        size_t total_matches = 0;
    };

    class Scanner {
    public:
        explicit Scanner(const ScanOptions& options) : options_(options) {}

        FileResult scan(std::string_view content, const std::string& pattern, const std::string& filename);

        FileResult scan_chunk(std::string_view full_content, 
                              const std::string& pattern, 
                              const std::string& filename, 
                              int start_line,
                              size_t range_start = 0,
                              size_t range_end = std::string_view::npos);

    private:
        ScanOptions options_;
    };

    bool is_binary(std::string_view content);
}