/**
 * @file repository.hpp
 * @brief High-level MyGit repository operations (plumbing & porcelain commands).
 * @author Rajath Gadagkar <rajathgadagkar@gmail.com>
 */

#ifndef MYGIT_REPOSITORY_HPP
#define MYGIT_REPOSITORY_HPP

#include "mygit/index.hpp"
#include "mygit/object_store.hpp"

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace mygit {

namespace fs = std::filesystem;

/** @brief Parsed representation of a commit object. */
struct CommitData {
    std::string hash;
    std::string treeHash;
    std::string parentHash;
    std::string author;
    std::string committer;
    std::string message;
};

/** @brief Categorized working-tree and staging-area status snapshot. */
struct StatusReport {
    std::vector<std::pair<char, std::string>> staged;    ///< 'A', 'M', 'D' vs HEAD
    std::vector<std::pair<char, std::string>> unstaged;  ///< 'M', 'D' vs index
    std::vector<std::string> untracked;                  ///< Files not in index
};

/**
 * @brief Core controller for a MyGit repository rooted at a working directory.
 *
 * Implements both plumbing commands (`hash-object`, `cat-file`, `write-tree`,
 * `ls-tree`) and porcelain commands (`init`, `add`, `commit`, `status`, `log`,
 * `show`, `diff`, `checkout`).
 */
class Repository {
public:
    explicit Repository(fs::path rootDir = fs::current_path());

    /** @brief Returns true if `.mygit` exists in this repository root. */
    bool isInitialized() const;

    /** @brief Initializes `.mygit` directory structure, HEAD, refs, and index. */
    bool init();

    // --- Plumbing Commands ---

    /** @brief Hashes a file as a blob object, optionally persisting it to objects/. */
    std::string hashObject(const std::string& filepath, bool write = false);

    /** @brief Inspects a stored object (-p content, -s size, -t type). */
    bool catFile(const std::string& flag, const std::string& hashOrPrefix) const;

    /** @brief Recursively writes a tree object from a working directory path. */
    std::string writeTree(const fs::path& dirPath = "") const;

    /** @brief Builds a hierarchical tree object directly from staged index entries. */
    std::string writeTreeFromIndex(const std::map<std::string, std::string>& indexEntries) const;

    /** @brief Lists entries of a tree object (detailed or --name-only). */
    bool lsTree(const std::string& treeHashOrPrefix, bool nameOnly = false) const;

    // --- Porcelain Commands ---

    /** @brief Stages one or more files, directories, or "." into `.mygit/index`. */
    bool add(const std::vector<std::string>& paths);

    /** @brief Creates a commit from the staged index and advances HEAD. */
    std::string commit(const std::string& message);

    /** @brief Displays commit history walking backward from HEAD. */
    bool log(std::size_t maxCount = 0) const;

    /** @brief Displays metadata and tree entries for a specific commit. */
    bool show(const std::string& commitHashOrPrefix) const;

    /** @brief Computes structured repository status (staged, unstaged, untracked). */
    StatusReport computeStatus() const;

    /** @brief Prints formatted repository status to stdout. */
    bool status() const;

    /** @brief Prints unified diffs for modified working tree files vs index. */
    bool diff() const;

    /** @brief Restores working tree and index to match a target commit snapshot. */
    bool checkout(const std::string& commitHashOrPrefix);

    // --- Inspection Helpers (also used by unit tests) ---

    std::string getHeadCommit() const;
    std::optional<CommitData> readCommit(const std::string& hashOrPrefix) const;
    std::map<std::string, std::string> flattenTree(const std::string& treeHash,
                                                   const std::string& prefix = "") const;
    const ObjectStore& objectStore() const { return store_; }

private:
    fs::path rootDir_;
    fs::path gitDir_;
    fs::path objectsDir_;
    fs::path refsDir_;
    fs::path headsDir_;
    fs::path indexPath_;
    fs::path headPath_;
    ObjectStore store_;

    void updateHead(const std::string& commitHash) const;
    std::vector<std::string> loadIgnoreRules() const;
    std::string buildTreeNode(const std::map<std::string, std::string>& files,
                              const std::string& prefix) const;
    void restoreTreeToDisk(const std::string& treeHash, const fs::path& targetDir) const;
    static std::string resolveAuthorIdentity();
};

}  // namespace mygit

#endif  // MYGIT_REPOSITORY_HPP
