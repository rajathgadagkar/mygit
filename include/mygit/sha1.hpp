/**
 * @file sha1.hpp
 * @brief RFC 3174 SHA-1 cryptographic hash implementation.
 * @author Rajath Gadagkar <rajathgadagkar@gmail.com>
 */

#ifndef MYGIT_SHA1_HPP
#define MYGIT_SHA1_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace mygit {

/**
 * @brief Streaming SHA-1 message digest calculator (RFC 3174).
 *
 * Produces lowercase 40-character hexadecimal SHA-1 digests compatible with
 * Git's object addressing format (`<type> <size>\0<content>`).
 */
class SHA1 {
public:
    SHA1();

    /** @brief Feeds a std::string into the running hash state. */
    void update(const std::string& input);

    /** @brief Feeds raw bytes into the running hash state. */
    void update(const char* data, std::size_t length);

    /** @brief Finalizes the digest, resets internal state, and returns 40-char hex. */
    std::string finalHex();

    /** @brief Convenience helper to compute SHA-1 directly from a string. */
    static std::string hashString(const std::string& input);

    /** @brief Convenience helper to compute SHA-1 directly from a file on disk. */
    static std::string hashFile(const std::string& filepath);

private:
    static constexpr std::size_t kDigestWords = 5;
    static constexpr std::size_t kBlockWords = 16;
    static constexpr std::size_t kBlockBytes = kBlockWords * 4;

    std::array<std::uint32_t, kDigestWords> digest_{};
    std::string buffer_;
    std::uint64_t transforms_ = 0;

    void reset();
    void transform(const std::array<std::uint32_t, kBlockWords>& block);
    static std::array<std::uint32_t, kBlockWords> bufferToBlock(const std::string& buf);
    static std::uint32_t rotateLeft(std::uint32_t value, std::size_t bits);
};

}  // namespace mygit

#endif  // MYGIT_SHA1_HPP
