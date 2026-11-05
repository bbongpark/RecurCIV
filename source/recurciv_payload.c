/* Generated at build time from the verified payload.txt. */
#include "civ_payload.h"
#include "recurciv_payload.h"

CivPayload civ_payload_embedded(void) {
    return (CivPayload){recurciv_payload,sizeof(recurciv_payload)};
}
