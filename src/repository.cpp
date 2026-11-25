/**
 * @file repository.cpp
 * @brief High-level MyGit repository operations (plumbing & porcelain commands).
 * @author Rajath Gadagkar <rajathgadagkar@gmail.com>
 */

#include "mygit/repository.hpp"

#include "mygit/diff.hpp"
#include "mygit/fs_utils.hpp"
#include "mygit/ui.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>
#include <utility>

namespace mygit {

Repository::Repository(fs::path rootDir)
    : rootDir_(std::move(rootDir)),
      gitDir_(rootDir_ / ".mygit"),
      objectsDir_(gitDir_ / "objects"),
      refsDir_(gitDir_ / "refs"),
      headsDir_(refsDir_ / "heads"),
      indexPath_(gitDir_ / "index"),
      headPath_(gitDir_ / "HEAD"),
      store_(objectsDir_) {}

bool Repository::isInitialized() const {
    return FsUtils::directoryExists(gitDir_) && FsUtils::directoryExists(objectsDir_);
}

bool Repository::init() {
    if (isInitialized()) {
        UI::printWarning("Reinitialized existing MyGit repository in " + gitDir_.string());
        return true;
    }

    if (!FsUtils::createDirectories(objectsDir_) || !FsUtils::createDirectories(headsDir_)) {
        UI::printError("Failed to create .mygit directory structure");
        return false;
    }

    if (!FsUtils::writeFile(headPath_, "") || !FsUtils::writeFile(indexPath_, "")) {
        UI::printError("Failed to initialize HEAD and index metadata files");
        return false;
    }

    // Create a sensible default .mygitignore only if one does not already exist
    const fs::path ignoreFile = rootDir_ / ".mygitignore";
    if (!FsUtils::fileExists(ignoreFile)) {
        const std::string defaultIgnore =
            "# MyGit ignore rules\n"
            "*.o\n"
            "*.exe\n"
            ".DS_Store\n"
            "build/\n";
        FsUtils::writeFile(ignoreFile, defaultIgnore);
    }

    return true;
}

std::vector<std::string> Repository::loadIgnoreRules() const {
    return FsUtils::loadIgnorePatterns(rootDir_ / ".mygitignore");
}

std::string Repository::resolveAuthorIdentity() {
    const char* envName = std::getenv("MYGIT_AUTHOR_NAME");
    const char* envEmail = std::getenv("MYGIT_AUTHOR_EMAIL");
    const std::string name = (envName != nullptr && envName[0] != '\0') ? envName : "Rajath Gadagkar";
    const std::string email =
        (envEmail != nullptr && envEmail[0] != '\0') ? envEmail : "rajathgadagkar@gmail.com";
    return name + " <" + email + ">";
}

std::string Repository::hashObject(const std::string& filepath, bool write) {
    const fs::path target = fs::path(filepath).is_absolute() ? fs::path(filepath) : (rootDir_ / filepath);
    if (!FsUtils::fileExists(target)) {
        UI::printError("File not found: " + filepath);
        return "";
    }

    auto content = FsUtils::readFile(target);
    if (!content.has_value()) {
        UI::printError("Failed to read file: " + filepath);
        return "";
    }

    if (write) {
        if (!isInitialized()) {
            UI::printError("Not a MyGit repository (run 'mygit init' first)");
            return "";
        }
        return store_.store("blob", *content);
    }
    return ObjectStore::computeHash("blob", *content);
}

bool Repository::catFile(const std::string& flag, const std::string& hashOrPrefix) const {
    if (!isInitialized()) {
        UI::printError("Not a MyGit repository (run 'mygit init' first)");
        return false;
    }

    auto obj = store_.read(hashOrPrefix);
    if (!obj.has_value()) {
        UI::printError("Object not found or ambiguous hash: " + hashOrPrefix);
        return false;
    }

    if (flag == "-t") {
        std::cout << obj->type << '\n';
        return true;
    }
    if (flag == "-s") {
        std::cout << obj->size << '\n';
        return true;
    }
    if (flag == "-p") {
        if (obj->type == "tree") {
            const auto entries = ObjectStore::parseTree(obj->data);
            for (const auto& entry : entries) {
                UI::printTreeEntry(entry.mode, entry.type, entry.hash, entry.name);
            }
        } else {
            std::cout.write(obj->data.data(), static_cast<std::streamsize>(obj->data.size()));
        }
        return true;
    }

    UI::printError("Unknown cat-file flag: " + flag + " (expected -p, -s, or -t)");
    return false;
}

std::string Repository::writeTree(const fs::path& dirPath) const {
    if (!isInitialized()) {
        UI::printError("Not a MyGit repository (run 'mygit init' first)");
        return "";
    }

    const fs::path currentDir = dirPath.empty() ? rootDir_ : dirPath;
    const auto ignoreRules = loadIgnoreRules();
    std::vector<TreeEntry> entries;

    std::error_code ec;
    for (const auto& dirEntry : fs::directory_iterator(currentDir, ec)) {
        const fs::path rel = fs::relative(dirEntry.path(), rootDir_, ec);
        if (ec) {
            continue;
        }
        const std::string normRel = FsUtils::normalizeRelativePath(rel);
        if (FsUtils::isIgnored(normRel, ignoreRules)) {
            continue;
        }

        const std::string name = dirEntry.path().filename().string();
        if (dirEntry.is_directory(ec)) {
            const std::string subHash = writeTree(dirEntry.path());
            if (!subHash.empty()) {
                entries.push_back({"040000", "tree", subHash, name});
            }
        } else if (dirEntry.is_regular_file(ec)) {
            auto content = FsUtils::readFile(dirEntry.path());
            if (content.has_value()) {
                const std::string blobHash = store_.store("blob", *content);
                if (!blobHash.empty()) {
                    entries.push_back({"100644", "blob", blobHash, name});
                }
            }
        }
    }

    const std::string payload = ObjectStore::serializeTree(std::move(entries));
    return store_.store("tree", payload);
}

std::string Repository::buildTreeNode(const std::map<std::string, std::string>& files,
                                      const std::string& prefix) const {
    std::vector<TreeEntry> entries;
    std::set<std::string> subDirs;

    for (const auto& [fullPath, blobHash] : files) {
        if (!prefix.empty()) {
            if (fullPath.rfind(prefix + "/", 0) != 0) {
                continue;
            }
        }

        const std::string remainder =
            prefix.empty() ? fullPath : fullPath.substr(prefix.size() + 1);
        const std::size_t slashPos = remainder.find('/');
        if (slashPos == std::string::npos) {
            entries.push_back({"100644", "blob", blobHash, remainder});
        } else {
            subDirs.insert(remainder.substr(0, slashPos));
        }
    }

    for (const auto& dirName : subDirs) {
        const std::string childPrefix = prefix.empty() ? dirName : (prefix + "/" + dirName);
        const std::string childTreeHash = buildTreeNode(files, childPrefix);
        if (!childTreeHash.empty()) {
            entries.push_back({"040000", "tree", childTreeHash, dirName});
        }
    }

    const std::string payload = ObjectStore::serializeTree(std::move(entries));
    return store_.store("tree", payload);
}

std::string Repository::writeTreeFromIndex(
    const std::map<std::string, std::string>& indexEntries) const {
    if (!isInitialized()) {
        UI::printError("Not a MyGit repository (run 'mygit init' first)");
        return "";
    }
    return buildTreeNode(indexEntries, "");
}

bool Repository::lsTree(const std::string& treeHashOrPrefix, bool nameOnly) const {
    if (!isInitialized()) {
        UI::printError("Not a MyGit repository (run 'mygit init' first)");
        return false;
    }

    auto obj = store_.read(treeHashOrPrefix);
    if (!obj.has_value()) {
        UI::printError("Tree object not found: " + treeHashOrPrefix);
        return false;
    }
    if (obj->type != "tree") {
        UI::printError("Object " + treeHashOrPrefix + " is a " + obj->type + ", not a tree");
        return false;
    }

    const auto entries = ObjectStore::parseTree(obj->data);
    for (const auto& entry : entries) {
        if (nameOnly) {
            std::cout << entry.name << '\n';
        } else {
            UI::printTreeEntry(entry.mode, entry.type, entry.hash, entry.name);
        }
    }
    return true;
}

bool Repository::add(const std::vector<std::string>& paths) {
    if (!isInitialized()) {
        UI::printError("Not a MyGit repository (run 'mygit init' first)");
        return false;
    }
    if (paths.empty()) {
        UI::printError("Nothing specified, nothing added");
        return false;
    }

    Index index(indexPath_);
    index.load();
    const auto ignoreRules = loadIgnoreRules();

    std::size_t stagedCount = 0;

    auto stageSingleFile = [&](const fs::path& fullPath, const std::string& normRel) -> bool {
        auto content = FsUtils::readFile(fullPath);
        if (!content.has_value()) {
            UI::printError("Failed to read file: " + normRel);
            return false;
        }
        const std::string blobHash = store_.store("blob", *content);
        if (blobHash.empty()) {
            UI::printError("Failed to store blob for: " + normRel);
            return false;
        }
        index.stage(normRel, blobHash);
        ++stagedCount;
        return true;
    };

    for (const auto& rawPath : paths) {
        const fs::path target = (rawPath == ".") ? rootDir_ : (rootDir_ / rawPath);

        if (rawPath == "." || FsUtils::directoryExists(target)) {
            std::error_code ec;
            // Remove deleted files under target directory from index
            const std::string dirNorm =
                (rawPath == ".") ? "" : FsUtils::normalizeRelativePath(fs::relative(target, rootDir_, ec));
            std::vector<std::string> toRemove;
            for (const auto& [trackedPath, _] : index.entries()) {
                if (!dirNorm.empty() && trackedPath != dirNorm &&
                    trackedPath.rfind(dirNorm + "/", 0) != 0) {
                    continue;
                }
                if (!FsUtils::fileExists(rootDir_ / trackedPath)) {
                    toRemove.push_back(trackedPath);
                }
            }
            for (const auto& removedPath : toRemove) {
                index.remove(removedPath);
                ++stagedCount;
            }

            // Stage all non-ignored regular files under target directory
            for (const auto& dirEntry : fs::recursive_directory_iterator(target, ec)) {
                if (!dirEntry.is_regular_file(ec)) {
                    continue;
                }
                const fs::path rel = fs::relative(dirEntry.path(), rootDir_, ec);
                if (ec) {
                    continue;
                }
                const std::string normRel = FsUtils::normalizeRelativePath(rel);
                if (FsUtils::isIgnored(normRel, ignoreRules)) {
                    continue;
                }
                if (!stageSingleFile(dirEntry.path(), normRel)) {
                    return false;
                }
            }
        } else if (FsUtils::fileExists(target)) {
            std::error_code ec;
            const fs::path rel = fs::relative(target, rootDir_, ec);
            const std::string normRel = FsUtils::normalizeRelativePath(rel);
            if (!stageSingleFile(target, normRel)) {
                return false;
            }
            UI::printSuccess("Staged: " + normRel);
        } else {
            // Allow staging a file deletion if it was previously tracked in the index
            const std::string normRel = FsUtils::normalizeRelativePath(rawPath);
            if (index.contains(normRel)) {
                index.remove(normRel);
                ++stagedCount;
                UI::printSuccess("Staged deletion: " + normRel);
            } else {
                UI::printError("Pathspec '" + rawPath + "' did not match any files");
                return false;
            }
        }
    }

    if (!index.save()) {
        UI::printError("Failed to write staging index");
        return false;
    }

    if (stagedCount > 0) {
        UI::printInfo(std::to_string(stagedCount) + " path(s) updated in staging area");
    }
    return true;
}

std::string Repository::commit(const std::string& message) {
    if (!isInitialized()) {
        UI::printError("Not a MyGit repository (run 'mygit init' first)");
        return "";
    }

    const std::string trimmedMsg = FsUtils::trim(message);
    if (trimmedMsg.empty()) {
        UI::printError("Aborting commit due to empty commit message");
        return "";
    }

    Index index(indexPath_);
    index.load();

    if (index.empty()) {
        UI::printError("Nothing staged to commit (use 'mygit add <files>' first)");
        return "";
    }

    const std::string treeHash = writeTreeFromIndex(index.entries());
    if (treeHash.empty()) {
        UI::printError("Failed to write tree from staging index");
        return "";
    }

    const std::string parentHash = getHeadCommit();
    const std::string identity = resolveAuthorIdentity();
    const std::string timestamp = FsUtils::currentTimestamp();

    std::ostringstream commitBody;
    commitBody << "tree " << treeHash << '\n';
    if (!parentHash.empty()) {
        commitBody << "parent " << parentHash << '\n';
    }
    commitBody << "author " << identity << ' ' << timestamp << '\n';
    commitBody << "committer " << identity << ' ' << timestamp << '\n';
    commitBody << '\n';
    commitBody << trimmedMsg << '\n';

    const std::string commitHash = store_.store("commit", commitBody.str());
    if (commitHash.empty()) {
        UI::printError("Failed to write commit object");
        return "";
    }

    updateHead(commitHash);
    return commitHash;
}

std::string Repository::getHeadCommit() const {
    auto content = FsUtils::readFile(headPath_);
    if (!content.has_value()) {
        return "";
    }
    return FsUtils::trim(*content);
}

void Repository::updateHead(const std::string& commitHash) const {
    FsUtils::writeFile(headPath_, commitHash + "\n");
    FsUtils::writeFile(headsDir_ / "master", commitHash + "\n");
}

std::optional<CommitData> Repository::readCommit(const std::string& hashOrPrefix) const {
    auto resolved = store_.resolveHash(hashOrPrefix);
    if (!resolved.has_value()) {
        return std::nullopt;
    }

    auto obj = store_.read(*resolved);
    if (!obj.has_value() || obj->type != "commit") {
        return std::nullopt;
    }

    CommitData info;
    info.hash = *resolved;

    std::istringstream iss(obj->data);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) {
            std::ostringstream msgStream;
            std::string msgLine;
            bool firstLine = true;
            while (std::getline(iss, msgLine)) {
                if (!firstLine) {
                    msgStream << '\n';
                }
                msgStream << msgLine;
                firstLine = false;
            }
            info.message = msgStream.str();
            break;
        }
        if (line.rfind("tree ", 0) == 0) {
            info.treeHash = FsUtils::trim(line.substr(5));
        } else if (line.rfind("parent ", 0) == 0) {
            info.parentHash = FsUtils::trim(line.substr(7));
        } else if (line.rfind("author ", 0) == 0) {
            info.author = FsUtils::trim(line.substr(7));
        } else if (line.rfind("committer ", 0) == 0) {
            info.committer = FsUtils::trim(line.substr(10));
        }
    }

    return info;
}

