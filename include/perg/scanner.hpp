#pragma once
#include <string_view>
#include <regex>
#include <string>

namespace Perg {
    struct ScanOptions {
        bool use_color = false;
        bool count_only = false;
        bool print_filename = false;
        bool ignore_case = false;
        int context_before = 0;
        int context_after = 0;
    };

    class Scanner {
    public:
        explicit Scanner(ScanOptions options) : options_(options) {}
        
        int scan(std::string_view content, const std::string& pattern, const std::string& filename);

    private:
        ScanOptions options_;
        void print_line(int line_no, std::string_view line, const std::regex& re, int padding, const std::string& filename, bool is_match);
    };
}