#pragma once
#include <string_view>
#include <regex>
#include <string>
#include <vector>
#include <map>
#include <memory>

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
        std::string content;
    };

    struct FileResult {
        std::string filename;
        int total_matches = 0;
        std::vector<MatchRecord> matches;
    };

    struct TreeNode {
        std::string name;
        bool is_file = false;
        int total_matches = 0;
        std::vector<MatchRecord> matches;
        std::map<std::string, std::unique_ptr<TreeNode>> children;
    };

    class Scanner {
    public:
        explicit Scanner(ScanOptions options) : options_(options) {}
        
        FileResult scan(std::string_view content, const std::string& pattern, const std::string& filename);

        void print_tree_graph(const std::vector<FileResult>& results, const std::string& pattern);

    private:
        ScanOptions options_;

        void render_output_group(std::string_view content, int match_ln, 
                                 size_t line_start, size_t line_end, 
                                 size_t& last_printed_pos, const std::regex& re, 
                                 const std::string& filename);

        void highlight_content(std::string_view line, const std::regex& re);

        void print_line(int line_no, std::string_view line, const std::regex& re, 
                        int padding, const std::string& filename, bool is_match);

        void build_tree(TreeNode& root, const FileResult& result);
        
        void draw_node(const TreeNode& node, const std::string& prefix, bool is_last, const std::string& pattern);
    };
}