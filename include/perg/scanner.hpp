#pragma once
#include <string_view>
#include <regex>
#include <string>

namespace Perg {
    struct ScanOptions {
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

    class Scanner {
    public:
        explicit Scanner(ScanOptions options) : options_(options) {}
        
        int scan(std::string_view content, const std::string& pattern, const std::string& filename);

    private:
        ScanOptions options_;

        void render_output_group(std::string_view content, int match_ln, 
                                 size_t line_start, size_t line_end, 
                                 size_t& last_printed_pos, const std::regex& re, 
                                 const std::string& filename);

        void print_line(int line_no, std::string_view line, const std::regex& re, 
                        int padding, const std::string& filename, bool is_match);
    };
}