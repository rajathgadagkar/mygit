/**
 * @file diff.cpp
 * @brief LCS-based unified line-by-line diff engine.
 * @author Rajath Gadagkar <rajathgadagkar@gmail.com>
 */

#include "mygit/diff.hpp"

#include <algorithm>
#include <sstream>

namespace mygit {

std::vector<std::string> Diff::splitLines(const std::string& text) {
    std::vector<std::string> lines;
    if (text.empty()) {
        return lines;
    }

    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t nl = text.find('\n', start);
        if (nl == std::string::npos) {
            if (start < text.size()) {
                std::string line = text.substr(start);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                lines.push_back(std::move(line));
            }
            break;
        }
        std::string line = text.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
        start = nl + 1;
    }
    return lines;
}

std::vector<DiffLine> Diff::computeEditScript(const std::string& oldText,
                                              const std::string& newText) {
    const auto a = splitLines(oldText);
    const auto b = splitLines(newText);
    const std::size_t m = a.size();
    const std::size_t n = b.size();

    // DP table for Longest Common Subsequence lengths
    std::vector<std::vector<std::size_t>> dp(m + 1, std::vector<std::size_t>(n + 1, 0));
    for (std::size_t i = m; i > 0; --i) {
        for (std::size_t j = n; j > 0; --j) {
            if (a[i - 1] == b[j - 1]) {
                dp[i - 1][j - 1] = dp[i][j] + 1;
            } else {
                dp[i - 1][j - 1] = std::max(dp[i][j - 1], dp[i - 1][j]);
            }
        }
    }

    // Reconstruct edit script from DP table
    std::vector<DiffLine> script;
    script.reserve(m + n);

    std::size_t i = 0;
    std::size_t j = 0;
    while (i < m && j < n) {
        if (a[i] == b[j]) {
            script.push_back({' ', a[i]});
            ++i;
            ++j;
        } else if (dp[i + 1][j] >= dp[i][j + 1]) {
            script.push_back({'-', a[i]});
            ++i;
        } else {
            script.push_back({'+', b[j]});
            ++j;
        }
    }
    while (i < m) {
        script.push_back({'-', a[i++]});
    }
    while (j < n) {
        script.push_back({'+', b[j++]});
    }

    return script;
}

std::string Diff::formatUnified(const std::string& filepath,
                                const std::string& oldText,
                                const std::string& newText,
                                bool useColor,
                                std::size_t contextLines) {
    const auto script = computeEditScript(oldText, newText);

    // Collect indices of changed lines
    std::vector<std::size_t> changeIndices;
    for (std::size_t idx = 0; idx < script.size(); ++idx) {
        if (script[idx].op != ' ') {
            changeIndices.push_back(idx);
        }
    }

    if (changeIndices.empty()) {
        return "";
    }

    // Group nearby changes into unified diff hunks
    struct HunkRange {
        std::size_t start;
        std::size_t end;  // exclusive
    };
    std::vector<HunkRange> hunks;
    for (std::size_t idx : changeIndices) {
        const std::size_t hunkStart = (idx > contextLines) ? (idx - contextLines) : 0;
        const std::size_t hunkEnd = std::min(script.size(), idx + contextLines + 1);
        if (hunks.empty() || hunkStart > hunks.back().end) {
            hunks.push_back({hunkStart, hunkEnd});
        } else {
            hunks.back().end = std::max(hunks.back().end, hunkEnd);
        }
    }

    const char* cReset = useColor ? "\033[0m" : "";
    const char* cBold = useColor ? "\033[1m" : "";
    const char* cRed = useColor ? "\033[31m" : "";
    const char* cGreen = useColor ? "\033[32m" : "";
    const char* cCyan = useColor ? "\033[36m" : "";

    std::ostringstream oss;
    oss << cBold << "diff --mygit a/" << filepath << " b/" << filepath << cReset << '\n';
    oss << cRed << "--- a/" << filepath << cReset << '\n';
    oss << cGreen << "+++ b/" << filepath << cReset << '\n';

    // Precompute 1-based line numbers for each edit-script position
    std::vector<std::size_t> oldLineNo(script.size() + 1, 1);
    std::vector<std::size_t> newLineNo(script.size() + 1, 1);
    for (std::size_t idx = 0; idx < script.size(); ++idx) {
        oldLineNo[idx + 1] = oldLineNo[idx] + (script[idx].op != '+' ? 1u : 0u);
        newLineNo[idx + 1] = newLineNo[idx] + (script[idx].op != '-' ? 1u : 0u);
    }

    for (const auto& hunk : hunks) {
        const std::size_t oldStart = oldLineNo[hunk.start];
        const std::size_t newStart = newLineNo[hunk.start];
        const std::size_t oldCount = oldLineNo[hunk.end] - oldStart;
        const std::size_t newCount = newLineNo[hunk.end] - newStart;

        oss << cCyan << "@@ -" << oldStart << ',' << oldCount
            << " +" << newStart << ',' << newCount << " @@" << cReset << '\n';

        for (std::size_t idx = hunk.start; idx < hunk.end; ++idx) {
            const auto& entry = script[idx];
            if (entry.op == '-') {
                oss << cRed << '-' << entry.text << cReset << '\n';
            } else if (entry.op == '+') {
                oss << cGreen << '+' << entry.text << cReset << '\n';
            } else {
                oss << ' ' << entry.text << '\n';
            }
        }
    }

    return oss.str();
}

}  // namespace mygit
