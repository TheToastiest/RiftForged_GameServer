// File: CryptoManager.h
#pragma once

#include <vector>
#include <cstdint>
#include <stdexcept> // For std::runtime_error
// It's good practice to forward-declare if you only need pointers/references,
// but for std::vector return types and parameters, including <vector> is necessary.
// Make sure your libsodium includes are appropriate (e.g., "sodium.h")
// For example, if you're using libsodium directly for constants:
#include "sodium.h" // Or your specific path to libsodium's headers

class CryptoManager {
public:
    // ChaCha20-Poly1305 Functions
    std::vector<uint8_t> EncryptChaCha20Poly1305(
        const std::vector<uint8_t>& plaintext,
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& nonce
    );

    std::vector<uint8_t> DecryptChaCha20Poly1305(
        const std::vector<uint8_t>& ciphertext,
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& nonce
    );

    // AES-256-GCM Functions
    std::vector<uint8_t> EncryptAES_GCM(
        const std::vector<uint8_t>& plaintext,
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& nonce
    );

    std::vector<uint8_t> DecryptAES_GCM(
        const std::vector<uint8_t>& ciphertext,
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& nonce
    );
};