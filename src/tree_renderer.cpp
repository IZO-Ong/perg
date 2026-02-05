#include "perg/tree_renderer.hpp"
#include "perg/colors.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <iomanip>

namespace fs = std::filesystem;

namespace Perg {
    void TreeRenderer::render(const std::vector<FileResult>& results, const std::string& pattern) {
        if (results.empty()) return;
        TreeNode root;
        root.name = ""; 
        for (const auto& res : results) build_tree(root, res);

        std::function<void(TreeNode&)> sort_node = [&](TreeNode& node) {
            if (node.is_file) {
                std::sort(node.matches.begin(), node.matches.end(), [](const auto& a, const auto& b) {
                    return a.line_no < b.line_no;
                });
            }
            for (auto& [name, child] : node.children) {
                sort_node(*child);
            }
        };
        sort_node(root);

        if (root.children.size() == 1) {
            draw_node(*root.children.begin()->second, "", true, pattern);
        } else {
            draw_node(root, "", true, pattern);
        }
    }

    void TreeRenderer::build_tree(TreeNode& root, const FileResult& result) {
        std::filesystem::path p(result.filename);
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
        current->matches.insert(current->matches.end(), result.matches.begin(), result.matches.end());
    }

    void TreeRenderer::draw_node(const TreeNode& node, const std::string& prefix, bool is_last, const std::string& pattern) {
        auto& out = std::cout;

        std::regex_constants::syntax_option_type flags = std::regex::optimize;
        if (options_.ignore_case) flags |= std::regex::icase;
        std::regex re(pattern, flags);

        out << prefix << (is_last ? "+-- " : "|-- ");
        if (node.is_file) {
            if (options_.use_color) out << Colors::MAGENTA;
            out << node.name << (options_.use_color ? Colors::RESET : "");
            out << " (" << node.total_matches << ")\n";

            if (!options_.count_only) {
                int max_ln = node.matches.empty() ? 0 : node.matches.back().line_no;
                int padding = std::max(4, (max_ln > 0 ? (int)std::log10(max_ln) + 1 : 1));
                std::string m_prefix = prefix + (is_last ? "    " : "|   ");
                
                for (size_t i = 0; i < node.matches.size(); ++i) {
                    bool last_m = (i == node.matches.size() - 1);
                    out << m_prefix << (last_m ? "+-- " : "|-- ");
                    if (options_.use_color) out << Colors::YELLOW;
                    out << std::setw(padding) << std::left << node.matches[i].line_no;
                    out << (options_.use_color ? Colors::RESET : "") << " : ";

                    std::string_view content = node.matches[i].content;
                    size_t first = content.find_first_not_of(" \t\r\n");
                    if (first != std::string_view::npos) content.remove_prefix(first);
                    highlight_content(content, re); 
                    out << "\n";
                }
            }
        } else {
            if (options_.use_color) out << Colors::BOLD;
            out << node.name << "/";
            if (options_.use_color) out << Colors::RESET;
            out << " (" << node.total_matches << ")\n";
        }

        std::vector<std::pair<std::string, TreeNode*>> sorted;
        for (auto& [name, child] : node.children) sorted.push_back({name, child.get()});
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
            if (a.second->is_file != b.second->is_file) return !a.second->is_file;
            return a.first < b.first;
        });

        std::string next_p = prefix + (is_last ? "    " : "|   ");
        for (size_t i = 0; i < sorted.size(); ++i) {
            draw_node(*sorted[i].second, next_p, (i == sorted.size() - 1), pattern);
        }
    }

    void TreeRenderer::highlight_content(std::string_view line, const std::regex& re) {
        auto& out = std::cout;
        size_t last_pos = 0;
        std::cregex_iterator it(line.data(), line.data() + line.size(), re), end;
        for (; it != end; ++it) {
            out << line.substr(last_pos, it->position() - last_pos);
            if (options_.use_color) out << Colors::BOLD << Colors::CYAN;
            out << it->str() << (options_.use_color ? Colors::RESET : "");
            last_pos = it->position() + it->length();
        }
        out << line.substr(last_pos);
    }
}