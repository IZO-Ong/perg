#pragma once
#include "scanner.hpp"
#include <filesystem>
#include <functional>

namespace Perg {

/**
 * @class SearchEngine
 * @brief Handles filesystem traversal and file discovery logic.
 */
class SearchEngine {
public:
    explicit SearchEngine(const ScanOptions& options) : options_(options) {}

    /**
     * @brief Recursively or shallowly traverses a path.
     * @param root Starting path.
     * @param callback Function to execute for every valid file found.
     */
    void walk(const std::filesystem::path& root, 
              std::function<void(const std::filesystem::path&)> callback);

private:
    ScanOptions options_;
};

} // namespace Perg