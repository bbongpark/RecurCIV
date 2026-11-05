#pragma once
#include "macro_storage.h"

/* One-level undo/redo. Caller owns this bounded object; no hidden allocations. */
typedef struct {
    MacroDocument current, previous, saved;
    bool has_previous, previous_is_redo, has_saved;
} MacroEditor;

bool macro_editor_open(MacroEditor *editor,const MacroDocument *document,bool saved);
bool macro_editor_change(MacroEditor *editor,const MacroDocument *document);
bool macro_editor_insert(MacroEditor *editor,size_t index,const MacroStep *step);
bool macro_editor_replace(MacroEditor *editor,size_t index,const MacroStep *step);
bool macro_editor_remove(MacroEditor *editor,size_t index);
bool macro_editor_duplicate(MacroEditor *editor,size_t index);
bool macro_editor_move(MacroEditor *editor,size_t from,size_t to);
bool macro_editor_undo(MacroEditor *editor);
bool macro_editor_redo(MacroEditor *editor);
bool macro_editor_dirty(const MacroEditor *editor);
void macro_editor_mark_saved(MacroEditor *editor);
