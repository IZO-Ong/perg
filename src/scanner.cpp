#include "perg/colors.hpp"
#include "perg/exceptions.hpp"
#include "perg/scanner.hpp"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <regex>
#include <algorithm>

namespace fs = std::filesystem;

namespace Perg {

    bool is_literal(const std::string& p) {
        return p.find_first_of(".+*?^$()[]{}|\\") == std::string::npos;
    }

    bool is_binary(std::string_view content) {
        size_t check_limit = std::min(content.size(), (size_t)1024);
        for (size_t i = 0; i < check_limit; ++i) {
            if (content[i] == '\0') return true;
        }
        return false;
    }

    size_t find_backward_nl(std::string_view content, size_t pos, int n) {
        if (n <= 0 || pos == 0) return pos;
        size_t current = pos - 1;
        int found = 0;
        while (current > 0) {
            if (content[current] == '\n') {
                found++;
                if (found > n) return current + 1;
            }
            current--;
        }
        return 0;
    }

    size_t find_forward_nl(std::string_view content, size_t pos, int n) {
        if (n <= 0 || pos >= content.size()) return pos;
        size_t current = pos;
        int found = 0;
        while (current < content.size()) {
            if (content[current] == '\n') {
                found++;
                if (found >= n) return current;
            }
            current++;
        }
        return content.size();
    }

    void Scanner::render_output_group(std::string_view content, int match_ln, 
            size_t line_start, size_t line_end, 
            size_t& last_printed_pos, const std::regex& re, 
            const std::string& filename) {
    
        size_t group_start = find_backward_nl(content, line_start, options_.context_before);
        
        if (last_printed_pos > 0 && group_start > last_printed_pos) {
            if (options_.context_before > 0 || options_.context_after > 0) {
                if (options_.use_color) std::cout << Colors::CYAN;
                std::cout << "--\n";
                if (options_.use_color) std::cout << Colors::RESET;
            }
        }

        size_t current_pos = std::max(group_start, last_printed_pos);
        
        int current_ln = match_ln - (int)std::count(content.data() + group_start, 
                                                content.data() + line_start, '\n');
        if (current_pos > group_start) {
            current_ln += (int)std::count(content.data() + group_start, 
                                        content.data() + current_pos, '\n');
        }

        size_t group_end = find_forward_nl(content, line_end, options_.context_after + 1);

        while (current_pos < group_end) {
            size_t next_nl = content.find('\n', current_pos);
            if (next_nl == std::string_view::npos) next_nl = content.size();
            
            std::string_view line_view = content.substr(current_pos, next_nl - current_pos);
            bool is_match_line = (current_ln == match_ln);
            
            print_line(current_ln, line_view, re, 4, filename, is_match_line);
            
            current_pos = next_nl + 1;
            current_ln++;
            if (next_nl == content.size()) break;
        }

        last_printed_pos = current_pos;
    }

    void Scanner::build_tree(TreeNode& root, const FileResult& result) {
        fs::path p(result.filename);
        TreeNode* current = &root;

        for (auto& part : p) {
            std::string part_str = part.string();
            if (part_str == "." || part_str == "./") continue;

            if (current->children.find(part_str) == current->children.end()) {
                current->children[part_str] = std::make_unique<TreeNode>();
                current->children[part_str]->name = part_str;
            }
            current = current->children[part_str].get();
            current->total_matches += result.total_matches; 
        }
        current->is_file = true;
        current->matches = result.matches;
    }

    void Scanner::print_tree_graph(const std::vector<FileResult>& results, const std::string& pattern) {
        if (results.empty()) return;

        TreeNode root;
        root.name = ""; 
        
        for (const auto& res : results) {
            build_tree(root, res);
        }

        if (root.children.size() == 1) {
            draw_node(*root.children.begin()->second, "", true, pattern);
        } else {
            draw_node(root, "", true, pattern);
        }
    }

    FileResult Scanner::scan(std::string_view content, const std::string& pattern, const std::string& filename) {
        FileResult file_res;
        file_res.filename = filename;

        if (is_binary(content)) return file_res;

        std::regex_constants::syntax_option_type flags = std::regex::optimize;
        if (options_.ignore_case) flags |= std::regex::icase;
        
        std::regex re;
        try {
            re = std::regex(pattern, flags);
        } catch (const std::regex_error& e) {
            throw RegexError(e.what());
        }

        int current_line_no = 1;
        size_t last_line_start = 0;
        size_t last_printed_pos = 0;

        auto it = std::cregex_iterator(content.data(), content.data() + content.size(), re);
        auto end = std::cregex_iterator();

        while (it != end) {
            size_t match_pos = it->position();
            
            size_t line_start = content.find_last_of('\n', match_pos);
            line_start = (line_start == std::string_view::npos) ? 0 : line_start + 1;
            size_t line_end = content.find('\n', match_pos);
            if (line_end == std::string_view::npos) line_end = content.size();

            current_line_no += std::count(content.data() + last_line_start, 
                                        content.data() + line_start, '\n');
            last_line_start = line_start;

            std::string_view line_view = content.substr(line_start, line_end - line_start);

            file_res.matches.push_back({current_line_no, std::string(line_view)});

            while (it != end && (size_t)it->position() < line_end) {
                file_res.total_matches++;
                ++it;
            }

            if (!options_.visualize_graph && !options_.count_only) {
                render_output_group(content, current_line_no, line_start, line_end, 
                                last_printed_pos, re, filename);
            }
        }

        if (options_.count_only && !options_.visualize_graph) {
            if (options_.print_filename) std::cout << filename << ":";
            std::cout << file_res.total_matches << "\n";
        }

        return file_res;
    }

