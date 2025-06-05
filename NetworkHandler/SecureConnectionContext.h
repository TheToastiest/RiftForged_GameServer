// File: SecureConnectionContext.h
#pragma once

#include "sodium.h" // For crypto_kx_...BYTES constants and other libsodium types
#include <vector>
#include <atomic>    // For handshake_state if you anticipate multithreaded access to it,
                     // otherwise, regular enum state is fine if access is synchronized.
#include <cstdint>   // For uint64_t

namespace RiftForged {
    namespace Networking {

        enum class SecureHandshakeState {
            INITIAL,                       // Connection just started, no security steps taken
            AWAITING_CLIENT_EPHEMERAL_KEY, // Server state: Waiting for the client's first handshake message
            SENT_CLIENT_EPHEMERAL_KEY,     // Client state: Client has sent its ephemeral key
            // (Client might then move to AWAITING_SERVER_CONFIRMATION or derive keys)
            KEYS_DERIVED,                  // Both sides have derived keys but maybe not fully confirmed channel
            HANDSHAKE_COMPLETE,            // Secure channel established, keys ready for use
            HANDSHAKE_FAILED               // Handshake attempt failed
        };

        struct SecureConnectionContext {
            SecureHandshakeState handshake_state;

            // For client to store its ephemeral keypair used in the handshake
            unsigned char client_ephemeral_pk[crypto_kx_PUBLICKEYBYTES];
            unsigned char client_ephemeral_sk[crypto_kx_SECRETKEYBYTES];

            // For server to store the client's ephemeral public key it received
            unsigned char received_client_ephemeral_pk[crypto_kx_PUBLICKEYBYTES];

            // Shared session keys, derived from the handshake
            unsigned char session_rx_key[crypto_kx_SESSIONKEYBYTES]; // For decrypting incoming data
            unsigned char session_tx_key[crypto_kx_SESSIONKEYBYTES]; // For encrypting outgoing data

            // Nonce counters for AEAD encryption
            uint64_t next_tx_nonce;
            uint64_t next_rx_nonce; // Track expected nonce from the other side

            SecureConnectionContext() :
                handshake_state(SecureHandshakeState::INITIAL),
                next_tx_nonce(0), // Start nonces from 0 (or a random value if preferred, but must be unique)
                next_rx_nonce(0)
            {
                // Initialize memory to prevent use of uninitialized values, though they get overwritten.
                sodium_memzero(client_ephemeral_pk, crypto_kx_PUBLICKEYBYTES);
                sodium_memzero(client_ephemeral_sk, crypto_kx_SECRETKEYBYTES);
                sodium_memzero(received_client_ephemeral_pk, crypto_kx_PUBLICKEYBYTES);
                sodium_memzero(session_rx_key, crypto_kx_SESSIONKEYBYTES);
                sodium_memzero(session_tx_key, crypto_kx_SESSIONKEYBYTES);
            }

            // Helper for nonce generation (ensure correct size for AEAD)
            std::vector<uint8_t> get_next_tx_nonce_bytes() {
                // Libsodium nonces (e.g., for ChaCha20-Poly1305 or AES-GCM) are typically
                // crypto_aead_chacha20poly1305_NPUBBYTES or crypto_aead_aes256gcm_NPUBBYTES long.
                // A common practice for a 12-byte (96-bit) nonce is to use the first 4 bytes
                // for a fixed value (or part of a connection ID) and the next 8 bytes for the counter.
                // Or simply use the 8-byte counter and pad with zeros if the nonce is longer.
                // For simplicity here, we'll just use the 64-bit counter directly and assume
                // it's correctly formatted for the chosen AEAD cipher's nonce requirements.
                // You'll need to adapt this if your AEAD cipher needs a different nonce structure.
                std::vector<uint8_t> nonce_bytes(crypto_aead_chacha20poly1305_NPUBBYTES, 0); // Example size

                uint64_t current_nonce_val = next_tx_nonce++; // Use and then increment

                // Simplistic: copy counter to the start of the nonce.
                // Ensure this aligns with how you construct nonces for your specific AEAD.
                // For example, if NPUBBYTES is 12 and sizeof(uint64_t) is 8, you could do:
                memcpy(nonce_bytes.data() + (nonce_bytes.size() - sizeof(current_nonce_val)), // Right-align counter
                    &current_nonce_val,
                    sizeof(current_nonce_val));
                // Or if you want it at the beginning:
                // memcpy(nonce_bytes.data(), &current_nonce_val, sizeof(current_nonce_val));


                return nonce_bytes;
            }

            // You'll also need a way to construct the *expected* nonce for received messages
            // based on next_rx_nonce.
        };

    }
} // namespace RiftForged::Networking