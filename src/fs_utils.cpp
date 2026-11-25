/**
 * @file fs_utils.cpp
 * @brief Portable filesystem, path normalization, and ignore-pattern utilities.
 * @author Rajath Gadagkar <rajathgadagkar@gmail.com>
 */

#include "mygit/fs_utils.hpp"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace mygit {

bool FsUtils::fileExists(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec) && fs::is_regular_file(path, ec);
}

bool FsUtils::directoryExists(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec) && fs::is_directory(path, ec);
}

bool FsUtils::createDirectories(const fs::path& path) {
    std::error_code ec;
    if (fs::exists(path, ec) && fs::is_directory(path, ec)) {
        return true;
    }
    return fs::create_directories(path, ec) && !ec;
}

std::optional<std::string> FsUtils::readFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

bool FsUtils::writeFile(const fs::path& path, const std::string& content) {
    if (path.has_parent_path()) {
        createDirectories(path.parent_path());
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    return file.good();
}

std::string FsUtils::normalizeRelativePath(const fs::path& path) {
    std::string normalized = path.lexically_normal().generic_string();
    while (normalized.rfind("./", 0) == 0) {
        normalized.erase(0, 2);
    }
    if (normalized == ".") {
        return "";
    }
    while (!normalized.empty() && normalized.back() == '/') {
        normalized.pop_back();
    }
    return normalized;
}

std::vector<std::string> FsUtils::loadIgnorePatterns(const fs::path& ignoreFile) {
    std::vector<std::string> patterns;
    auto content = readFile(ignoreFile);
    if (!content.has_value()) {
        return patterns;
    }

    std::istringstream iss(*content);
    std::string line;
    while (std::getline(iss, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        patterns.push_back(trimmed);
    }
    return patterns;
}

bool FsUtils::matchesGlob(const std::string& text, const std::string& pattern) {
    std::size_t ti = 0;
    std::size_t pi = 0;
    std::size_t starIdx = std::string::npos;
    std::size_t matchIdx = 0;

    while (ti < text.size()) {
        if (pi < pattern.size() && (pattern[pi] == '?' || pattern[pi] == text[ti])) {
            ++ti;
            ++pi;
        } else if (pi < pattern.size() && pattern[pi] == '*') {
            starIdx = pi++;
            matchIdx = ti;
        } else if (starIdx != std::string::npos) {
            pi = starIdx + 1;
            ti = ++matchIdx;
        } else {
            return false;
        }
    }

    while (pi < pattern.size() && pattern[pi] == '*') {
        ++pi;
    }
    return pi == pattern.size();
}

bool FsUtils::isIgnored(const std::string& normalizedRelPath,
                        const std::vector<std::string>& patterns) {
    if (normalizedRelPath.empty()) {
        return true;
    }

    // Always ignore internal repository metadata directories
    if (normalizedRelPath == ".mygit" || normalizedRelPath.rfind(".mygit/", 0) == 0 ||
        normalizedRelPath == ".git" || normalizedRelPath.rfind(".git/", 0) == 0) {
        return true;
    }

    const std::string filename = fs::path(normalizedRelPath).filename().string();

    for (const auto& rawPattern : patterns) {
        std::string pat = rawPattern;
        bool dirOnly = false;
        if (!pat.empty() && pat.back() == '/') {
            dirOnly = true;
            pat.pop_back();
        }
        if (pat.empty()) {
            continue;
        }

        if (dirOnly) {
            if (normalizedRelPath == pat || normalizedRelPath.rfind(pat + "/", 0) == 0) {
                return true;
            }
            continue;
        }

        // Match against full relative path or basename
        if (matchesGlob(normalizedRelPath, pat) || matchesGlob(filename, pat)) {
            return true;
        }

        // Also match if path is inside a matching top-level directory
        if (normalizedRelPath.rfind(pat + "/", 0) == 0) {
            return true;
        }
    }

    return false;
}

std::string FsUtils::currentTimestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm localTm{};
#if defined(_WIN32)
    localtime_s(&localTm, &now);
#else
    localtime_r(&now, &localTm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&localTm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string FsUtils::trim(const std::string& text) {
    const std::size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

}  // namespace mygit
