#include "LZ4Compressor.h"

int LZ4Compressor::Compress(const char* src, char* dest, int srcSize, int maxDestSize) {
    return LZ4_compress_default(src, dest, srcSize, maxDestSize);
}

int LZ4Compressor::Decompress(const char* src, char* dest, int compressedSize, int maxDecompressedSize) {
    return LZ4_decompress_safe(src, dest, compressedSize, maxDecompressedSize);
}