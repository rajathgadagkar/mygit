/**
 * @file compression.hpp
 * @brief Zlib deflate/inflate helpers for Git object storage.
 * @author Rajath Gadagkar <rajathgadagkar@gmail.com>
 */

#ifndef MYGIT_COMPRESSION_HPP
#define MYGIT_COMPRESSION_HPP

#include <optional>
#include <string>

namespace mygit {

/**
 * @brief Stateless wrappers around zlib stream compression and decompression.
 */
class Compression {
public:
    /**
     * @brief Compresses raw bytes using zlib deflate (default compression level).
     * @param input Uncompressed data.
     * @return Compressed bytes, or std::nullopt on zlib failure.
     */
    static std::optional<std::string> deflateData(const std::string& input);

    /**
     * @brief Decompresses zlib-compressed bytes using inflate.
     * @param input Zlib-compressed byte stream.
     * @return Decompressed bytes, or std::nullopt if invalid or corrupt.
     */
    static std::optional<std::string> inflateData(const std::string& input);
};

}  // namespace mygit

#endif  // MYGIT_COMPRESSION_HPP
