/**
 * @file object_store.hpp
 * @brief Content-addressable Git object database (.mygit/objects).
 * @author Rajath Gadagkar <rajathgadagkar@gmail.com>
 */

#ifndef MYGIT_OBJECT_STORE_HPP
#define MYGIT_OBJECT_STORE_HPP

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace mygit {

namespace fs = std::filesystem;

/** @brief Represents a decoded Git object (blob, tree, or commit). */
struct GitObject {
    std::string type;
    std::size_t size = 0;
    std::string data;
};

/** @brief Represents a single entry inside a tree object. */
struct TreeEntry {
    std::string mode;  ///< "100644" for regular file, "040000" for directory tree
    std::string type;  ///< "blob" or "tree"
    std::string hash;  ///< 40-character SHA-1 hex digest
    std::string name;  ///< Entry filename (single path component)
};

/**
 * @brief Manages reading, writing, and resolving content-addressed objects.
 *
 * Objects are formatted as `"<type> <size>\0<payload>"`, hashed with SHA-1,
 * compressed with zlib, and persisted under `.mygit/objects/<2-hex>/<38-hex>`.
 */
class ObjectStore {
public:
    explicit ObjectStore(fs::path objectsDir);

    /** @brief Computes the SHA-1 hash of an object without writing to disk. */
    static std::string computeHash(const std::string& type, const std::string& data);

    /** @brief Stores an object (zlib-compressed) and returns its 40-char SHA-1 hash. */
    std::string store(const std::string& type, const std::string& data) const;

    /**
     * @brief Resolves a full 40-char hash or an unambiguous prefix (>= 4 hex chars).
     * @return Full 40-char hash, or std::nullopt if missing or ambiguous.
     */
    std::optional<std::string> resolveHash(const std::string& hashOrPrefix) const;

    /** @brief Reads and parses a stored object by full or abbreviated hash. */
    std::optional<GitObject> read(const std::string& hashOrPrefix) const;

    /** @brief Returns true if an object matching the hash/prefix exists. */
    bool exists(const std::string& hashOrPrefix) const;

    /** @brief Counts total loose objects stored on disk. */
    std::size_t objectCount() const;

    /** @brief Serializes tree entries in deterministic sorted order. */
    static std::string serializeTree(std::vector<TreeEntry> entries);

    /** @brief Parses serialized tree payload into structured entries. */
    static std::vector<TreeEntry> parseTree(const std::string& treeData);

private:
    fs::path objectsDir_;

    fs::path objectPath(const std::string& fullHash) const;
    static bool isHexString(const std::string& value);
};

}  // namespace mygit

#endif  // MYGIT_OBJECT_STORE_HPP