std::map<std::string, std::string> Repository::flattenTree(const std::string& treeHash,
                                                           const std::string& prefix) const {
    std::map<std::string, std::string> result;
    if (treeHash.empty()) {
        return result;
    }

    auto obj = store_.read(treeHash);
    if (!obj.has_value() || obj->type != "tree") {
        return result;
    }

    for (const auto& entry : ObjectStore::parseTree(obj->data)) {
        const std::string fullPath = prefix.empty() ? entry.name : (prefix + "/" + entry.name);
        if (entry.type == "tree") {
            auto sub = flattenTree(entry.hash, fullPath);
            result.insert(sub.begin(), sub.end());
        } else {
            result[fullPath] = entry.hash;
        }
    }
    return result;
}

bool Repository::log(std::size_t maxCount) const {
    if (!isInitialized()) {
        UI::printError("Not a MyGit repository (run 'mygit init' first)");
        return false;
    }

    std::string currentHash = getHeadCommit();
    if (currentHash.empty()) {
        UI::printInfo("No commits yet on branch master");
        return true;
    }

    UI::printSeparator();
    std::size_t count = 0;
    bool first = true;

    while (!currentHash.empty() && (maxCount == 0 || count < maxCount)) {
        auto commitOpt = readCommit(currentHash);
        if (!commitOpt.has_value()) {
            break;
        }

        if (!first) {
            std::cout << UI::style(UI::kYellow, "│") << '\n';
        }

        std::cout << UI::style(UI::kYellow, "● ")
                  << UI::style(UI::kBrightYellow, "commit " + commitOpt->hash);
        if (first) {
            std::cout << ' ' << UI::style(UI::kBrightCyan, "(HEAD -> master)");
        }
        std::cout << '\n';

        if (!commitOpt->parentHash.empty()) {
            std::cout << UI::style(UI::kYellow, "│") << ' '
                      << UI::style(UI::kDim, "Parent: " + commitOpt->parentHash.substr(0, 7)) << '\n';
        }
        std::cout << UI::style(UI::kYellow, "│") << " Author: "
                  << UI::style(UI::kBrightWhite, commitOpt->author) << '\n';
        std::cout << UI::style(UI::kYellow, "│") << '\n';
        std::cout << UI::style(UI::kYellow, "│") << "     " << commitOpt->message << '\n';

        currentHash = commitOpt->parentHash;
        first = false;
        ++count;
    }

    std::cout << '\n';
    UI::printSeparator();
    return true;
}

