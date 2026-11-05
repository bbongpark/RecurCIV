#include "macro_storage.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include <windows.h>

static MacroDocument doc,loaded;
static uint8_t bytes[MACRO_FILE_MAX_BYTES+1];
static char root[MAX_PATH],path[MAX_PATH];
static void name_path(const char *name) { snprintf(path,sizeof(path),"%s/%s",root,name); }
static void create_file(const char *name,const void *data,size_t n) {
    name_path(name);FILE *f=fopen(path,"wb");assert(f);assert(fwrite(data,1,n,f)==n);assert(!fclose(f));
}
static void erase(const char *name) { name_path(name);assert(remove(path)==0); }
int main(void) {
    char base[MAX_PATH];assert(GetTempPathA(sizeof(base),base)>0);
    snprintf(root,sizeof(root),"%srecurciv-storage-%lu",base,(unsigned long)GetCurrentProcessId());
    assert(_mkdir(root)==0); /* never reuse or remove a pre-existing directory */
    strcpy(doc.name,"Test Macro");strcpy(doc.description,"Simultaneous + analog\nExample");
    doc.created_unix=123;doc.modified_unix=456;doc.speed_percent=150;doc.repeat_count=3;doc.loop_delay_ms=75;
    MacroStep step={{0x084,1,0,255,23,200,0},1234};
    assert(macro_sequence_insert(&doc.sequence,0,&step));
    size_t n=0;assert(macro_encode(&doc,bytes,sizeof(bytes),&n)==MACRO_STORE_OK && n==376);
    assert(macro_decode(bytes,n,&loaded)==MACRO_STORE_OK);
    assert(!strcmp(loaded.name,doc.name) && !strcmp(loaded.description,doc.description));
    assert(loaded.created_unix==123 && loaded.modified_unix==456 && loaded.speed_percent==150);
    assert(loaded.repeat_count==3 && loaded.loop_delay_ms==75 && loaded.sequence.duration_ms==1234);
    assert(loaded.sequence.steps[0].report.buttons==0x84 && loaded.sequence.steps[0].report.hat==1);
    assert(loaded.sequence.steps[0].report.lx==0 && loaded.sequence.steps[0].report.ly==255);
    assert(macro_decode(bytes,n-1,&loaded)==MACRO_STORE_INVALID);
    bytes[4]=2;assert(macro_decode(bytes,n,&loaded)==MACRO_STORE_VERSION);bytes[4]=1;
    bytes[100]^=1;assert(macro_decode(bytes,n,&loaded)==MACRO_STORE_INVALID);bytes[100]^=1;
    assert(!macro_name_valid("../bad") && !macro_name_valid("a/b") && !macro_name_valid("a.") && !macro_name_valid(" bad"));
    assert(macro_store_save(root,&doc,false)==MACRO_STORE_OK);
    assert(macro_store_save(root,&doc,false)==MACRO_STORE_EXISTS);
    assert(macro_store_load(root,"TEST MACRO",&loaded)==MACRO_STORE_OK);
    assert(!strcmp(loaded.name,"Test Macro"));
    strcpy(doc.description,"updated");assert(macro_store_save(root,&doc,true)==MACRO_STORE_OK);
    assert(macro_store_load(root,"test macro",&loaded)==MACRO_STORE_OK && !strcmp(loaded.description,"updated"));
#ifdef MACRO_STORAGE_TESTING
    const MacroStoreFault faults[]={MACRO_FAULT_CREATE,MACRO_FAULT_WRITE,MACRO_FAULT_FLUSH,
        MACRO_FAULT_FSYNC,MACRO_FAULT_READBACK,MACRO_FAULT_BACKUP,MACRO_FAULT_REPLACE,MACRO_FAULT_CLEANUP,MACRO_FAULT_RESTORE};
    for(size_t i=0;i<sizeof(faults)/sizeof(faults[0]);i++) {
        strcpy(doc.description,"updated");assert(macro_store_save(root,&doc,true)==MACRO_STORE_OK);
        strcpy(doc.description,"new contents");
        macro_store_test_fault(faults[i],EIO);
        assert(macro_store_save(root,&doc,true)!=MACRO_STORE_OK);
        MacroStoreResult current=macro_store_load(root,"test macro",&loaded);
        bool old_in_destination=current==MACRO_STORE_OK && !strcmp(loaded.description,"updated");
        name_path("test macro.bak");
        MacroStoreResult backup_result=macro_store_validate_file(path,&loaded);
        bool old_in_backup=backup_result==MACRO_STORE_OK && !strcmp(loaded.description,"updated");
        assert(old_in_destination || old_in_backup);
        if(current==MACRO_STORE_NOT_FOUND && old_in_backup) {
            name_path("test macro.bak");char dst[MAX_PATH];snprintf(dst,sizeof(dst),"%s/test macro.rcm",root);
            assert(rename(path,dst)==0);
        }
        /* Only remove artifacts created by this test; the old document is valid. */
        name_path("test macro.tmp");if(remove(path)!=0) assert(errno==ENOENT);
        name_path("test macro.bak");if(remove(path)!=0) assert(errno==ENOENT);
    }
    strcpy(doc.description,"updated");assert(macro_store_save(root,&doc,true)==MACRO_STORE_OK);
    macro_store_test_fault(MACRO_FAULT_CREATE,ENOSPC);
    assert(macro_store_save(root,&doc,true)==MACRO_STORE_FULL);
    macro_store_test_fault(MACRO_FAULT_WRITE,ENOSPC);
    assert(macro_store_save(root,&doc,true)==MACRO_STORE_FULL);
    assert(macro_store_load(root,"test macro",&loaded)==MACRO_STORE_OK && !strcmp(loaded.description,"updated"));
    erase("test macro.tmp");
#endif
    assert(macro_store_duplicate(root,"Test Macro","Copy")==MACRO_STORE_OK);
    assert(macro_store_duplicate(root,"Test Macro","COPY")==MACRO_STORE_EXISTS);
    assert(macro_store_rename(root,"Copy","Renamed")==MACRO_STORE_OK);
    assert(macro_store_load(root,"copy",&loaded)==MACRO_STORE_NOT_FOUND);
    assert(macro_store_load(root,"renamed",&loaded)==MACRO_STORE_OK && !strcmp(loaded.name,"Renamed"));
    char names[3][MACRO_NAME_BYTES];size_t count=0;
    assert(macro_store_list(root,names,1,&count)==MACRO_STORE_BUFFER_SMALL && count==2);
    assert(macro_store_list(root,names,3,&count)==MACRO_STORE_OK && count==2);
    assert(!strcmp(names[0],"renamed") && !strcmp(names[1],"test macro"));
    /* Simulate interruption after destination -> backup, with a staged file. */
    name_path("test macro.rcm");char backup[MAX_PATH];snprintf(backup,sizeof(backup),"%s/test macro.bak",root);
    assert(rename(path,backup)==0);create_file("test macro.tmp",bytes,n);
    assert(macro_store_recover(root,"Test Macro")==MACRO_STORE_RECOVERY_REQUIRED);
    assert(macro_store_load(root,"Test Macro",&loaded)==MACRO_STORE_OK && !strcmp(loaded.description,"updated"));
    erase("test macro.tmp");assert(macro_store_recover(root,"Test Macro")==MACRO_STORE_OK);
    create_file("test macro.tmp","bad",3);
    assert(macro_store_save(root,&doc,true)==MACRO_STORE_RECOVERY_REQUIRED);
    assert(macro_store_delete(root,"test macro")==MACRO_STORE_RECOVERY_REQUIRED);
    erase("test macro.tmp");
    create_file("invalid.rcm",bytes,n-1);
    name_path("invalid.rcm");assert(macro_store_validate_file(path,&loaded)==MACRO_STORE_INVALID);
    erase("invalid.rcm");
    create_file("test macro.bak","bad",3);
    assert(macro_store_recover(root,"Test Macro")==MACRO_STORE_RECOVERY_REQUIRED);
    assert(macro_store_load(root,"Test Macro",&loaded)==MACRO_STORE_OK && !strcmp(loaded.description,"updated"));
    erase("test macro.bak");
    create_file("staged.tmp",bytes,n);
    assert(macro_store_recover(root,"staged")==MACRO_STORE_RECOVERY_REQUIRED);
    erase("staged.tmp");
    assert(macro_store_load("Z:/no-such-folder","missing",&loaded)!=MACRO_STORE_OK);
    assert(macro_store_delete(root,"renamed")==MACRO_STORE_OK);
    assert(macro_store_delete(root,"test macro")==MACRO_STORE_OK);
    assert(macro_store_delete(root,"test macro")==MACRO_STORE_NOT_FOUND);
    assert(_rmdir(root)==0);
    puts("PASS: macro codec, metadata, file operations and interrupted-save recovery.");
    return 0;
}
