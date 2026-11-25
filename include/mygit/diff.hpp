/**
 * @file diff.hpp
 * @brief LCS-based unified line-by-line diff engine.
 * @author Rajath Gadagkar <rajathgadagkar@gmail.com>
 */

#ifndef MYGIT_DIFF_HPP
#define MYGIT_DIFF_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace mygit {

/** @brief Represents a single line operation in a line-level diff. */
struct DiffLine {
    char op = ' ';       ///< ' ' (context), '-' (removed), '+' (added)
    std::string text;
};

/**
 * @brief Computes unified line-by-line diffs using Longest Common Subsequence.
 */
class Diff {
public:
    /** @brief Splits text into individual lines (stripping trailing '\\r'). */
    static std::vector<std::string> splitLines(const std::string& text);

    /** @brief Computes the edit script transforming oldText into newText. */
    static std::vector<DiffLine> computeEditScript(const std::string& oldText,
                                                   const std::string& newText);

    /**
     * @brief Formats a unified diff between two versions of a file.
     * @param filepath Display path of the file.
     * @param oldText Staged (baseline) content.
     * @param newText Working directory (current) content.
     * @param useColor Whether to emit ANSI color escape codes.
     * @param contextLines Number of unchanged lines surrounding each change.
     */
    static std::string formatUnified(const std::string& filepath,
                                     const std::string& oldText,
                                     const std::string& newText,
                                     bool useColor = false,
                                     std::size_t contextLines = 3);
};

}  // namespace mygit

#endif  // MYGIT_DIFF_HPP
