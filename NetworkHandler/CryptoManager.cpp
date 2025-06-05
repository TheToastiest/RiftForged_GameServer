// File: CryptoManager.cpp
#include "CryptoManager.h"
// Include libsodium headers if not already pulled in by CryptoManager.h
// or if specific implementation details here need them.
// e.g., #include "sodium.h"

// --- ChaCha20-Poly1305 Definitions ---
std::vector<uint8_t> CryptoManager::EncryptChaCha20Poly1305(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& nonce) {

    if (key.size() != crypto_aead_chacha20poly1305_KEYBYTES) {
        throw std::runtime_error("EncryptChaCha20Poly1305: Invalid key size.");
    }
    if (nonce.size() != crypto_aead_chacha20poly1305_NPUBBYTES) {
        throw std::runtime_error("EncryptChaCha20Poly1305: Invalid nonce size.");
    }

    std::vector<uint8_t> ciphertext(plaintext.size() + crypto_aead_chacha20poly1305_ABYTES);
    unsigned long long ciphertext_len;

    // Assuming no additional authenticated data (AAD)
    if (crypto_aead_chacha20poly1305_encrypt(
        ciphertext.data(), &ciphertext_len,
        plaintext.data(), plaintext.size(),
        NULL, 0, // AAD and AAD length
        NULL,     // nsec - not used, must be NULL
        nonce.data(), key.data()) != 0) {
        throw std::runtime_error("EncryptChaCha20Poly1305: Encryption failed.");
    }
    // Resize ciphertext to actual length, though libsodium usually fills it if buffer is exact
    ciphertext.resize(ciphertext_len);
    return ciphertext;
}

std::vector<uint8_t> CryptoManager::DecryptChaCha20Poly1305(
    const std::vector<uint8_t>& ciphertext,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& nonce) {

    if (key.size() != crypto_aead_chacha20poly1305_KEYBYTES) {
        throw std::runtime_error("DecryptChaCha20Poly1305: Invalid key size.");
    }
    if (nonce.size() != crypto_aead_chacha20poly1305_NPUBBYTES) {
        throw std::runtime_error("DecryptChaCha20Poly1305: Invalid nonce size.");
    }
    if (ciphertext.size() < crypto_aead_chacha20poly1305_ABYTES) {
        throw std::runtime_error("DecryptChaCha20Poly1305: Ciphertext too short.");
    }

    std::vector<uint8_t> decrypted(ciphertext.size() - crypto_aead_chacha20poly1305_ABYTES);
    unsigned long long decrypted_len;

    if (crypto_aead_chacha20poly1305_decrypt(
        decrypted.data(), &decrypted_len,
        NULL,     // nsec - not used, must be NULL
        ciphertext.data(), ciphertext.size(),
        NULL, 0, // AAD and AAD length
        nonce.data(), key.data()) != 0) {
        throw std::runtime_error("DecryptChaCha20Poly1305: Decryption failed (authentication tag mismatch or corrupted data).");
    }
    decrypted.resize(decrypted_len);
    return decrypted;
}

// --- AES-256-GCM Definitions ---
std::vector<uint8_t> CryptoManager::EncryptAES_GCM(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& nonce) {

    if (!crypto_aead_aes256gcm_is_available()) {
        throw std::runtime_error("AES-GCM is not available on this platform!");
    }
    if (key.size() != crypto_aead_aes256gcm_KEYBYTES) {
        throw std::runtime_error("EncryptAES_GCM: Invalid key size.");
    }
    if (nonce.size() != crypto_aead_aes256gcm_NPUBBYTES) {
        throw std::runtime_error("EncryptAES_GCM: Invalid nonce size.");
    }

    std::vector<uint8_t> ciphertext(plaintext.size() + crypto_aead_aes256gcm_ABYTES);
    unsigned long long ciphertext_len;

    if (crypto_aead_aes256gcm_encrypt(
        ciphertext.data(), &ciphertext_len,
        plaintext.data(), plaintext.size(),
        NULL, 0, // AAD
        NULL,     // nsec
        nonce.data(), key.data()) != 0) {
        throw std::runtime_error("EncryptAES_GCM: Encryption failed.");
    }
    ciphertext.resize(ciphertext_len);
    return ciphertext;
}

std::vector<uint8_t> CryptoManager::DecryptAES_GCM(
    const std::vector<uint8_t>& ciphertext,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& nonce) {

    if (!crypto_aead_aes256gcm_is_available()) {
        throw std::runtime_error("AES-GCM is not available on this platform!");
    }
    if (key.size() != crypto_aead_aes256gcm_KEYBYTES) {
        throw std::runtime_error("DecryptAES_GCM: Invalid key size.");
    }
    if (nonce.size() != crypto_aead_aes256gcm_NPUBBYTES) {
        throw std::runtime_error("DecryptAES_GCM: Invalid nonce size.");
    }
    if (ciphertext.size() < crypto_aead_aes256gcm_ABYTES) {
        throw std::runtime_error("DecryptAES_GCM: Ciphertext too short.");
    }

    std::vector<uint8_t> decrypted(ciphertext.size() - crypto_aead_aes256gcm_ABYTES);
    unsigned long long decrypted_len;

    if (crypto_aead_aes256gcm_decrypt(
        decrypted.data(), &decrypted_len,
        NULL,     // nsec
        ciphertext.data(), ciphertext.size(),
        NULL, 0, // AAD
        nonce.data(), key.data()) != 0) {
        throw std::runtime_error("DecryptAES_GCM: Decryption failed (authentication tag mismatch or corrupted data).");
    }
    decrypted.resize(decrypted_len);
    return decrypted;
}