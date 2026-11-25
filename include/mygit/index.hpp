/**
 * @file index.hpp
 * @brief Staging area (.mygit/index) reader and writer.
 * @author Rajath Gadagkar <rajathgadagkar@gmail.com>
 */

#ifndef MYGIT_INDEX_HPP
#define MYGIT_INDEX_HPP

#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace mygit {

namespace fs = std::filesystem;

/**
 * @brief Represents the MyGit staging area mapping normalized relative file
 *        paths to their staged 40-character blob SHA-1 hashes.
 */
class Index {
public:
    explicit Index(fs::path indexPath);

    /** @brief Loads index entries from disk (empty map if file does not exist). */
    bool load();

    /** @brief Persists current index entries to disk in sorted path order. */
    bool save() const;

    /** @brief Adds or updates a staged file path and its blob SHA-1 hash. */
    void stage(const std::string& normalizedPath, const std::string& blobHash);

    /** @brief Removes a file path from the staging area. */
    bool remove(const std::string& normalizedPath);

    /** @brief Clears all entries from the in-memory index. */
    void clear();

    /** @brief Replaces all entries with a new path-to-hash map. */
    void replaceAll(std::map<std::string, std::string> newEntries);

    /** @brief Checks whether a normalized path is present in the index. */
    bool contains(const std::string& normalizedPath) const;

    /** @brief Retrieves the staged blob hash for a path if present. */
    std::optional<std::string> getHash(const std::string& normalizedPath) const;

    /** @brief Returns a read-only view of all staged entries. */
    const std::map<std::string, std::string>& entries() const;

    /** @brief Returns true if the staging area has no entries. */
    bool empty() const;

private:
    fs::path indexPath_;
    std::map<std::string, std::string> entries_;
};

}  // namespace mygit

#endif  // MYGIT_INDEX_HPP
