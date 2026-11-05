#include "macro_editor.h"
#include <string.h>

static bool equal_documents(const MacroDocument *a,const MacroDocument *b) {
    if(strcmp(a->name,b->name) || strcmp(a->description,b->description) ||
       a->created_unix!=b->created_unix || a->modified_unix!=b->modified_unix ||
       a->speed_percent!=b->speed_percent || a->repeat_count!=b->repeat_count ||
       a->loop_delay_ms!=b->loop_delay_ms || a->sequence.count!=b->sequence.count ||
       a->sequence.duration_ms!=b->sequence.duration_ms) return false;
    for(size_t i=0;i<a->sequence.count;i++) {
        const MacroStep *x=&a->sequence.steps[i],*y=&b->sequence.steps[i];
        if(x->duration_ms!=y->duration_ms || memcmp(&x->report,&y->report,sizeof(x->report))) return false;
    }
    return true;
}
bool macro_editor_open(MacroEditor *e,const MacroDocument *d,bool saved) {
    if(!e || !macro_document_valid(d)) return false;
    /* d may alias editor storage. */
    MacroDocument copy=*d;memset(e,0,sizeof(*e));e->current=copy;
    if(saved) { e->saved=copy;e->has_saved=true; }
    return true;
}
bool macro_editor_change(MacroEditor *e,const MacroDocument *d) {
    if(!e || !macro_document_valid(d)) return false;
    if(equal_documents(&e->current,d)) return true;
    MacroDocument copy=*d;e->previous=e->current;e->current=copy;
    e->has_previous=true;e->previous_is_redo=false;return true;
}
bool macro_editor_insert(MacroEditor *e,size_t i,const MacroStep *s) {
    if(!e) return false;
    MacroDocument next=e->current;
    return macro_sequence_insert(&next.sequence,i,s) && macro_editor_change(e,&next);
}
bool macro_editor_replace(MacroEditor *e,size_t i,const MacroStep *s) {
    if(!e) return false;
    MacroDocument next=e->current;
    return macro_sequence_replace(&next.sequence,i,s) && macro_editor_change(e,&next);
}
bool macro_editor_remove(MacroEditor *e,size_t i) {
    if(!e) return false;
    MacroDocument next=e->current;
    return macro_sequence_remove(&next.sequence,i) && macro_editor_change(e,&next);
}
bool macro_editor_duplicate(MacroEditor *e,size_t i) {
    if(!e) return false;
    MacroDocument next=e->current;
    return macro_sequence_duplicate(&next.sequence,i) && macro_editor_change(e,&next);
}
bool macro_editor_move(MacroEditor *e,size_t from,size_t to) {
    if(!e) return false;
    MacroDocument next=e->current;
    return macro_sequence_move(&next.sequence,from,to) && macro_editor_change(e,&next);
}
static void swap_history(MacroEditor *e) {
    MacroDocument tmp=e->current;e->current=e->previous;e->previous=tmp;
    e->previous_is_redo=!e->previous_is_redo;
}
bool macro_editor_undo(MacroEditor *e) {
    if(!e || !e->has_previous || e->previous_is_redo) return false;
    swap_history(e);return true;
}
bool macro_editor_redo(MacroEditor *e) {
    if(!e || !e->has_previous || !e->previous_is_redo) return false;
    swap_history(e);return true;
}
bool macro_editor_dirty(const MacroEditor *e) {
    return e && (!e->has_saved || !equal_documents(&e->current,&e->saved));
}
void macro_editor_mark_saved(MacroEditor *e) {
    if(e) { e->saved=e->current;e->has_saved=true; }
}
