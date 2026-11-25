/**
 * @file unit_tests.cpp
 * @brief Self-contained C++17 unit and integration test suite for MyGit.
 * @author Rajath Gadagkar <rajathgadagkar@gmail.com>
 */

#include "mygit/compression.hpp"
#include "mygit/diff.hpp"
#include "mygit/fs_utils.hpp"
#include "mygit/index.hpp"
#include "mygit/object_store.hpp"
#include "mygit/repository.hpp"
#include "mygit/sha1.hpp"
#include "mygit/ui.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace mygit;

namespace {

int gPassed = 0;
int gFailed = 0;

void assertTrue(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error("Assertion failed: " + message);
    }
}

void assertEqual(const std::string& actual, const std::string& expected, const std::string& label) {
    if (actual != expected) {
        throw std::runtime_error(label + " mismatch:\n  expected: '" + expected +
                                 "'\n  actual:   '" + actual + "'");
    }
}

void runTest(const std::string& name, void (*fn)()) {
    try {
        fn();
        std::cout << "[PASS] " << name << '\n';
        ++gPassed;
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] " << name << " -> " << ex.what() << '\n';
        ++gFailed;
    }
}

fs::path createTempDir(const std::string& suffix) {
    const auto ts = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path dir = fs::temp_directory_path() / ("mygit_test_" + suffix + "_" + std::to_string(ts));
    fs::create_directories(dir);
    return dir;
}

