#include "ZstdCompressor.h"

size_t ZstdCompressor::Compress(const char* src, char* dest, size_t srcSize, size_t maxDestSize) {
    return ZSTD_compress(dest, maxDestSize, src, srcSize, 1);
}

size_t ZstdCompressor::Decompress(const char* src, char* dest, size_t compressedSize, size_t maxDecompressedSize) {
    return ZSTD_decompress(dest, maxDecompressedSize, src, compressedSize);
}