#include "perg/search_engine.hpp"
#include "perg/exceptions.hpp"

namespace Perg {

/**
 * @brief Traverses the filesystem starting from a given root and executes a 
 * callback for every valid file found.
 * This function handles both single-file targets and directory trees. It 
 * abstracts the complexity of recursion, permission errors, and file filtering 
 * away from the main search logic.
 * @param root The starting filesystem path (file or directory).
 * @param callback A function to execute for each discovered file that passes 
 * the filter criteria.
 * @throws FileError If the path is invalid or if a directory is targeted 
 * without recursive mode enabled.
 */
void SearchEngine::walk(const std::filesystem::path& root, 
                        std::function<void(const std::filesystem::path&)> callback) {
    namespace fs = std::filesystem;

    if (!fs::exists(root)) {
        throw FileError("Path does not exist: " + root.string());
    }

    if (fs::is_regular_file(root)) {
        callback(root);
        return;
    }

    if (fs::is_directory(root)) {
        if (!options_.recursive) {
            throw FileError("perg: " + root.string() + ": Is a directory (use -r)");
        }

        /* We use skip_permission_denied to prevent the entire scan from 
         * crashing if user encounters a system or restricted folder
         */
        auto iter_opt = fs::directory_options::skip_permission_denied;
        for (const auto& entry : fs::recursive_directory_iterator(root, iter_opt)) {
            
            if (fs::is_regular_file(entry.path())) {
                
                // Extension filtering is handled early to avoid unnecessary I/O ops
                if (!options_.file_filter.empty() && 
                    entry.path().extension() != options_.file_filter) {
                    continue;
                }

                /* Empty files are skipped as they cannot contain patterns.
                 */
                std::error_code ec;
                if (fs::file_size(entry.path(), ec) == 0 || ec) {
                    continue;
                }

                callback(entry.path());
            }
        }
    }
}

} // namespace Perg