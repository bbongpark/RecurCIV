#pragma once
#include "civ_payload.h"
#include "switch_gamepad.h"
#include "text.h"

/* Bounds for borrowed input, not an expanded macro allocation. */
#define CIV_RUNNER_MAX_BYTES 32768u
#define CIV_RUNNER_MAX_LINES 128u
#define CIV_RUNNER_MAX_LINE_BYTES 253u

typedef enum {
    CIV_STAGE_START_DELAY, CIV_STAGE_PREAMBLE_A, CIV_STAGE_PREAMBLE_RELEASE,
    CIV_STAGE_ESCAPE, CIV_STAGE_ESCAPE_SETTLE, CIV_STAGE_ACTIVATE_A,
    CIV_STAGE_ACTIVATE_SETTLE, CIV_STAGE_CHARACTER, CIV_STAGE_REPEAT_RELEASE,
    CIV_STAGE_ENTER_1, CIV_STAGE_ENTER_GAP, CIV_STAGE_ENTER_2,
    CIV_STAGE_LINE_SETTLE, CIV_STAGE_SEPARATOR_A, CIV_STAGE_SEPARATOR_SETTLE,
    CIV_STAGE_NEUTRAL
} CivRunnerStage;
typedef enum {
    CIV_RUNNER_IDLE, CIV_RUNNER_RUNNING, CIV_RUNNER_CLEANING_UP,
    CIV_RUNNER_CLEANUP_BLOCKED, CIV_RUNNER_FINISHED, CIV_RUNNER_CANCELLED,
    CIV_RUNNER_FAILED, CIV_RUNNER_INTERRUPTED
} CivRunnerStatus;
typedef enum {
    CIV_ERROR_NONE, CIV_ERROR_SUBMISSION, CIV_ERROR_COMPLETION,
    CIV_ERROR_TRANSPORT, CIV_ERROR_EXTERNAL, CIV_ERROR_TICKET_EXHAUSTED
} CivRunnerError;
enum { CIV_INTERFACE_KEYBOARD=1u, CIV_INTERFACE_GAMEPAD=2u, CIV_INTERFACE_BOTH=3u };

typedef struct {
    uint64_t ticket;
    Stroke keyboard;
    SwitchGamepadReport gamepad;
    uint32_t minimum_ms;
    CivRunnerStage stage;
} CivRunnerOutput;
typedef enum { CIV_TRANSFER_OK, CIV_TRANSFER_FAILED, CIV_TRANSFER_INTERRUPTED } CivTransferResult;
typedef struct {
    uint64_t ticket;
    unsigned completed; /* Successful interface completions; may arrive separately. */
    CivTransferResult result;
} CivRunnerCompletion;

/* All callbacks MUST be nonblocking. This is a logical two-interface transaction,
   not a live USB adapter. Phase 2 must hold exclusive application ownership.
   submit copies the output and accepts it once; false is treated as ambiguous
   failure, never a reason to retry input. poll returns token-matched successful
   completions (even for neutral/unchanged halves). idle is true only after every
   old transaction is completed or fenced/drained. abort requests that fencing;
   it must prevent old queued output being sent after idle becomes true.
   No callback may re-enter the runner. */
typedef struct {
    void *context;
    bool (*ready)(void *context); /* Both HID interfaces ready. */
    bool (*idle)(void *context);
    bool (*submit)(void *context,const CivRunnerOutput *output);
    CivRunnerCompletion (*poll)(void *context,uint64_t ticket);
    void (*abort)(void *context,uint64_t ticket);
} CivRunnerTransport;

typedef struct {
    CivRunnerStatus status;
    CivRunnerStage stage;
    CivRunnerError error;
    size_t line,lines_total,lines_completed; /* line is 1-based after Start. */
    size_t character,line_length,characters_completed,characters_total;
    uint64_t ticket;
    bool pending,neutral_confirmed;
} CivRunnerProgress;

/* Stack/static allocation; initialize ONCE while inactive. Do not copy an active
   instance or mutate its borrowed payload. Tickets persist across starts. */
typedef struct {
    CivRunnerTransport transport;
    CivPayload payload;
    CivRunnerProgress progress;
    CivRunnerStatus outcome;
    CivRunnerOutput output;
    size_t line_start,line_end,position;
    uint64_t next_ticket,completed_at;
    unsigned preamble_count,completed_interfaces;
    bool action_live,waiting,initialized;
} CivRunner;

bool civ_runner_init(CivRunner *runner,const CivRunnerTransport *transport);
/* Preflight is bounded; rejection emits nothing and leaves the instance intact.
   CR/LF delimiters are not typed. Empty/whitespace-only lines, overlong lines,
   non-ASCII/control bytes (except Tab) and unsupported characters are rejected. */
bool civ_runner_start(CivRunner *runner,CivPayload payload);
void civ_runner_tick(CivRunner *runner,uint64_t now_ms);
void civ_runner_cancel(CivRunner *runner);
void civ_runner_fail(CivRunner *runner);
void civ_runner_interrupt(CivRunner *runner);
/* Explicit retry of NEUTRAL CLEANUP ONLY after a cleanup submission/completion
   failure. Never resumes/retries payload input. Ordinary reconnect also performs
   only cleanup, not continuation. */
bool civ_runner_retry_cleanup(CivRunner *runner);
bool civ_runner_active(const CivRunner *runner);
CivRunnerProgress civ_runner_progress(const CivRunner *runner);
