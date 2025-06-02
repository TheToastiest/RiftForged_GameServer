#include "CryptoManager.h"

std::vector<uint8_t> CryptoManager::EncryptChaCha20Poly1305(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key, const std::vector<uint8_t>& nonce) {
    std::vector<uint8_t> ciphertext(plaintext.size() + crypto_aead_chacha20poly1305_ABYTES);
    unsigned long long ciphertext_len;

    crypto_aead_chacha20poly1305_encrypt(ciphertext.data(), &ciphertext_len, plaintext.data(), plaintext.size(), NULL, 0, NULL, nonce.data(), key.data());

    return ciphertext;
}

std::vector<uint8_t> CryptoManager::DecryptChaCha20Poly1305(const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key, const std::vector<uint8_t>& nonce) {
    std::vector<uint8_t> decrypted(ciphertext.size() - crypto_aead_chacha20poly1305_ABYTES);
    unsigned long long decrypted_len;

    if (crypto_aead_chacha20poly1305_decrypt(decrypted.data(), &decrypted_len, NULL, ciphertext.data(), ciphertext.size(), NULL, 0, nonce.data(), key.data()) != 0) {
        throw std::runtime_error("Decryption failed.");
    }

    return decrypted;
}