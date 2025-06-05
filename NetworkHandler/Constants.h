#pragma once
// In your server's configuration or initialization code:
#include "sodium.h"
#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>

unsigned char server_static_public_key_on_server[crypto_kx_PUBLICKEYBYTES];
unsigned char server_static_secret_key_on_server[crypto_kx_SECRETKEYBYTES];
bool server_keys_initialized = false;

bool hex_string_to_byte_array(const char* hex_string, unsigned char* byte_array, size_t byte_array_len) {
    if (hex_string == nullptr || byte_array == nullptr) return false;
    size_t hex_len = strlen(hex_string);
    if (hex_len != byte_array_len * 2) {
        // std::cerr << "Error: Hex string has incorrect length. Expected " << byte_array_len * 2 << ", got " << hex_len << std::endl;
        return false;
    }
    size_t decoded_len;
    if (sodium_hex2bin(byte_array, byte_array_len, hex_string, hex_len, NULL, &decoded_len, NULL) != 0) {
        // std::cerr << "Error: sodium_hex2bin failed." << std::endl;
        return false;
    }
    if (decoded_len != byte_array_len) {
        // std::cerr << "Error: Decoded length " << decoded_len << " does not match expected length " << byte_array_len << std::endl;
        return false;
    }
    return true;
}


void initialize_server_side_static_keys() {
    const char* server_pk_hex = "35cebb98cfd213fbd11338d24df058aa21bf368b17841b6f1ffd4dad6023fd0b"; // Key1 Public
    const char* server_sk_hex = "c18e9c533e7a1835db8612fb7dd4f4c4c4002367c91e90ebc451d304ece2d873"; // Key1 Secret

    if (!hex_string_to_byte_array(server_pk_hex, server_static_public_key_on_server, crypto_kx_PUBLICKEYBYTES)) {
        throw std::runtime_error("Failed to initialize server static public key on server.");
    }
    if (!hex_string_to_byte_array(server_sk_hex, server_static_secret_key_on_server, crypto_kx_SECRETKEYBYTES)) {
        throw std::runtime_error("Failed to initialize server static secret key on server.");
    }
    server_keys_initialized = true;
    // Now server_static_public_key_on_server and server_static_secret_key_on_server are ready
}

// Call initialize_server_side_static_keys() once at server startup after sodium_init().
// When you need them for crypto_kx_server_session_keys:
// if (!server_keys_initialized) { /* handle error */ }
// ... use server_static_public_key_on_server and server_static_secret_key_on_server ...