    void Scanner::highlight_content(std::string_view line, const std::regex& re) {
        auto& out = std::cout;
        size_t last_pos = 0;
        std::cregex_iterator it(line.data(), line.data() + line.size(), re), end;

        for (; it != end; ++it) {
            out << line.substr(last_pos, it->position() - last_pos);
            if (options_.use_color) out << Colors::BOLD << Colors::CYAN;
            out << it->str();
            if (options_.use_color) out << Colors::RESET;
            last_pos = it->position() + it->length();
        }
        out << line.substr(last_pos);
    }

    void Scanner::print_line(int line_no, std::string_view line, const std::regex& re, 
                        int padding, const std::string& filename, bool is_match) {
        auto& out = std::cout;

        size_t first_content = line.find_first_not_of(" \t\r\n");
        if (first_content != std::string_view::npos) {
            line.remove_prefix(first_content);
        } else {
            line = "";
        }

        if (options_.print_line_numbers) {
            if (options_.use_color) out << Colors::YELLOW;
            out << std::setw(padding) << std::left << line_no;
            if (options_.use_color) out << Colors::RESET;
            
            out << (is_match ? " : " : " - ");
        }

        if (!is_match) {
            out << line;
        } else {
            highlight_content(line, re);
        }

        // 4. Print Filename Suffix
        if (options_.print_filename) {
            out << " | ";
            if (options_.use_color) out << Colors::CYAN;
            out << filename;
            if (options_.use_color) out << Colors::RESET;
        }

        out << "\n";
    }

    void Scanner::draw_node(const TreeNode& node, const std::string& prefix, bool is_last, const std::string& pattern) {
        auto& out = std::cout;

        std::regex_constants::syntax_option_type flags = std::regex::optimize;
        if (options_.ignore_case) flags |= std::regex::icase;
        std::regex re(pattern, flags);

        out << prefix << (is_last ? "+-- " : "|-- ");
        
        if (node.is_file) {
            if (options_.use_color) out << Colors::CYAN;
            out << node.name;
            if (options_.use_color) out << Colors::RESET;
            
            out << " (" << node.total_matches << ")\n";

            if (!options_.count_only) {
                int max_ln = node.matches.empty() ? 0 : node.matches.back().line_no;
                int padding = std::max(4, (max_ln > 0 ? (int)std::log10(max_ln) + 1 : 1));
                std::string match_prefix = prefix + (is_last ? "    " : "|   ");
                
                for (size_t i = 0; i < node.matches.size(); ++i) {
                    bool last_match = (i == node.matches.size() - 1);
                    out << match_prefix << (last_match ? "+-- " : "|-- ");
                    
                    if (options_.use_color) out << Colors::YELLOW;
                    out << std::setw(padding) << std::left << node.matches[i].line_no;
                    if (options_.use_color) out << Colors::RESET;
                    
                    out << " : ";

                    std::string_view content = node.matches[i].content;
                    size_t first = content.find_first_not_of(" \t\r\n");
                    if (first != std::string_view::npos) content.remove_prefix(first);

                    highlight_content(content, re); 
                    out << "\n";
                }
            }
        } else {
            out << node.name << "/ (" << node.total_matches << ")\n";
        }

        std::vector<std::pair<std::string, TreeNode*>> sorted_children;
        for (auto& [name, child] : node.children) {
            sorted_children.push_back({name, child.get()});
        }

        // Sort based on lexographical order
        std::sort(sorted_children.begin(), sorted_children.end(), 
            [](const auto& a, const auto& b) {
                if (a.second->is_file != b.second->is_file) {
                    return !a.second->is_file;
                }
                return a.first < b.first;
            }
        );

        std::string new_prefix = prefix + (is_last ? "    " : "|   ");
        for (size_t i = 0; i < sorted_children.size(); ++i) {
            bool last_child = (i == sorted_children.size() - 1);
            draw_node(*sorted_children[i].second, new_prefix, last_child, pattern);
        }
    }
}