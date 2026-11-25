/**
 * @file compression.cpp
 * @brief Zlib deflate/inflate helpers for Git object storage.
 * @author Rajath Gadagkar <rajathgadagkar@gmail.com>
 */

#include "mygit/compression.hpp"

#include <cstring>
#include <zlib.h>

namespace mygit {

std::optional<std::string> Compression::deflateData(const std::string& input) {
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));

    if (deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK) {
        return std::nullopt;
    }

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    zs.avail_in = static_cast<uInt>(input.size());

    int ret = Z_OK;
    char outBuffer[32768];
    std::string compressed;

    do {
        zs.next_out = reinterpret_cast<Bytef*>(outBuffer);
        zs.avail_out = sizeof(outBuffer);

        ret = deflate(&zs, Z_FINISH);
        if (compressed.size() < zs.total_out) {
            compressed.append(outBuffer, zs.total_out - compressed.size());
        }
    } while (ret == Z_OK);

    deflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        return std::nullopt;
    }

    return compressed;
}

std::optional<std::string> Compression::inflateData(const std::string& input) {
    if (input.empty()) {
        return std::nullopt;
    }

    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));

    if (inflateInit(&zs) != Z_OK) {
        return std::nullopt;
    }

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    zs.avail_in = static_cast<uInt>(input.size());

    int ret = Z_OK;
    char outBuffer[32768];
    std::string decompressed;

    do {
        zs.next_out = reinterpret_cast<Bytef*>(outBuffer);
        zs.avail_out = sizeof(outBuffer);

        ret = inflate(&zs, Z_NO_FLUSH);
        if (decompressed.size() < zs.total_out) {
            decompressed.append(outBuffer, zs.total_out - decompressed.size());
        }
    } while (ret == Z_OK);

    inflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        return std::nullopt;
    }

    return decompressed;
}

}  // namespace mygit
