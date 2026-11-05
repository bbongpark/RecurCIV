#pragma once
#include "macro_model.h"
#include <stddef.h>
#include <stdint.h>

#define MACRO_STORAGE_DIRECTORY "sdmc:/switch/RecurCIV/macros"
#define MACRO_NAME_BYTES 64
#define MACRO_DESCRIPTION_BYTES 256
#define MACRO_FILE_MAX_BYTES (364u + 12u * MACRO_MAX_STEPS)

typedef struct {
    char name[MACRO_NAME_BYTES];
    char description[MACRO_DESCRIPTION_BYTES];
    uint64_t created_unix, modified_unix; /* zero means unavailable */
    uint32_t speed_percent, repeat_count, loop_delay_ms;
    MacroSequence sequence;
} MacroDocument;

typedef enum {
    MACRO_STORE_OK, MACRO_STORE_INVALID, MACRO_STORE_VERSION,
    MACRO_STORE_NOT_FOUND, MACRO_STORE_EXISTS, MACRO_STORE_IO,
    MACRO_STORE_RECOVERY_REQUIRED, MACRO_STORE_BUFFER_SMALL, MACRO_STORE_FULL
} MacroStoreResult;

#ifdef MACRO_STORAGE_TESTING
/* Host tests only; inject a one-shot errno at a named save boundary. */
typedef enum {
    MACRO_FAULT_NONE, MACRO_FAULT_CREATE, MACRO_FAULT_WRITE,
    MACRO_FAULT_FLUSH, MACRO_FAULT_FSYNC, MACRO_FAULT_READBACK,
    MACRO_FAULT_BACKUP, MACRO_FAULT_REPLACE, MACRO_FAULT_CLEANUP,
    MACRO_FAULT_RESTORE
} MacroStoreFault;
void macro_store_test_fault(MacroStoreFault fault,int error_number);
#endif

bool macro_name_valid(const char *name);
bool macro_document_valid(const MacroDocument *document);
MacroStoreResult macro_encode(const MacroDocument *document, uint8_t *bytes, size_t capacity, size_t *length);
MacroStoreResult macro_decode(const uint8_t *bytes, size_t length, MacroDocument *document);

/* A store directory must exist before operations; prepare_default creates the
   SD hierarchy. Root is injected for host tests and future import workflows. */
MacroStoreResult macro_store_prepare_default(void);
MacroStoreResult macro_store_load(const char *root,const char *name,MacroDocument *out);
MacroStoreResult macro_store_validate_file(const char *path,MacroDocument *out);
MacroStoreResult macro_store_save(const char *root,const MacroDocument *doc,bool replace);
MacroStoreResult macro_store_duplicate(const char *root,const char *source,const char *destination);
MacroStoreResult macro_store_rename(const char *root,const char *source,const char *destination);
MacroStoreResult macro_store_delete(const char *root,const char *name);
MacroStoreResult macro_store_recover(const char *root,const char *name);
MacroStoreResult macro_store_list(const char *root,char (*names)[MACRO_NAME_BYTES],size_t capacity,size_t *count);
