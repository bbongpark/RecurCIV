#include "macro_editor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static MacroEditor editor;
static MacroDocument doc;
int main(void) {
    strcpy(doc.name,"Macro");doc.speed_percent=100;
    assert(macro_editor_open(&editor,&doc,true));assert(!macro_editor_dirty(&editor));
    MacroStep a={{0x084,8,128,128,255,0,0},100};
    MacroStep b={{0,2,1,2,3,4,0},500};
    assert(macro_editor_insert(&editor,0,&a));assert(macro_editor_dirty(&editor));
    assert(macro_editor_undo(&editor));assert(!macro_editor_dirty(&editor));
    assert(macro_editor_redo(&editor));assert(macro_editor_dirty(&editor));
    assert(macro_editor_insert(&editor,1,&b));assert(macro_editor_move(&editor,1,0));
    assert(editor.current.sequence.steps[0].report.hat==2);
    assert(macro_editor_duplicate(&editor,1));assert(editor.current.sequence.duration_ms==700);
    assert(macro_editor_remove(&editor,0));assert(editor.current.sequence.duration_ms==200);
    assert(macro_editor_replace(&editor,0,&b));assert(editor.current.sequence.duration_ms==600);
    macro_editor_mark_saved(&editor);assert(!macro_editor_dirty(&editor));
    assert(macro_editor_undo(&editor));assert(macro_editor_dirty(&editor));
    assert(macro_editor_redo(&editor));assert(!macro_editor_dirty(&editor));
    doc=editor.current;strcpy(doc.description,"Changed");
    assert(macro_editor_change(&editor,&doc));assert(macro_editor_dirty(&editor));
    assert(macro_editor_undo(&editor));assert(!macro_editor_dirty(&editor));
    a.duration_ms=0;assert(!macro_editor_insert(&editor,0,&a));assert(!macro_editor_dirty(&editor));
    assert(macro_editor_redo(&editor));
    puts("PASS: macro editor operations, metadata, bounded undo/redo and dirty tracking.");
    return 0;
}
