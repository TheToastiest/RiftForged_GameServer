#pragma once
// In your client's configuration or a constants file:
#include "sodium.h" // For crypto_kx_PUBLICKEYBYTES and sodium_hex2bin
#include <string>
#include <vector>
#include <stdexcept> // For error handling
#include <cstring>   // For strlen, memcpy if not using std::vector directly

// Global or accessible server static public key for the client
unsigned char server_static_public_key_for_client[crypto_kx_PUBLICKEYBYTES];
bool server_pk_initialized = false;

// Helper function to convert hex string to a fixed-size byte array
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

void initialize_client_side_server_pk() {
    const char* server_pk_hex = "35cebb98cfd213fbd11338d24df058aa21bf368b17841b6f1ffd4dad6023fd0b"; // Using Key1 Public
    if (!hex_string_to_byte_array(server_pk_hex, server_static_public_key_for_client, crypto_kx_PUBLICKEYBYTES)) {
        throw std::runtime_error("Failed to initialize server static public key on client.");
    }
    server_pk_initialized = true;
    // Now server_static_public_key_for_client is ready to be used
}

// Call initialize_client_side_server_pk() once at client startup after sodium_init().
// When you need it for crypto_kx_client_session_keys:
// if (!server_pk_initialized) { /* handle error, not initialized */ }
// ... use server_static_public_key_for_client ...
