#pragma once
#include "scanner.hpp"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Perg {

/**
 * @struct TreeNode
 * @brief Represents a directory or file in the visual result tree.
 */
struct TreeNode {
    std::string name;
    bool is_file = false;
    int total_matches = 0;
    std::vector<MatchRecord> matches;
    std::map<std::string, std::unique_ptr<TreeNode>> children; ///< Sorted map for deterministic tree output.
};

/**
 * @class TreeRenderer
 * @brief Formats and prints scan results as an ASCII directory tree.
 */
class TreeRenderer {
public:
    explicit TreeRenderer(const ScanOptions& options) : options_(options) {}

    /**
     * @brief Builds and renders the result tree to stdout.
     */
    void render(const std::vector<FileResult>& results, const std::string& pattern);

private:
    ScanOptions options_;
    void build_tree(TreeNode& root, const FileResult& result);
    void draw_node(const TreeNode& node, const std::string& prefix, bool is_last, const std::string& pattern);
    void highlight_content(std::string_view line, const std::regex& re);
};

} // namespace Perg