bool Repository::show(const std::string& commitHashOrPrefix) const {
    if (!isInitialized()) {
        UI::printError("Not a MyGit repository (run 'mygit init' first)");
        return false;
    }

    auto commitOpt = readCommit(commitHashOrPrefix);
    if (!commitOpt.has_value()) {
        UI::printError("Commit not found: " + commitHashOrPrefix);
        return false;
    }

    std::cout << UI::style(UI::kBrightYellow, "commit " + commitOpt->hash) << '\n';
    std::cout << UI::style(UI::kDim, "tree " + commitOpt->treeHash) << '\n';
    if (!commitOpt->parentHash.empty()) {
        std::cout << UI::style(UI::kDim, "parent " + commitOpt->parentHash) << '\n';
    }
    std::cout << "Author: " << commitOpt->author << '\n';
    std::cout << '\n' << "    " << commitOpt->message << "\n\n";

    UI::printSeparator();
    std::cout << UI::style(UI::kBold, "Snapshot entries:") << '\n';
    return lsTree(commitOpt->treeHash, false);
}

StatusReport Repository::computeStatus() const {
    StatusReport report;

    // 1. Load HEAD tree snapshot (empty if no commits yet)
    std::map<std::string, std::string> headFiles;
    const std::string headHash = getHeadCommit();
    if (!headHash.empty()) {
        auto headCommit = readCommit(headHash);
        if (headCommit.has_value()) {
            headFiles = flattenTree(headCommit->treeHash);
        }
    }

    // 2. Load staging index
    Index index(indexPath_);
    index.load();
    const auto& indexFiles = index.entries();

    // 3. Staged changes: compare HEAD vs Index
    for (const auto& [path, idxHash] : indexFiles) {
        auto it = headFiles.find(path);
        if (it == headFiles.end()) {
            report.staged.emplace_back('A', path);
        } else if (it->second != idxHash) {
            report.staged.emplace_back('M', path);
        }
    }
    for (const auto& [path, _] : headFiles) {
        if (indexFiles.find(path) == indexFiles.end()) {
            report.staged.emplace_back('D', path);
        }
    }

    // 4. Unstaged changes: compare Index vs Working Tree
    for (const auto& [path, idxHash] : indexFiles) {
        const fs::path fullPath = rootDir_ / path;
        if (!FsUtils::fileExists(fullPath)) {
            report.unstaged.emplace_back('D', path);
            continue;
        }
        auto content = FsUtils::readFile(fullPath);
        if (!content.has_value()) {
            continue;
        }
        const std::string workHash = ObjectStore::computeHash("blob", *content);
        if (workHash != idxHash) {
            report.unstaged.emplace_back('M', path);
        }
    }

    // 5. Untracked files: Working Tree files not in Index and not ignored
    const auto ignoreRules = loadIgnoreRules();
    std::error_code ec;
    for (const auto& dirEntry : fs::recursive_directory_iterator(rootDir_, ec)) {
        if (!dirEntry.is_regular_file(ec)) {
            continue;
        }
        const fs::path rel = fs::relative(dirEntry.path(), rootDir_, ec);
        if (ec) {
            continue;
        }
        const std::string normRel = FsUtils::normalizeRelativePath(rel);
        if (FsUtils::isIgnored(normRel, ignoreRules)) {
            continue;
        }
        if (indexFiles.find(normRel) == indexFiles.end()) {
            report.untracked.push_back(normRel);
        }
    }
    std::sort(report.untracked.begin(), report.untracked.end());

    return report;
}

