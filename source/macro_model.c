#include "macro_model.h"
#include <string.h>

bool macro_step_valid(const MacroStep *s) {
    return s && s->duration_ms && s->duration_ms<=MACRO_MAX_STEP_MS &&
        !(s->report.buttons & 0xf000u) && s->report.hat<=8 && s->report.vendor==0;
}
bool macro_sequence_valid(const MacroSequence *s) {
    if(!s || s->count>MACRO_MAX_STEPS) return false;
    uint64_t total=0;
    for(size_t i=0;i<s->count;i++) {
        if(!macro_step_valid(&s->steps[i])) return false;
        total+=s->steps[i].duration_ms;
        if(total>MACRO_MAX_TOTAL_MS) return false;
    }
    return total==s->duration_ms;
}
void macro_sequence_init(MacroSequence *s) { if(s) memset(s,0,sizeof(*s)); }
bool macro_sequence_insert(MacroSequence *s,size_t i,const MacroStep *step) {
    if(!macro_sequence_valid(s) || !macro_step_valid(step) || s->count>=MACRO_MAX_STEPS || i>s->count ||
        s->duration_ms+step->duration_ms>MACRO_MAX_TOTAL_MS) return false;
    /* Copy first: callers may insert a step already inside the sequence. */
    MacroStep copy=*step;
    memmove(&s->steps[i+1],&s->steps[i],(s->count-i)*sizeof(copy));
    s->steps[i]=copy;s->count++;s->duration_ms+=copy.duration_ms;return true;
}
bool macro_sequence_replace(MacroSequence *s,size_t i,const MacroStep *step) {
    if(!macro_sequence_valid(s) || i>=s->count || !macro_step_valid(step)) return false;
    uint64_t total=s->duration_ms-s->steps[i].duration_ms+step->duration_ms;
    if(total>MACRO_MAX_TOTAL_MS) return false;
    s->steps[i]=*step;s->duration_ms=total;return true;
}
bool macro_sequence_remove(MacroSequence *s,size_t i) {
    if(!macro_sequence_valid(s) || i>=s->count) return false;
    s->duration_ms-=s->steps[i].duration_ms;s->count--;
    memmove(&s->steps[i],&s->steps[i+1],(s->count-i)*sizeof(s->steps[0]));return true;
}
bool macro_sequence_move(MacroSequence *s,size_t from,size_t to) {
    if(!macro_sequence_valid(s) || from>=s->count || to>=s->count) return false;
    MacroStep step=s->steps[from];
    if(from<to) memmove(&s->steps[from],&s->steps[from+1],(to-from)*sizeof(step));
    else if(from>to) memmove(&s->steps[to+1],&s->steps[to],(from-to)*sizeof(step));
    s->steps[to]=step;return true;
}
bool macro_sequence_duplicate(MacroSequence *s,size_t i) {
    return macro_sequence_valid(s) && i<s->count && macro_sequence_insert(s,i+1,&s->steps[i]);
}
