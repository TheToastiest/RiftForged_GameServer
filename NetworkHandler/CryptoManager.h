#ifndef CRYPTO_MANAGER_H
#define CRYPTO_MANAGER_H

#include <stdexcept>
#include <sodium.h>
#include <vector>

class CryptoManager {
public:
    static CryptoManager& GetInstance();

    std::vector<uint8_t> EncryptChaCha20Poly1305(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key, const std::vector<uint8_t>& nonce);
    std::vector<uint8_t> DecryptChaCha20Poly1305(const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key, const std::vector<uint8_t>& nonce);

    std::vector<uint8_t> EncryptAES_GCM(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key, const std::vector<uint8_t>& nonce);
    std::vector<uint8_t> DecryptAES_GCM(const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key, const std::vector<uint8_t>& nonce);

private:
    CryptoManager();
};

#endif