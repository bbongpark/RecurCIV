#include "macro_model.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    static MacroSequence s;
    MacroStep a={{0x84,8,128,128,255,0,0},100};
    MacroStep b={{0,2,128,128,128,128,0},500};
    macro_sequence_init(&s);
    assert(macro_sequence_valid(&s));
    assert(macro_sequence_insert(&s,0,&a));
    assert(macro_sequence_insert(&s,1,&b));
    assert(macro_sequence_duplicate(&s,0));
    assert(s.count==3 && s.duration_ms==700 && s.steps[1].report.buttons==0x84);
    assert(macro_sequence_move(&s,2,0));
    assert(s.steps[0].report.hat==2 && s.steps[1].report.rx==255);
    assert(macro_sequence_replace(&s,1,&b));
    assert(s.duration_ms==1100);
    assert(macro_sequence_remove(&s,0));
    assert(s.count==2 && s.duration_ms==600 && macro_sequence_valid(&s));
    b.report.hat=9;assert(!macro_sequence_insert(&s,0,&b));
    b.report.hat=8;b.report.buttons=0x1000;assert(!macro_sequence_insert(&s,0,&b));
    b.report.buttons=0;b.report.vendor=1;assert(!macro_sequence_insert(&s,0,&b));
    b.report.vendor=0;b.duration_ms=0;assert(!macro_sequence_insert(&s,0,&b));
    assert(macro_sequence_valid(&s));
    puts("PASS: bounded macro snapshots, combinations, analogs and editing.");
    return 0;
}
