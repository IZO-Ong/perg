#pragma once
#include "scanner.hpp"
#include <filesystem>
#include <functional>

namespace Perg {
    class SearchEngine {
    public:
        explicit SearchEngine(const ScanOptions& options) : options_(options) {}

        // walks path and executes the callback for each file found
        void walk(const std::filesystem::path& root, 
                  std::function<void(const std::filesystem::path&)> callback);

    private:
        ScanOptions options_;
    };
}