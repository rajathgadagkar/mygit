/**
 * @file sha1.cpp
 * @brief RFC 3174 SHA-1 cryptographic hash implementation.
 * @author Rajath Gadagkar <rajathgadagkar@gmail.com>
 */

#include "mygit/sha1.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace mygit {

namespace {

constexpr std::uint32_t f1(std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    return d ^ (b & (c ^ d));
}

constexpr std::uint32_t f2(std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    return b ^ c ^ d;
}

constexpr std::uint32_t f3(std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    return (b & c) | (d & (b | c));
}

}  // namespace

SHA1::SHA1() {
    reset();
}

void SHA1::reset() {
    digest_ = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    buffer_.clear();
    transforms_ = 0;
}

std::uint32_t SHA1::rotateLeft(std::uint32_t value, std::size_t bits) {
    return (value << bits) | (value >> (32u - bits));
}

std::array<std::uint32_t, SHA1::kBlockWords> SHA1::bufferToBlock(const std::string& buf) {
    std::array<std::uint32_t, kBlockWords> block{};
    for (std::size_t i = 0; i < kBlockWords; ++i) {
        const auto b0 = static_cast<std::uint32_t>(static_cast<std::uint8_t>(buf[4 * i + 0]));
        const auto b1 = static_cast<std::uint32_t>(static_cast<std::uint8_t>(buf[4 * i + 1]));
        const auto b2 = static_cast<std::uint32_t>(static_cast<std::uint8_t>(buf[4 * i + 2]));
        const auto b3 = static_cast<std::uint32_t>(static_cast<std::uint8_t>(buf[4 * i + 3]));
        block[i] = (b0 << 24u) | (b1 << 16u) | (b2 << 8u) | b3;
    }
    return block;
}

void SHA1::transform(const std::array<std::uint32_t, kBlockWords>& block) {
    std::array<std::uint32_t, 80> w{};
    for (std::size_t i = 0; i < kBlockWords; ++i) {
        w[i] = block[i];
    }
    for (std::size_t i = kBlockWords; i < 80; ++i) {
        w[i] = rotateLeft(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    std::uint32_t a = digest_[0];
    std::uint32_t b = digest_[1];
    std::uint32_t c = digest_[2];
    std::uint32_t d = digest_[3];
    std::uint32_t e = digest_[4];

    for (std::size_t i = 0; i < 80; ++i) {
        std::uint32_t f = 0;
        std::uint32_t k = 0;

        if (i < 20) {
            f = f1(b, c, d);
            k = 0x5A827999u;
        } else if (i < 40) {
            f = f2(b, c, d);
            k = 0x6ED9EBA1u;
        } else if (i < 60) {
            f = f3(b, c, d);
            k = 0x8F1BBCDCu;
        } else {
            f = f2(b, c, d);
            k = 0xCA62C1D6u;
        }

        const std::uint32_t temp = rotateLeft(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rotateLeft(b, 30);
        b = a;
        a = temp;
    }

    digest_[0] += a;
    digest_[1] += b;
    digest_[2] += c;
    digest_[3] += d;
    digest_[4] += e;

    ++transforms_;
}

void SHA1::update(const std::string& input) {
    update(input.data(), input.size());
}

void SHA1::update(const char* data, std::size_t length) {
    buffer_.append(data, length);

    while (buffer_.size() >= kBlockBytes) {
        const auto block = bufferToBlock(buffer_);
        transform(block);
        buffer_.erase(0, kBlockBytes);
    }
}

std::string SHA1::finalHex() {
    const std::uint64_t totalBits = (transforms_ * kBlockBytes + buffer_.size()) * 8u;

    // Append mandatory 0x80 terminator bit followed by zero padding
    buffer_.push_back(static_cast<char>(0x80));
    const std::size_t paddedSize = buffer_.size();
    while (buffer_.size() < kBlockBytes) {
        buffer_.push_back('\0');
    }

    auto block = bufferToBlock(buffer_);

    // If not enough room for the 64-bit length trailer, spill into a second block
    if (paddedSize > kBlockBytes - 8) {
        transform(block);
        block.fill(0);
    }

    block[kBlockWords - 2] = static_cast<std::uint32_t>(totalBits >> 32u);
    block[kBlockWords - 1] = static_cast<std::uint32_t>(totalBits & 0xFFFFFFFFu);
    transform(block);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::uint32_t word : digest_) {
        oss << std::setw(8) << word;
    }

    reset();
    return oss.str();
}

std::string SHA1::hashString(const std::string& input) {
    SHA1 hasher;
    hasher.update(input);
    return hasher.finalHex();
}

std::string SHA1::hashFile(const std::string& filepath) {
    std::ifstream stream(filepath, std::ios::binary);
    if (!stream) {
        return "";
    }

    SHA1 hasher;
    char chunk[4096];
    while (stream.read(chunk, sizeof(chunk))) {
        hasher.update(chunk, sizeof(chunk));
    }
    if (stream.gcount() > 0) {
        hasher.update(chunk, static_cast<std::size_t>(stream.gcount()));
    }
    return hasher.finalHex();
}

}  // namespace mygit
