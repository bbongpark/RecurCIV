#include "macro_storage.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _MSC_VER
#include <direct.h>
#include <io.h>
#include <windows.h>
#define MAKE_DIR(p) _mkdir(p)
#define OPEN_FILE _open
#define FILE_STREAM _fdopen
#define CLOSE_FILE _close
#define SYNC_FILE(f) _commit(_fileno(f))
#define OPEN_FLAGS (_O_WRONLY|_O_CREAT|_O_EXCL|_O_BINARY)
#define S_ISDIR(m) (((m)&_S_IFMT)==_S_IFDIR)
#else
#include <dirent.h>
#include <unistd.h>
#define MAKE_DIR(p) mkdir(p,0777)
#define OPEN_FILE open
#define FILE_STREAM fdopen
#define CLOSE_FILE close
#define SYNC_FILE(f) fsync(fileno(f))
#define OPEN_FLAGS (O_WRONLY|O_CREAT|O_EXCL)
#endif

#define PATH_CAP 1024
#ifdef MACRO_STORAGE_TESTING
static MacroStoreFault injected_fault;
static int injected_errno;
void macro_store_test_fault(MacroStoreFault fault,int error_number) {
    injected_fault=fault;injected_errno=error_number;
}
static bool fault_at(MacroStoreFault fault) {
    /* RESTORE simulates a failed replacement followed by a failed rollback. */
    if(fault==MACRO_FAULT_REPLACE && injected_fault==MACRO_FAULT_RESTORE) { errno=injected_errno;return true; }
    if(injected_fault!=fault) return false;
    injected_fault=MACRO_FAULT_NONE;errno=injected_errno;return true;
}
#define FAULT(stage) fault_at(MACRO_FAULT_##stage)
#else
#define FAULT(stage) false
#endif
static MacroStoreResult storage_error(int error_number) {
    return error_number==ENOSPC?MACRO_STORE_FULL:MACRO_STORE_IO;
}
static bool make_path(char *out,const char *root,const char *name,const char *ext) {
    if(!out || !root || !macro_name_valid(name)) return false;
    size_t r=strlen(root),n=strlen(name),e=strlen(ext);
    if(!r || r+1+n+e+1>PATH_CAP) return false;
    memcpy(out,root,r);if(out[r-1]!='/') out[r++]='/';
    for(size_t i=0;i<n;i++) { char c=name[i];out[r++]=(c>='A' && c<='Z')?(char)(c+'a'-'A'):c; }
    memcpy(out+r,ext,e+1);return true;
}
static MacroStoreResult exists(const char *path,bool *found) {
    struct stat st;
    if(stat(path,&st)==0) { *found=true;return MACRO_STORE_OK; }
    if(errno==ENOENT) { *found=false;return MACRO_STORE_OK; }
    return MACRO_STORE_IO;
}
static MacroStoreResult ensure_dir(const char *path) {
    struct stat st;
    if(MAKE_DIR(path)==0) return MACRO_STORE_OK;
    if(errno==EEXIST && stat(path,&st)==0 && S_ISDIR(st.st_mode)) return MACRO_STORE_OK;
    return MACRO_STORE_IO;
}
MacroStoreResult macro_store_prepare_default(void) {
    const char *dirs[]={"sdmc:/switch","sdmc:/switch/RecurCIV",MACRO_STORAGE_DIRECTORY};
    for(size_t i=0;i<3;i++) if(ensure_dir(dirs[i])!=MACRO_STORE_OK) return MACRO_STORE_IO;
    return MACRO_STORE_OK;
}
MacroStoreResult macro_store_validate_file(const char *path,MacroDocument *out) {
    if(!path || !out) return MACRO_STORE_INVALID;
    FILE *f=fopen(path,"rb");
    if(!f) return errno==ENOENT?MACRO_STORE_NOT_FOUND:MACRO_STORE_IO;
    uint8_t *bytes=malloc(MACRO_FILE_MAX_BYTES+1u);
    if(!bytes) { fclose(f);return MACRO_STORE_IO; }
    size_t n=fread(bytes,1,MACRO_FILE_MAX_BYTES+1u,f);bool failed=ferror(f)!=0;
    if(fclose(f)!=0) failed=true;
    MacroStoreResult result=failed?MACRO_STORE_IO:macro_decode(bytes,n,out);
    free(bytes);return result;
}
MacroStoreResult macro_store_load(const char *root,const char *name,MacroDocument *out) {
    char path[PATH_CAP];if(!make_path(path,root,name,".rcm")) return MACRO_STORE_INVALID;
    MacroDocument *doc=malloc(sizeof(*doc));if(!doc) return MACRO_STORE_IO;
    MacroStoreResult r=macro_store_validate_file(path,doc);
    char embedded[PATH_CAP];
    if(r==MACRO_STORE_OK && (!make_path(embedded,root,doc->name,".rcm") || strcmp(path,embedded))) r=MACRO_STORE_INVALID;
    if(r==MACRO_STORE_OK && out) *out=*doc;
    else if(!out) r=MACRO_STORE_INVALID;
    free(doc);return r;
}
MacroStoreResult macro_store_save(const char *root,const MacroDocument *doc,bool replace) {
    char dst[PATH_CAP],tmp[PATH_CAP],bak[PATH_CAP];bool present,temp_exists,backup_exists;
    if(!macro_document_valid(doc) || !make_path(dst,root,doc->name,".rcm") ||
       !make_path(tmp,root,doc->name,".tmp") || !make_path(bak,root,doc->name,".bak")) return MACRO_STORE_INVALID;
    if(exists(dst,&present)!=MACRO_STORE_OK || exists(tmp,&temp_exists)!=MACRO_STORE_OK || exists(bak,&backup_exists)!=MACRO_STORE_OK) return MACRO_STORE_IO;
    if(temp_exists || backup_exists) return MACRO_STORE_RECOVERY_REQUIRED;
    if(present && !replace) return MACRO_STORE_EXISTS;
    MacroDocument *verify=malloc(sizeof(*verify));uint8_t *bytes=malloc(MACRO_FILE_MAX_BYTES);size_t length=0;
    if(!verify || !bytes) { free(verify);free(bytes);return MACRO_STORE_IO; }
    MacroStoreResult result=macro_encode(doc,bytes,MACRO_FILE_MAX_BYTES,&length);
    /* Never replace a damaged or newer-version file silently. */
    if(result==MACRO_STORE_OK && present) result=macro_store_load(root,doc->name,verify);
    if(result!=MACRO_STORE_OK) { free(verify);free(bytes);return result; }
    int fd=FAULT(CREATE)?-1:OPEN_FILE(tmp,OPEN_FLAGS,0666);
    if(fd<0) { int e=errno;free(verify);free(bytes);return e==EEXIST?MACRO_STORE_RECOVERY_REQUIRED:storage_error(e); }
    FILE *f=FILE_STREAM(fd,"wb");
    if(!f) { CLOSE_FILE(fd);free(verify);free(bytes);return MACRO_STORE_RECOVERY_REQUIRED; }
    bool ok=true;int failure=0;
    if(FAULT(WRITE)) { failure=errno;(void)fwrite(bytes,1,length/2,f);ok=false; }
    else if(fwrite(bytes,1,length,f)!=length) { failure=errno;ok=false; }
    if(ok && (FAULT(FLUSH) || fflush(f)!=0)) { failure=errno;ok=false; }
    if(ok && (FAULT(FSYNC) || SYNC_FILE(f)!=0)) { failure=errno;ok=false; }
    if(fclose(f)!=0 && ok) { failure=errno;ok=false; }
    if(ok) {
        result=FAULT(READBACK)?MACRO_STORE_IO:macro_store_validate_file(tmp,verify);
        size_t verify_len=0;uint8_t *check=malloc(MACRO_FILE_MAX_BYTES);
        if(!check || result!=MACRO_STORE_OK || macro_encode(verify,check,MACRO_FILE_MAX_BYTES,&verify_len)!=MACRO_STORE_OK ||
           verify_len!=length || memcmp(bytes,check,length)) ok=false;
        free(check);
    }
    free(verify);free(bytes);
    if(!ok) return failure==ENOSPC?MACRO_STORE_FULL:MACRO_STORE_RECOVERY_REQUIRED; /* retain temporary */
    if(present && (FAULT(BACKUP) || rename(dst,bak)!=0)) return errno==ENOSPC?MACRO_STORE_FULL:MACRO_STORE_RECOVERY_REQUIRED;
    if(FAULT(REPLACE) || rename(tmp,dst)!=0) {
        int e=errno;
        if(present && !FAULT(RESTORE)) (void)rename(bak,dst);
        if(e==ENOSPC) return MACRO_STORE_FULL;
        return MACRO_STORE_RECOVERY_REQUIRED;
    }
    if(present && (FAULT(CLEANUP) || remove(bak)!=0)) return MACRO_STORE_RECOVERY_REQUIRED;
    return MACRO_STORE_OK;
}
MacroStoreResult macro_store_duplicate(const char *root,const char *source,const char *destination) {
    if(!macro_name_valid(destination)) return MACRO_STORE_INVALID;
    MacroDocument *doc=malloc(sizeof(*doc));if(!doc) return MACRO_STORE_IO;
    MacroStoreResult r=macro_store_load(root,source,doc);
    if(r==MACRO_STORE_OK) { strcpy(doc->name,destination);r=macro_store_save(root,doc,false); }
    free(doc);return r;
}
MacroStoreResult macro_store_rename(const char *root,const char *source,const char *destination) {
    MacroStoreResult r=macro_store_duplicate(root,source,destination);
    return r==MACRO_STORE_OK?macro_store_delete(root,source):r;
}
MacroStoreResult macro_store_delete(const char *root,const char *name) {
    char dst[PATH_CAP],tmp[PATH_CAP],bak[PATH_CAP];bool a,b;
    if(!make_path(dst,root,name,".rcm") || !make_path(tmp,root,name,".tmp") || !make_path(bak,root,name,".bak")) return MACRO_STORE_INVALID;
    if(exists(tmp,&a)!=MACRO_STORE_OK || exists(bak,&b)!=MACRO_STORE_OK) return MACRO_STORE_IO;
    if(a || b) return MACRO_STORE_RECOVERY_REQUIRED;
    if(remove(dst)==0) return MACRO_STORE_OK;
    return errno==ENOENT?MACRO_STORE_NOT_FOUND:MACRO_STORE_IO;
}
static bool valid_recovery_file(const char *path,const char *root,const char *name,MacroDocument *doc) {
    char expected[PATH_CAP],actual[PATH_CAP];
    return macro_store_validate_file(path,doc)==MACRO_STORE_OK &&
        make_path(expected,root,name,".rcm") && make_path(actual,root,doc->name,".rcm") && !strcmp(expected,actual);
}
MacroStoreResult macro_store_recover(const char *root,const char *name) {
    char dst[PATH_CAP],tmp[PATH_CAP],bak[PATH_CAP];bool d,t,b;
    if(!make_path(dst,root,name,".rcm") || !make_path(tmp,root,name,".tmp") || !make_path(bak,root,name,".bak")) return MACRO_STORE_INVALID;
    if(exists(dst,&d)!=MACRO_STORE_OK || exists(tmp,&t)!=MACRO_STORE_OK || exists(bak,&b)!=MACRO_STORE_OK) return MACRO_STORE_IO;
    MacroDocument *doc=malloc(sizeof(*doc));if(!doc) return MACRO_STORE_IO;
    /* Never overwrite or delete an invalid destination or backup automatically. */
    MacroStoreResult r=MACRO_STORE_OK;
    if(d && !valid_recovery_file(dst,root,name,doc)) r=MACRO_STORE_RECOVERY_REQUIRED;
    if(b && !valid_recovery_file(bak,root,name,doc)) r=MACRO_STORE_RECOVERY_REQUIRED;
    if(r==MACRO_STORE_OK && b && !d) { if(rename(bak,dst)!=0) r=MACRO_STORE_IO;else { b=false;d=true; } }
    if(r==MACRO_STORE_OK && b && !t && remove(bak)!=0) r=MACRO_STORE_IO;
    if(r==MACRO_STORE_OK && t && !d) {
        if(!valid_recovery_file(tmp,root,name,doc)) r=MACRO_STORE_RECOVERY_REQUIRED;
        else if(rename(tmp,dst)!=0) r=MACRO_STORE_IO;
        else t=false;
    }
    /* Keep potentially valuable staged edits when a destination already exists. */
    if(r==MACRO_STORE_OK && t) r=MACRO_STORE_RECOVERY_REQUIRED;
    free(doc);return r;
}
static int compare_names(const void *a,const void *b) { return strcmp((const char *)a,(const char *)b); }
MacroStoreResult macro_store_list(const char *root,char (*names)[MACRO_NAME_BYTES],size_t capacity,size_t *count) {
    if(!root || !count || (capacity && !names)) return MACRO_STORE_INVALID;
#ifdef _MSC_VER
    char pattern[PATH_CAP];int written=snprintf(pattern,sizeof(pattern),"%s/*",root);
    if(written<0 || (size_t)written>=sizeof(pattern)) return MACRO_STORE_INVALID;
    WIN32_FIND_DATAA entry;HANDLE handle=FindFirstFileA(pattern,&entry);
    if(handle==INVALID_HANDLE_VALUE) return MACRO_STORE_IO;
#else
    DIR *d=opendir(root);if(!d) return MACRO_STORE_IO;
#endif
    size_t n=0;
#ifdef _MSC_VER
    do {
        const char *filename=entry.cFileName;
#else
    struct dirent *e;errno=0;
    while((e=readdir(d))!=NULL) {
        const char *filename=e->d_name;
#endif
        size_t l=strlen(filename);
        if(l>4 && l<MACRO_NAME_BYTES+4 && !strcmp(filename+l-4,".rcm")) {
            char name[MACRO_NAME_BYTES];memcpy(name,filename,l-4);name[l-4]=0;
            if(macro_name_valid(name)) { if(n<capacity) strcpy(names[n],name);n++; }
        }
#ifdef _MSC_VER
    } while(FindNextFileA(handle,&entry));
    DWORD error=GetLastError();FindClose(handle);
    if(error!=ERROR_NO_MORE_FILES) return MACRO_STORE_IO;
#else
    }
    int read_error=errno;
    if(closedir(d)!=0) return MACRO_STORE_IO;
    if(read_error) return MACRO_STORE_IO;
#endif
    *count=n;
    if(n>capacity) return MACRO_STORE_BUFFER_SMALL;
    if(n>1) qsort(names,n,sizeof(*names),compare_names);
    return MACRO_STORE_OK;
}
