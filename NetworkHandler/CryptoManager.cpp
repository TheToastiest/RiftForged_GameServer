#include "CryptoManager.h"

CryptoManager& CryptoManager::GetInstance() {
    static CryptoManager instance;
    return instance;
}

// ChaCha20-Poly1305 Encryption
std::vector<uint8_t> CryptoManager::EncryptChaCha20Poly1305(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key, const std::vector<uint8_t>& nonce) {
    std::vector<uint8_t> ciphertext(plaintext.size() + crypto_aead_chacha20poly1305_ABYTES);
    unsigned long long ciphertext_len;

    if (crypto_aead_chacha20poly1305_encrypt(ciphertext.data(), &ciphertext_len,
        plaintext.data(), plaintext.size(), NULL, 0, NULL,
        nonce.data(), key.data()) != 0) {
        throw std::runtime_error("ChaCha20-Poly1305 encryption failed.");
    }

    return ciphertext;
}

// ChaCha20-Poly1305 Decryption
std::vector<uint8_t> CryptoManager::DecryptChaCha20Poly1305(const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key, const std::vector<uint8_t>& nonce) {
    std::vector<uint8_t> decrypted(ciphertext.size() - crypto_aead_chacha20poly1305_ABYTES);
    unsigned long long decrypted_len;

    if (crypto_aead_chacha20poly1305_decrypt(decrypted.data(), &decrypted_len, NULL,
        ciphertext.data(), ciphertext.size(), NULL, 0, nonce.data(),
        key.data()) != 0) {
        throw std::runtime_error("ChaCha20-Poly1305 decryption failed.");
    }

    return decrypted;
}

// AES-256-GCM Encryption
std::vector<uint8_t> CryptoManager::EncryptAES_GCM(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key, const std::vector<uint8_t>& nonce) {
    std::vector<uint8_t> ciphertext(plaintext.size() + crypto_aead_aes256gcm_ABYTES);
    unsigned long long ciphertext_len;

    if (crypto_aead_aes256gcm_encrypt(ciphertext.data(), &ciphertext_len,
        plaintext.data(), plaintext.size(), NULL, 0, NULL,
        nonce.data(), key.data()) != 0) {
        throw std::runtime_error("AES-256-GCM encryption failed.");
    }

    return ciphertext;
}

// AES-256-GCM Decryption
std::vector<uint8_t> CryptoManager::DecryptAES_GCM(const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key, const std::vector<uint8_t>& nonce) {
	std::vector<uint8_t> decrypted(ciphertext.size() - crypto_aead_aes256gcm_ABYTES);
	unsigned long long decrypted_len;
	if (crypto_aead_aes256gcm_decrypt(decrypted.data(), &decrypted_len, NULL,
		ciphertext.data(), ciphertext.size(), NULL, 0, nonce.data(),
		key.data()) != 0) {
		throw std::runtime_error("AES-256-GCM decryption failed.");
	}
	return decrypted;
}
