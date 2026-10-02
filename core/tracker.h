#ifndef TU1352_TRACKER_H
#define TU1352_TRACKER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define TU_MAX_CONTEXTS 8
#define TU_MAX_TROPHIES 127

typedef struct {
    int32_t context;
    int32_t handle;
    uint32_t service_label;
    bool created;
    bool registered;
} tu_context_t;

typedef struct {
    tu_context_t slots[TU_MAX_CONTEXTS];
    unsigned active_index;
} tu_tracker_t;

typedef enum {
    TU_SEL_OK = 0,
    TU_SEL_NO_CONTEXT,
    TU_SEL_BAD_COUNT,
    TU_SEL_BAD_ID,
    TU_SEL_ALREADY_UNLOCKED
} tu_selection_status_t;

void tu_tracker_init(tu_tracker_t *t);
void tu_tracker_on_create(tu_tracker_t *t, int32_t context, uint32_t service_label, int result);
void tu_tracker_on_register(tu_tracker_t *t, int32_t context, int32_t handle, int result);
void tu_tracker_clear_context(tu_tracker_t *t, int32_t context);
void tu_tracker_clear_handle(tu_tracker_t *t, int32_t handle);
size_t tu_tracker_registered_count(const tu_tracker_t *t);
const tu_context_t *tu_tracker_active(const tu_tracker_t *t);
bool tu_tracker_select_next(tu_tracker_t *t);
bool tu_tracker_select_prev(tu_tracker_t *t);
bool tu_flag_is_unlocked(const uint32_t flags[4], uint32_t trophy_id);
tu_selection_status_t tu_validate_selection(const tu_context_t *ctx, uint32_t trophy_count,
                                             uint32_t trophy_id, const uint32_t flags[4]);

#endif
