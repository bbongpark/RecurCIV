#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "switch_gamepad.h"

/* Full wire-state snapshots; no unsupported descriptor buttons or vendor data. */
#define MACRO_MAX_STEPS 1024u
#define MACRO_MAX_STEP_MS 3600000u
#define MACRO_MAX_TOTAL_MS 86400000u
typedef struct { SwitchGamepadReport report; uint32_t duration_ms; } MacroStep;
typedef struct { MacroStep steps[MACRO_MAX_STEPS]; size_t count; uint64_t duration_ms; } MacroSequence;

bool macro_step_valid(const MacroStep *step);
bool macro_sequence_valid(const MacroSequence *sequence);
void macro_sequence_init(MacroSequence *sequence);
bool macro_sequence_insert(MacroSequence *sequence, size_t index, const MacroStep *step);
bool macro_sequence_replace(MacroSequence *sequence, size_t index, const MacroStep *step);
bool macro_sequence_remove(MacroSequence *sequence, size_t index);
bool macro_sequence_move(MacroSequence *sequence, size_t from, size_t to);
bool macro_sequence_duplicate(MacroSequence *sequence, size_t index);
