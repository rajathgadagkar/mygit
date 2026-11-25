/**
 * @file index.cpp
 * @brief Staging area (.mygit/index) reader and writer.
 * @author Rajath Gadagkar <rajathgadagkar@gmail.com>
 */

#include "mygit/index.hpp"

#include "mygit/fs_utils.hpp"

#include <sstream>
#include <utility>

namespace mygit {

Index::Index(fs::path indexPath) : indexPath_(std::move(indexPath)) {}

bool Index::load() {
    entries_.clear();
    if (!FsUtils::fileExists(indexPath_)) {
        return true;
    }

    auto content = FsUtils::readFile(indexPath_);
    if (!content.has_value()) {
        return false;
    }

    std::istringstream iss(*content);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) {
            continue;
        }
        const std::size_t spacePos = line.find(' ');
        if (spacePos == std::string::npos || spacePos != 40) {
            continue;
        }
        const std::string hash = line.substr(0, spacePos);
        const std::string relPath = FsUtils::normalizeRelativePath(line.substr(spacePos + 1));
        if (!relPath.empty()) {
            entries_[relPath] = hash;
        }
    }
    return true;
}

bool Index::save() const {
    std::ostringstream oss;
    for (const auto& [path, hash] : entries_) {
        oss << hash << ' ' << path << '\n';
    }
    return FsUtils::writeFile(indexPath_, oss.str());
}

void Index::stage(const std::string& normalizedPath, const std::string& blobHash) {
    entries_[normalizedPath] = blobHash;
}

bool Index::remove(const std::string& normalizedPath) {
    return entries_.erase(normalizedPath) > 0;
}

void Index::clear() {
    entries_.clear();
}

void Index::replaceAll(std::map<std::string, std::string> newEntries) {
    entries_ = std::move(newEntries);
}

bool Index::contains(const std::string& normalizedPath) const {
    return entries_.find(normalizedPath) != entries_.end();
}

std::optional<std::string> Index::getHash(const std::string& normalizedPath) const {
    auto it = entries_.find(normalizedPath);
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return it->second;
}

const std::map<std::string, std::string>& Index::entries() const {
    return entries_;
}

bool Index::empty() const {
    return entries_.empty();
}

}  // namespace mygit
