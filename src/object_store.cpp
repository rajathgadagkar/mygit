/**
 * @file object_store.cpp
 * @brief Content-addressable Git object database (.mygit/objects).
 * @author Rajath Gadagkar <rajathgadagkar@gmail.com>
 */

#include "mygit/object_store.hpp"

#include "mygit/compression.hpp"
#include "mygit/fs_utils.hpp"
#include "mygit/sha1.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace mygit {

ObjectStore::ObjectStore(fs::path objectsDir) : objectsDir_(std::move(objectsDir)) {}

bool ObjectStore::isHexString(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isxdigit(ch) != 0;
    });
}

fs::path ObjectStore::objectPath(const std::string& fullHash) const {
    return objectsDir_ / fullHash.substr(0, 2) / fullHash.substr(2);
}

std::string ObjectStore::computeHash(const std::string& type, const std::string& data) {
    std::string raw;
    raw.reserve(type.size() + 16 + data.size());
    raw += type;
    raw.push_back(' ');
    raw += std::to_string(data.size());
    raw.push_back('\0');
    raw += data;
    return SHA1::hashString(raw);
}

std::string ObjectStore::store(const std::string& type, const std::string& data) const {
    std::string raw;
    raw.reserve(type.size() + 16 + data.size());
    raw += type;
    raw.push_back(' ');
    raw += std::to_string(data.size());
    raw.push_back('\0');
    raw += data;

    const std::string hash = SHA1::hashString(raw);
    const fs::path dst = objectPath(hash);

    // Content-addressed objects are immutable; skip rewrite if already stored
    if (FsUtils::fileExists(dst)) {
        return hash;
    }

    auto compressed = Compression::deflateData(raw);
    const std::string& payload = compressed.has_value() ? *compressed : raw;
    if (!FsUtils::writeFile(dst, payload)) {
        return "";
    }

    return hash;
}

std::optional<std::string> ObjectStore::resolveHash(const std::string& hashOrPrefix) const {
    if (hashOrPrefix.size() < 4 || hashOrPrefix.size() > 40 || !isHexString(hashOrPrefix)) {
        return std::nullopt;
    }

    std::string lower = hashOrPrefix;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (lower.size() == 40) {
        return FsUtils::fileExists(objectPath(lower)) ? std::optional<std::string>(lower)
                                                      : std::nullopt;
    }

    // Resolve abbreviated prefix by scanning the 2-hex-char bucket directory
    const fs::path bucketDir = objectsDir_ / lower.substr(0, 2);
    if (!FsUtils::directoryExists(bucketDir)) {
        return std::nullopt;
    }

    const std::string suffixPrefix = lower.substr(2);
    std::string matchedHash;
    std::size_t matches = 0;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(bucketDir, ec)) {
        if (!entry.is_regular_file(ec)) {
            continue;
        }
        const std::string fname = entry.path().filename().string();
        if (fname.rfind(suffixPrefix, 0) == 0) {
            matchedHash = lower.substr(0, 2) + fname;
            ++matches;
            if (matches > 1) {
                return std::nullopt;  // Ambiguous prefix
            }
        }
    }

    return (matches == 1) ? std::optional<std::string>(matchedHash) : std::nullopt;
}

std::optional<GitObject> ObjectStore::read(const std::string& hashOrPrefix) const {
    auto resolved = resolveHash(hashOrPrefix);
    if (!resolved.has_value()) {
        return std::nullopt;
    }

    auto stored = FsUtils::readFile(objectPath(*resolved));
    if (!stored.has_value()) {
        return std::nullopt;
    }

    auto inflated = Compression::inflateData(*stored);
    const std::string raw = inflated.has_value() ? *inflated : *stored;

    const std::size_t nullPos = raw.find('\0');
    if (nullPos == std::string::npos) {
        return std::nullopt;
    }

    const std::string header = raw.substr(0, nullPos);
    const std::size_t spacePos = header.find(' ');
    if (spacePos == std::string::npos) {
        return std::nullopt;
    }

    GitObject obj;
    obj.type = header.substr(0, spacePos);
    try {
        obj.size = static_cast<std::size_t>(std::stoull(header.substr(spacePos + 1)));
    } catch (...) {
        return std::nullopt;
    }
    obj.data = raw.substr(nullPos + 1);
    return obj;
}

bool ObjectStore::exists(const std::string& hashOrPrefix) const {
    return resolveHash(hashOrPrefix).has_value();
}

std::size_t ObjectStore::objectCount() const {
    if (!FsUtils::directoryExists(objectsDir_)) {
        return 0;
    }
    std::size_t count = 0;
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(objectsDir_, ec)) {
        if (entry.is_regular_file(ec)) {
            ++count;
        }
    }
    return count;
}

std::string ObjectStore::serializeTree(std::vector<TreeEntry> entries) {
    // Sort deterministically by entry name for reproducible tree hashes
    std::sort(entries.begin(), entries.end(), [](const TreeEntry& a, const TreeEntry& b) {
        return a.name < b.name;
    });

    std::string serialized;
    for (const auto& entry : entries) {
        serialized += entry.mode;
        serialized.push_back(' ');
        serialized += entry.name;
        serialized.push_back('\0');  // Explicit NUL delimiter (never C-string "\0")
        serialized += entry.hash;
    }
    return serialized;
}

std::vector<TreeEntry> ObjectStore::parseTree(const std::string& treeData) {
    std::vector<TreeEntry> entries;
    std::size_t pos = 0;

    while (pos < treeData.size()) {
        const std::size_t spacePos = treeData.find(' ', pos);
        if (spacePos == std::string::npos) {
            break;
        }

        const std::size_t nullPos = treeData.find('\0', spacePos + 1);
        if (nullPos == std::string::npos) {
            break;
        }

        // Verify 40-char SHA-1 hex digest fits after the NUL byte
        if (nullPos + 1 + 40 > treeData.size()) {
            break;
        }

        TreeEntry entry;
        entry.mode = treeData.substr(pos, spacePos - pos);
        entry.name = treeData.substr(spacePos + 1, nullPos - spacePos - 1);
        entry.hash = treeData.substr(nullPos + 1, 40);
        entry.type = (entry.mode == "040000") ? "tree" : "blob";
        entries.push_back(std::move(entry));

        pos = nullPos + 1 + 40;
    }

    return entries;
}

}  // namespace mygit
