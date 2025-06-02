#ifndef ZSTD_COMPRESSOR_H
#define ZSTD_COMPRESSOR_H

#include <zstd.h>

class ZstdCompressor {
public:
    static size_t Compress(const char* src, char* dest, size_t srcSize, size_t maxDestSize);
    static size_t Decompress(const char* src, char* dest, size_t compressedSize, size_t maxDecompressedSize);
};

#endif