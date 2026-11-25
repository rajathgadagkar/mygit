/**
 * @file ui.hpp
 * @brief TTY-aware ANSI terminal formatting and CLI output helpers.
 * @author Rajath Gadagkar <rajathgadagkar@gmail.com>
 */

#ifndef MYGIT_UI_HPP
#define MYGIT_UI_HPP

#include <string>

namespace mygit {

/**
 * @brief Provides styled CLI output that automatically disables ANSI escape
 *        codes when stdout is piped/redirected or when NO_COLOR is set.
 */
class UI {
public:
    /** @brief Initializes color state from TTY detection and NO_COLOR env var. */
    static void initFromEnvironment();

    /** @brief Explicitly enables or disables ANSI color output. */
    static void setColorEnabled(bool enabled);

    /** @brief Returns true if ANSI color output is currently enabled. */
    static bool isColorEnabled();

    /** @brief Wraps text in an ANSI escape code if colors are enabled. */
    static std::string style(const std::string& code, const std::string& text);

    // Common ANSI SGR codes
    static constexpr const char* kBold = "\033[1m";
    static constexpr const char* kDim = "\033[2m";
    static constexpr const char* kRed = "\033[31m";
    static constexpr const char* kGreen = "\033[32m";
    static constexpr const char* kYellow = "\033[33m";
    static constexpr const char* kBlue = "\033[34m";
    static constexpr const char* kMagenta = "\033[35m";
    static constexpr const char* kCyan = "\033[36m";
    static constexpr const char* kBrightBlack = "\033[90m";
    static constexpr const char* kBrightGreen = "\033[92m";
    static constexpr const char* kBrightYellow = "\033[93m";
    static constexpr const char* kBrightCyan = "\033[96m";
    static constexpr const char* kBrightWhite = "\033[97m";

    static void printHeader();
    static void printSuccess(const std::string& message);
    static void printError(const std::string& message);
    static void printWarning(const std::string& message);
    static void printInfo(const std::string& message);
    static void printProgress(const std::string& message);
    static void printSeparator();
    static void printFileStatus(char status, const std::string& filename);
    static void printTreeEntry(const std::string& mode,
                               const std::string& type,
                               const std::string& hash,
                               const std::string& name);

private:
    static bool colorEnabled_;
};

}  // namespace mygit

#endif  // MYGIT_UI_HPP
