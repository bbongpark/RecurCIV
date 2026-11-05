#pragma once
#include <stddef.h>
#include <stdint.h>

/* Borrowed, immutable bytes; no terminating NUL is implied. */
typedef struct { const uint8_t *bytes;size_t size; } CivPayload;
CivPayload civ_payload_embedded(void);
