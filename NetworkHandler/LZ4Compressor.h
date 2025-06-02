#ifndef LZ4_COMPRESSOR_H
#define LZ4_COMPRESSOR_H

#include <lz4.h>

class LZ4Compressor {
public:
    static int Compress(const char* src, char* dest, int srcSize, int maxDestSize);
    static int Decompress(const char* src, char* dest, int compressedSize, int maxDecompressedSize);
};

#endif