void testSha1Vectors() {
    // Standard NIST / RFC 3174 vectors
    assertEqual(SHA1::hashString(""), "da39a3ee5e6b4b0d3255bfef95601890afd80709", "empty SHA1");
    assertEqual(SHA1::hashString("abc"), "a9993e364706816aba3e25717850c26c9cd0d89d", "abc SHA1");
    assertEqual(SHA1::hashString("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
                "84983e441c3bd26ebaae4aa1f95129e5e54670f1", "56-byte SHA1 (2-block spill)");
    assertEqual(SHA1::hashString("The quick brown fox jumps over the lazy dog"),
                "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12", "fox SHA1");

    // Git-compatible blob hash: "blob 12\0Hello World\n"
    const std::string blobHash = ObjectStore::computeHash("blob", "Hello World\n");
    assertEqual(blobHash, "557db03de997c86a4a028e1ebd3a1ceb225be238", "Git-compatible blob SHA1");
}

void testZlibRoundTrip() {
    const std::string sample = "Repeated data repeated data repeated data 1234567890!\n";
    auto compressed = Compression::deflateData(sample);
    assertTrue(compressed.has_value(), "deflate should succeed");
    assertTrue(compressed->size() < sample.size() + 32, "compressed payload should be compact");

    auto decompressed = Compression::inflateData(*compressed);
    assertTrue(decompressed.has_value(), "inflate should succeed");
    assertEqual(*decompressed, sample, "zlib round-trip");

    // Empty payload round-trip
    auto emptyComp = Compression::deflateData("");
    assertTrue(emptyComp.has_value(), "deflate empty should succeed");
    auto emptyDecomp = Compression::inflateData(*emptyComp);
    assertTrue(emptyDecomp.has_value(), "inflate empty should succeed");
    assertEqual(*emptyDecomp, "", "empty round-trip");

    // Invalid stream returns nullopt
    assertTrue(!Compression::inflateData("not-zlib-bytes").has_value(), "invalid zlib rejected");
}

void testPathNormalizationAndIgnore() {
    assertEqual(FsUtils::normalizeRelativePath("./src//main.cpp"), "src/main.cpp", "normalize ./");
    assertEqual(FsUtils::normalizeRelativePath("a/b/../c.txt"), "a/c.txt", "normalize ..");

    const std::vector<std::string> patterns = {"*.o", "build/", ".DS_Store"};
    assertTrue(FsUtils::isIgnored(".mygit/HEAD", patterns), ".mygit always ignored");
    assertTrue(FsUtils::isIgnored("src/main.o", patterns), "*.o ignored");
    assertTrue(FsUtils::isIgnored("build/app", patterns), "build/ ignored");
    assertTrue(!FsUtils::isIgnored("src/main.cpp", patterns), "source file not ignored");
}

void testObjectStoreAndTreeSerialization() {
    const fs::path tmp = createTempDir("objstore");
    ObjectStore store(tmp / "objects");

    const std::string binaryPayload("binary\0payload\xff", 15);
    const std::string blobHash = store.store("blob", binaryPayload);
    assertTrue(blobHash.size() == 40, "stored hash length == 40");

    // Resolve by short prefix
    auto resolved = store.resolveHash(blobHash.substr(0, 7));
    assertTrue(resolved.has_value(), "7-char prefix resolves");
    assertEqual(*resolved, blobHash, "resolved full hash");

    // Reject too-short or non-hex hashes safely (no crash)
    assertTrue(!store.resolveHash("a").has_value(), "1-char hash rejected safely");
    assertTrue(!store.resolveHash("zzzzzz").has_value(), "non-hex hash rejected safely");

    // Read binary blob back verbatim
    auto obj = store.read(blobHash.substr(0, 8));
    assertTrue(obj.has_value(), "read by short hash succeeds");
    assertEqual(obj->type, "blob", "object type");
    assertTrue(obj->size == binaryPayload.size(), "object size matches binary length");
    assertEqual(obj->data, binaryPayload, "binary data preserved");

    // Tree serialization & parsing (verifies NUL delimiter fix!)
    std::vector<TreeEntry> entries = {
        {"100644", "blob", blobHash, "zeta.txt"},
        {"100644", "blob", blobHash, "alpha.txt"},
        {"040000", "tree", blobHash, "subdir"},
    };
    const std::string rawTree = ObjectStore::serializeTree(entries);
    const std::string treeHash = store.store("tree", rawTree);
    auto treeObj = store.read(treeHash);
    assertTrue(treeObj.has_value() && treeObj->type == "tree", "tree stored and read");

    const auto parsed = ObjectStore::parseTree(treeObj->data);
    assertTrue(parsed.size() == 3, "parsed all 3 tree entries");
    assertEqual(parsed[0].name, "alpha.txt", "sorted entry 0");
    assertEqual(parsed[1].name, "subdir", "sorted entry 1");
    assertEqual(parsed[1].type, "tree", "directory type");
    assertEqual(parsed[2].name, "zeta.txt", "sorted entry 2");

    fs::remove_all(tmp);
}

void testUnifiedDiffEngine() {
    const std::string oldText = "line 1\nline 2\nline 3\n";
    const std::string newText = "line 1\nline 2 modified\nline 3\nline 4\n";

    const std::string diffOut = Diff::formatUnified("demo.txt", oldText, newText, false);
    assertTrue(diffOut.find("--- a/demo.txt") != std::string::npos, "contains old header");
    assertTrue(diffOut.find("+++ b/demo.txt") != std::string::npos, "contains new header");
    assertTrue(diffOut.find("-line 2") != std::string::npos, "contains removed line");
    assertTrue(diffOut.find("+line 2 modified") != std::string::npos, "contains added line");
    assertTrue(diffOut.find("+line 4") != std::string::npos, "contains appended line");

    assertEqual(Diff::formatUnified("same.txt", oldText, oldText, false), "", "no diff for equal");
}

void testFullRepositoryLifecycle() {
    const fs::path tmp = createTempDir("repo");
    Repository repo(tmp);

    assertTrue(repo.init(), "init succeeds");
    assertTrue(repo.isInitialized(), "repo initialized");

    // Create files including nested directory
    FsUtils::writeFile(tmp / "hello.txt", "Hello v1\n");
    FsUtils::writeFile(tmp / "nested/deep/data.txt", "Nested content\n");
    FsUtils::writeFile(tmp / "ignored.o", "object file");

    // Status before add: untracked files (excluding ignored.o)
    auto st0 = repo.computeStatus();
    assertTrue(st0.staged.empty(), "nothing staged initially");
    assertTrue(st0.untracked.size() >= 2, "untracked files detected");
    for (const auto& u : st0.untracked) {
        assertTrue(u != "ignored.o", "ignored.o must not appear in untracked");
    }

    // Stage and commit v1
    assertTrue(repo.add({"hello.txt", "nested/deep/data.txt"}), "stage files");
    auto st1 = repo.computeStatus();
    assertTrue(st1.staged.size() == 2, "2 files staged");

    const std::string c1 = repo.commit("Initial commit");
    assertTrue(c1.size() == 40, "commit 1 hash");

    // After commit, staged & unstaged should be clean
    auto st2 = repo.computeStatus();
    assertTrue(st2.staged.empty(), "staged empty after commit");
    assertTrue(st2.unstaged.empty(), "unstaged empty after commit");

    // Modify hello.txt and add new file for commit 2
    FsUtils::writeFile(tmp / "hello.txt", "Hello v2\n");
    FsUtils::writeFile(tmp / "extra.txt", "Extra file\n");

    auto st3 = repo.computeStatus();
    assertTrue(st3.unstaged.size() == 1 && st3.unstaged[0].first == 'M', "hello.txt modified");

    assertTrue(repo.add({"hello.txt", "extra.txt"}), "stage v2 changes");
    const std::string c2 = repo.commit("Second commit");
    assertTrue(c2.size() == 40 && c2 != c1, "commit 2 hash distinct");

    // Verify parent pointer
    auto c2Info = repo.readCommit(c2);
    assertTrue(c2Info.has_value(), "read commit 2");
    assertEqual(c2Info->parentHash, c1, "commit 2 parent is commit 1");

    // Checkout commit 1 using a 7-character short hash!
    assertTrue(repo.checkout(c1.substr(0, 7)), "checkout commit 1 via short hash");
    assertEqual(repo.getHeadCommit(), c1, "HEAD moved to commit 1");
    assertEqual(*FsUtils::readFile(tmp / "hello.txt"), "Hello v1\n", "hello.txt restored to v1");
    assertTrue(!FsUtils::fileExists(tmp / "extra.txt"), "extra.txt removed on checkout c1");
    assertTrue(FsUtils::fileExists(tmp / "nested/deep/data.txt"), "nested file preserved");

    // Status should be clean after checkout (index synced!)
    auto stAfterCheckout = repo.computeStatus();
    assertTrue(stAfterCheckout.staged.empty(), "staged clean after checkout");
    assertTrue(stAfterCheckout.unstaged.empty(), "unstaged clean after checkout");

    // Checkout back to commit 2
    assertTrue(repo.checkout(c2), "checkout commit 2");
    assertEqual(*FsUtils::readFile(tmp / "hello.txt"), "Hello v2\n", "hello.txt restored to v2");
    assertTrue(FsUtils::fileExists(tmp / "extra.txt"), "extra.txt restored on checkout c2");

    fs::remove_all(tmp);
}

}  // namespace

int main() {
    UI::setColorEnabled(false);

    std::cout << "=== MyGit C++17 Unit & Integration Test Suite ===\n";
    runTest("SHA-1 RFC 3174 & Git Blob Vectors", testSha1Vectors);
    runTest("Zlib Deflate/Inflate Round-Trip", testZlibRoundTrip);
    runTest("Path Normalization & .mygitignore Matching", testPathNormalizationAndIgnore);
    runTest("ObjectStore & Tree Serialization/Parsing", testObjectStoreAndTreeSerialization);
    runTest("LCS Unified Diff Engine", testUnifiedDiffEngine);
    runTest("Full Repository Lifecycle & Checkout", testFullRepositoryLifecycle);

    std::cout << "\nSummary: " << gPassed << " passed, " << gFailed << " failed.\n";
    return (gFailed == 0) ? 0 : 1;
}
