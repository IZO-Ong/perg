#pragma once
#include <string_view>
#include <regex>

namespace Perg {
    struct ScanOptions {
        bool use_color = false;
        bool count_only = false;
    };

    class Scanner {
    public:
        explicit Scanner(ScanOptions options) : options_(options) {}
        
        int scan(std::string_view content, const std::string& pattern);

    private:
        ScanOptions options_;
        void print_line(int line_no, std::string_view line, const std::regex& re, int padding);
    };
}