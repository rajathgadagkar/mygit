/**
 * @file ui.cpp
 * @brief TTY-aware ANSI terminal formatting and CLI output helpers.
 * @author Rajath Gadagkar <rajathgadagkar@gmail.com>
 */

#include "mygit/ui.hpp"

#include <cstdlib>
#include <iomanip>
#include <iostream>

#if defined(_WIN32)
#include <io.h>
#define MYGIT_ISATTY _isatty
#define MYGIT_STDOUT_FD _fileno(stdout)
#else
#include <unistd.h>
#define MYGIT_ISATTY isatty
#define MYGIT_STDOUT_FD STDOUT_FILENO
#endif

namespace mygit {

bool UI::colorEnabled_ = false;

void UI::initFromEnvironment() {
    const char* noColor = std::getenv("NO_COLOR");
    if (noColor != nullptr && noColor[0] != '\0') {
        colorEnabled_ = false;
        return;
    }
    colorEnabled_ = (MYGIT_ISATTY(MYGIT_STDOUT_FD) != 0);
}

void UI::setColorEnabled(bool enabled) {
    colorEnabled_ = enabled;
}

bool UI::isColorEnabled() {
    return colorEnabled_;
}

std::string UI::style(const std::string& code, const std::string& text) {
    if (!colorEnabled_) {
        return text;
    }
    return code + text + "\033[0m";
}

void UI::printHeader() {
    std::cout << '\n';
    std::cout << style(kBrightCyan, "  __  __        ____ _ _   ") << '\n';
    std::cout << style(kBrightCyan, " |  \\/  |_   _ / ___(_) |_ ") << '\n';
    std::cout << style(kBrightCyan, " | |\\/| | | | | |  _| | __|") << '\n';
    std::cout << style(kBrightCyan, " | |  | | |_| | |_| | | |_ ") << '\n';
    std::cout << style(kBrightCyan, " |_|  |_|\\__, |\\____|_|\\__|") << '\n';
    std::cout << style(kBrightCyan, "         |___/             ") << '\n';
    std::cout << style(kDim, "         Content-Addressable Version Control in C++17") << '\n';
    std::cout << '\n';
}

void UI::printSuccess(const std::string& message) {
    std::cout << style(kGreen, "✓ " + message) << '\n';
}

void UI::printError(const std::string& message) {
    std::cerr << style(kRed, "✗ Error: " + message) << '\n';
}

void UI::printWarning(const std::string& message) {
    std::cout << style(kYellow, "⚠ Warning: " + message) << '\n';
}

void UI::printInfo(const std::string& message) {
    std::cout << style(kBlue, "ℹ " + message) << '\n';
}

void UI::printProgress(const std::string& message) {
    std::cout << style(kMagenta, "◆ " + message) << '\n';
}

void UI::printSeparator() {
    std::cout << style(kDim, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━") << '\n';
}

void UI::printFileStatus(char status, const std::string& filename) {
    switch (status) {
        case 'A':
            std::cout << style(kGreen, "  A ") << filename << '\n';
            break;
        case 'M':
            std::cout << style(kYellow, "  M ") << filename << '\n';
            break;
        case 'D':
            std::cout << style(kRed, "  D ") << filename << '\n';
            break;
        case '?':
            std::cout << style(kBrightBlack, "  ? ") << filename << '\n';
            break;
        default:
            std::cout << "    " << filename << '\n';
            break;
    }
}

void UI::printTreeEntry(const std::string& mode,
                        const std::string& type,
                        const std::string& hash,
                        const std::string& name) {
    std::cout << mode << ' ' << std::left << std::setw(4) << type << ' '
              << style(kBrightYellow, hash) << '\t'
              << (type == "tree" ? style(kBlue, name + "/") : name) << '\n';
}

}  // namespace mygit
