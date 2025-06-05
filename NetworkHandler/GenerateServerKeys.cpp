// generate_server_keys_util.cpp
#include <iostream>
#include "sodium.h" // Or your specific libsodium header path

int main() {
    if (sodium_init() < 0) {
        std::cerr << "Libsodium initialization failed!" << std::endl;
        return 1;
    }

    unsigned char server_static_public_key[crypto_kx_PUBLICKEYBYTES];
    unsigned char server_static_secret_key[crypto_kx_SECRETKEYBYTES];
    crypto_kx_keypair(server_static_public_key, server_static_secret_key);

    std::cout << "Server Static Public Key (for client, hex): ";
    for (size_t i = 0; i < crypto_kx_PUBLICKEYBYTES; ++i) {
        printf("%02x", server_static_public_key[i]);
    }
    std::cout << std::endl;

    std::cout << "Server Static Secret Key (for server, hex): ";
    for (size_t i = 0; i < crypto_kx_SECRETKEYBYTES; ++i) {
        printf("%02x", server_static_secret_key[i]);
    }
    std::cout << std::endl;

    return 0;
}