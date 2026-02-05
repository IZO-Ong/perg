#include "perg/search_engine.hpp"
#include "perg/exceptions.hpp"

namespace Perg {
    void SearchEngine::walk(const std::filesystem::path& root, 
                            std::function<void(const std::filesystem::path&)> callback) {
        namespace fs = std::filesystem;

        if (!fs::exists(root)) throw FileError("Path does not exist: " + root.string());

        if (fs::is_regular_file(root)) {
            callback(root);
            return;
        }

        if (fs::is_directory(root)) {
            if (!options_.recursive) {
                throw FileError("perg: " + root.string() + ": Is a directory (use -r)");
            }

            for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
                if (fs::is_regular_file(entry.path())) {
                    if (!options_.file_filter.empty() && entry.path().extension() != options_.file_filter) 
                        continue;

                    std::error_code ec;
                    if (fs::file_size(entry.path(), ec) == 0 || ec) continue;

                    callback(entry.path());
                }
            }
        }
    }
}