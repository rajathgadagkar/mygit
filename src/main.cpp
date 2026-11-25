/**
 * @file main.cpp
 * @brief Command-line entry point for MyGit.
 * @author Rajath Gadagkar <rajathgadagkar@gmail.com>
 */

#include "mygit/repository.hpp"
#include "mygit/ui.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr const char* kVersion = "2.0.0";

void printUsage() {
    using mygit::UI;
    UI::printHeader();

    std::cout << UI::style(UI::kBold, "USAGE:") << '\n';
    std::cout << "  mygit [--no-color] <command> [options]\n\n";

    std::cout << UI::style(UI::kBold, "PORCELAIN COMMANDS:") << '\n';
    std::cout << "  " << UI::style(UI::kCyan, "init") << "                         Initialize a new repository\n";
    std::cout << "  " << UI::style(UI::kCyan, "status") << "                       Show working tree & staging status\n";
    std::cout << "  " << UI::style(UI::kCyan, "add") << " <paths...>               Stage files, directories, or '.'\n";
    std::cout << "  " << UI::style(UI::kCyan, "commit") << " -m <message>          Record staged snapshot to history\n";
    std::cout << "  " << UI::style(UI::kCyan, "log") << " [-n <count>]             Show commit history\n";
    std::cout << "  " << UI::style(UI::kCyan, "show") << " <commit>                Inspect commit metadata and snapshot\n";
    std::cout << "  " << UI::style(UI::kCyan, "diff") << "                         Show unified diff (index vs working tree)\n";
    std::cout << "  " << UI::style(UI::kCyan, "checkout") << " <commit>            Restore working tree & index to commit\n\n";

    std::cout << UI::style(UI::kBold, "PLUMBING COMMANDS:") << '\n';
    std::cout << "  " << UI::style(UI::kCyan, "hash-object") << " [-w] <file>      Compute SHA-1 hash (optionally store)\n";
    std::cout << "  " << UI::style(UI::kCyan, "cat-file") << " <-p|-s|-t> <hash>   Inspect stored object content/size/type\n";
    std::cout << "  " << UI::style(UI::kCyan, "write-tree") << "                   Create tree object from working tree\n";
    std::cout << "  " << UI::style(UI::kCyan, "ls-tree") << " [--name-only] <hash> List entries of a tree object\n\n";

    std::cout << UI::style(UI::kBold, "OPTIONS:") << '\n';
    std::cout << "  -h, --help                   Show this help message\n";
    std::cout << "  -v, --version                Show version information\n";
    std::cout << "  --no-color                   Disable ANSI color output\n\n";

    UI::printSeparator();
    std::cout << UI::style(UI::kDim, "Author: Rajath Gadagkar <rajathgadagkar@gmail.com>") << '\n';
}

void printVersion() {
    std::cout << "mygit version " << kVersion << '\n';
    std::cout << "Built with C++17 & zlib — Author: Rajath Gadagkar <rajathgadagkar@gmail.com>\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    using mygit::Repository;
    using mygit::UI;

    UI::initFromEnvironment();

    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc));
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--no-color") {
            UI::setColorEnabled(false);
        } else {
            args.push_back(std::move(arg));
        }
    }

    if (args.empty()) {
        printUsage();
        return 1;
    }

    const std::string& command = args[0];

    if (command == "-h" || command == "--help" || command == "help") {
        printUsage();
        return 0;
    }
    if (command == "-v" || command == "--version" || command == "version") {
        printVersion();
        return 0;
    }

    Repository repo;

    if (command == "init") {
        const bool ok = repo.init();
        if (ok) {
            UI::printSuccess("Initialized MyGit repository in .mygit/");
        }
        return ok ? 0 : 1;
    }

    if (command == "status") {
        return repo.status() ? 0 : 1;
    }

    if (command == "hash-object") {
        bool write = false;
        std::string filename;
        if (args.size() == 3 && args[1] == "-w") {
            write = true;
            filename = args[2];
        } else if (args.size() == 2) {
            filename = args[1];
        } else {
            UI::printError("Usage: mygit hash-object [-w] <file>");
            return 1;
        }

        const std::string hash = repo.hashObject(filename, write);
        if (hash.empty()) {
            return 1;
        }
        std::cout << UI::style(UI::kBrightYellow, hash) << '\n';
        return 0;
    }

    if (command == "cat-file") {
        if (args.size() != 3) {
            UI::printError("Usage: mygit cat-file <-p|-s|-t> <hash>");
            return 1;
        }
        return repo.catFile(args[1], args[2]) ? 0 : 1;
    }

    if (command == "write-tree") {
        const std::string treeHash = repo.writeTree();
        if (treeHash.empty()) {
            return 1;
        }
        std::cout << UI::style(UI::kBrightYellow, treeHash) << '\n';
        return 0;
    }

    if (command == "ls-tree") {
        bool nameOnly = false;
        std::string treeHash;
        if (args.size() == 3 && args[1] == "--name-only") {
            nameOnly = true;
            treeHash = args[2];
        } else if (args.size() == 2) {
            treeHash = args[1];
        } else {
            UI::printError("Usage: mygit ls-tree [--name-only] <tree-hash>");
            return 1;
        }
        return repo.lsTree(treeHash, nameOnly) ? 0 : 1;
    }

    if (command == "add") {
        if (args.size() < 2) {
            UI::printError("Usage: mygit add <paths...>");
            return 1;
        }
        std::vector<std::string> files(args.begin() + 1, args.end());
        return repo.add(files) ? 0 : 1;
    }

    if (command == "commit") {
        if (args.size() < 3 || args[1] != "-m") {
            UI::printError("Usage: mygit commit -m <message>");
            return 1;
        }
        const std::string commitHash = repo.commit(args[2]);
        if (commitHash.empty()) {
            return 1;
        }
        UI::printSuccess("Committed " + commitHash.substr(0, 7));
        std::cout << UI::style(UI::kBrightYellow, commitHash) << '\n';
        return 0;
    }

    if (command == "log") {
        std::size_t maxCount = 0;
        if (args.size() == 3 && args[1] == "-n") {
            try {
                maxCount = static_cast<std::size_t>(std::stoull(args[2]));
            } catch (...) {
                UI::printError("Invalid count for -n: " + args[2]);
                return 1;
            }
        } else if (args.size() != 1) {
            UI::printError("Usage: mygit log [-n <count>]");
            return 1;
        }
        return repo.log(maxCount) ? 0 : 1;
    }

    if (command == "show") {
        if (args.size() != 2) {
            UI::printError("Usage: mygit show <commit-hash>");
            return 1;
        }
        return repo.show(args[1]) ? 0 : 1;
    }

    if (command == "diff") {
        return repo.diff() ? 0 : 1;
    }

    if (command == "checkout") {
        if (args.size() != 2) {
            UI::printError("Usage: mygit checkout <commit-hash>");
            return 1;
        }
        return repo.checkout(args[1]) ? 0 : 1;
    }

    UI::printError("Unknown command: " + command);
    std::cerr << "Run 'mygit --help' for usage information.\n";
    return 1;
}