bool Repository::status() const {
    if (!isInitialized()) {
        UI::printError("Not a MyGit repository (run 'mygit init' first)");
        return false;
    }

    std::cout << UI::style(UI::kBold, "On branch ") << UI::style(UI::kGreen, "master") << '\n';
    if (getHeadCommit().empty()) {
        std::cout << "\nNo commits yet\n";
    }
    std::cout << '\n';

    const StatusReport report = computeStatus();

    if (!report.staged.empty()) {
        std::cout << UI::style(UI::kGreen, "Changes to be committed:") << '\n';
        for (const auto& [code, path] : report.staged) {
            UI::printFileStatus(code, path);
        }
        std::cout << '\n';
    }

    if (!report.unstaged.empty()) {
        std::cout << UI::style(UI::kRed, "Changes not staged for commit:") << '\n';
        for (const auto& [code, path] : report.unstaged) {
            UI::printFileStatus(code, path);
        }
        std::cout << '\n';
    }

    if (!report.untracked.empty()) {
        std::cout << "Untracked files:\n";
        for (const auto& path : report.untracked) {
            UI::printFileStatus('?', path);
        }
        std::cout << '\n';
    }

    if (report.staged.empty() && report.unstaged.empty() && report.untracked.empty()) {
        std::cout << "nothing to commit, working tree clean\n";
    }
    return true;
}

