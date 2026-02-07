#pragma once
#include <string>
#include <vector>
#include <string_view>
#include <regex>

namespace Perg {

/**
 * @struct ScanOptions
 * @brief Configuration parameters for search behavior and output formatting.
 */
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

/**
 * @struct MatchRecord
 * @brief Represents a single line matching the pattern or providing context.
 */
struct MatchRecord {
    int line_no;              ///< 1-based line number in the source file.
    std::string_view content; ///< View into the mapped memory for the line.
    bool is_context;          ///< True if this line is context, false if a direct match.
    
    bool operator==(const MatchRecord& other) const {
        return line_no == other.line_no;
    }
};

/**
 * @struct FileResult
 * @brief Aggregated search results for a single file.
 */
struct FileResult {
    std::string filename;
    std::vector<MatchRecord> matches;
    size_t total_matches = 0; ///< Total occurrences, which may exceed line count.
};

/**
 * @class Scanner
 * @brief Orchestrates pattern matching within file content.
 */
class Scanner {
public:
    explicit Scanner(const ScanOptions& options) : options_(options) {}

    /**
     * @brief Scans a full content view for the given pattern.
     */
    FileResult scan(std::string_view content, const std::string& pattern, const std::string& filename);

    /**
     * @brief Scans a specific byte range, used for parallelizing large file searches.
     * @param start_line Theoretical line number at range_start for result alignment.
     */
    FileResult scan_chunk(std::string_view full_content, 
                          const std::string& pattern, 
                          const std::string& filename, 
                          int start_line,
                          size_t range_start = 0,
                          size_t range_end = std::string_view::npos);

private:
    ScanOptions options_;
};

/**
 * @brief Performs a fast check for null bytes to determine if content is binary.
 */
bool is_binary(std::string_view content);

} // namespace Perg