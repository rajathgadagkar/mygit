/**
 * @file fs_utils.hpp
 * @brief Portable filesystem, path normalization, and ignore-pattern utilities.
 * @author Rajath Gadagkar <rajathgadagkar@gmail.com>
 */

#ifndef MYGIT_FS_UTILS_HPP
#define MYGIT_FS_UTILS_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace mygit {

namespace fs = std::filesystem;

/**
 * @brief Filesystem and path helper utilities used across repository modules.
 */
class FsUtils {
public:
    static bool fileExists(const fs::path& path);
    static bool directoryExists(const fs::path& path);
    static bool createDirectories(const fs::path& path);

    static std::optional<std::string> readFile(const fs::path& path);
    static bool writeFile(const fs::path& path, const std::string& content);

    /**
     * @brief Normalizes a relative path to POSIX format without leading "./".
     * Example: "./src//main.cpp" -> "src/main.cpp".
     */
    static std::string normalizeRelativePath(const fs::path& path);

    /**
     * @brief Loads non-empty, non-comment patterns from a .mygitignore file.
     */
    static std::vector<std::string> loadIgnorePatterns(const fs::path& ignoreFile);

    /**
     * @brief Returns true if a normalized relative path matches any ignore pattern
     *        or belongs inside .mygit / .git metadata directories.
     */
    static bool isIgnored(const std::string& normalizedRelPath,
                          const std::vector<std::string>& patterns);

    /**
     * @brief Simple wildcard glob matcher supporting '*' and '?'.
     */
    static bool matchesGlob(const std::string& text, const std::string& pattern);

    /**
     * @brief Returns formatted local timestamp (YYYY-MM-DD HH:MM:SS).
     */
    static std::string currentTimestamp();

    /**
     * @brief Trims leading and trailing whitespace characters.
     */
    static std::string trim(const std::string& text);
};

}  // namespace mygit

#endif  // MYGIT_FS_UTILS_HPP