bool Repository::diff() const {
    if (!isInitialized()) {
        UI::printError("Not a MyGit repository (run 'mygit init' first)");
        return false;
    }

    Index index(indexPath_);
    index.load();

    bool anyChanges = false;
    for (const auto& [path, idxHash] : index.entries()) {
        const fs::path fullPath = rootDir_ / path;
        std::string newContent;
        if (FsUtils::fileExists(fullPath)) {
            auto readOpt = FsUtils::readFile(fullPath);
            if (!readOpt.has_value()) {
                continue;
            }
            newContent = *readOpt;
        }

        auto blobObj = store_.read(idxHash);
        const std::string oldContent = blobObj.has_value() ? blobObj->data : "";

        if (oldContent != newContent) {
            anyChanges = true;
            std::cout << Diff::formatUnified(path, oldContent, newContent, UI::isColorEnabled());
        }
    }

    if (!anyChanges) {
        UI::printInfo("No unstaged changes detected");
    }
    return true;
}

void Repository::restoreTreeToDisk(const std::string& treeHash, const fs::path& targetDir) const {
    auto obj = store_.read(treeHash);
    if (!obj.has_value() || obj->type != "tree") {
        return;
    }

    for (const auto& entry : ObjectStore::parseTree(obj->data)) {
        const fs::path dest = targetDir / entry.name;
        if (entry.type == "tree") {
            FsUtils::createDirectories(dest);
            restoreTreeToDisk(entry.hash, dest);
        } else {
            auto blob = store_.read(entry.hash);
            if (blob.has_value() && blob->type == "blob") {
                FsUtils::writeFile(dest, blob->data);
            }
        }
    }
}

