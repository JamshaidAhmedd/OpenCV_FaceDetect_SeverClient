#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>   // For uint32_t, uint8_t
#include <stddef.h>

static const uint32_t PROTOCOL_PREFIX = 0x23107231U;
#define OP_FACE_DETECT    0  // Client -> Server
#define OP_FACE_REPLACE   1  // Client -> Server
#define OP_OUTPUT_IMAGE   2  // Server -> Client
#define OP_ERROR_MESSAGE  3  // Server -> Client

// Send all bytes in buffer, return true on success.
bool send_all(int sockfd, const char *buf, size_t len);

// Receive exactly len bytes into buf, return true on success (false on error/EOF).
bool recv_all(int sockfd, char *buf, size_t len);

#endif // PROTOCOL_H