bool Repository::checkout(const std::string& commitHashOrPrefix) {
    if (!isInitialized()) {
        UI::printError("Not a MyGit repository (run 'mygit init' first)");
        return false;
    }

    auto commitOpt = readCommit(commitHashOrPrefix);
    if (!commitOpt.has_value()) {
        UI::printError("Commit not found or invalid commit hash: " + commitHashOrPrefix);
        return false;
    }

    // Collect currently tracked files (from index + current HEAD) so we cleanly
    // remove files that do not exist in the target commit without touching ignored files
    std::set<std::string> trackedToClean;
    Index index(indexPath_);
    index.load();
    for (const auto& [path, _] : index.entries()) {
        trackedToClean.insert(path);
    }
    const std::string currentHead = getHeadCommit();
    if (!currentHead.empty()) {
        auto currentCommit = readCommit(currentHead);
        if (currentCommit.has_value()) {
            for (const auto& [path, _] : flattenTree(currentCommit->treeHash)) {
                trackedToClean.insert(path);
            }
        }
    }

    const auto targetFiles = flattenTree(commitOpt->treeHash);

    // Remove tracked files that are absent in the target snapshot
    std::error_code ec;
    for (const auto& oldPath : trackedToClean) {
        if (targetFiles.find(oldPath) == targetFiles.end()) {
            const fs::path fullPath = rootDir_ / oldPath;
            fs::remove(fullPath, ec);

            // Prune now-empty parent directories up to rootDir_
            fs::path parent = fullPath.parent_path();
            while (!parent.empty() && parent != rootDir_) {
                if (fs::is_empty(parent, ec)) {
                    fs::remove(parent, ec);
                    parent = parent.parent_path();
                } else {
                    break;
                }
            }
        }
    }

    // Materialize target commit tree to working directory
    restoreTreeToDisk(commitOpt->treeHash, rootDir_);

    // Synchronize staging index and HEAD to target commit
    index.replaceAll(targetFiles);
    index.save();
    updateHead(commitOpt->hash);

    UI::printSuccess("HEAD is now at " + commitOpt->hash.substr(0, 7) + " — " + commitOpt->message);
    return true;
}

}  // namespace mygit
