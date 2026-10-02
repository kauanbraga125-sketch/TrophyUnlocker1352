#include "tracker.h"
#include <string.h>

static int find_context(const tu_tracker_t *t, int32_t context) {
    for (int i = 0; i < TU_MAX_CONTEXTS; ++i) {
        if (t->slots[i].created && t->slots[i].context == context) return i;
    }
    return -1;
}

static int find_free(const tu_tracker_t *t) {
    for (int i = 0; i < TU_MAX_CONTEXTS; ++i) {
        if (!t->slots[i].created) return i;
    }
    return -1;
}

static int first_registered(const tu_tracker_t *t) {
    for (int i = 0; i < TU_MAX_CONTEXTS; ++i) {
        if (t->slots[i].created && t->slots[i].registered) return i;
    }
    return -1;
}

void tu_tracker_init(tu_tracker_t *t) {
    if (!t) return;
    memset(t, 0, sizeof(*t));
}

void tu_tracker_on_create(tu_tracker_t *t, int32_t context, uint32_t service_label, int result) {
    if (!t || result < 0 || context <= 0) return;
    int i = find_context(t, context);
    if (i < 0) i = find_free(t);
    if (i < 0) return;
    t->slots[i].context = context;
    t->slots[i].handle = -1;
    t->slots[i].service_label = service_label;
    t->slots[i].created = true;
    t->slots[i].registered = false;
}

void tu_tracker_on_register(tu_tracker_t *t, int32_t context, int32_t handle, int result) {
    if (!t || result < 0 || handle <= 0) return;
    int i = find_context(t, context);
    if (i < 0) return;
    t->slots[i].handle = handle;
    t->slots[i].registered = true;
    if (!tu_tracker_active(t)) t->active_index = (unsigned)i;
}

void tu_tracker_clear_context(tu_tracker_t *t, int32_t context) {
    if (!t) return;
    int i = find_context(t, context);
    if (i < 0) return;
    memset(&t->slots[i], 0, sizeof(t->slots[i]));
    int f = first_registered(t);
    t->active_index = f >= 0 ? (unsigned)f : 0u;
}

void tu_tracker_clear_handle(tu_tracker_t *t, int32_t handle) {
    if (!t || handle <= 0) return;
    bool changed = false;
    for (int i = 0; i < TU_MAX_CONTEXTS; ++i) {
        if (t->slots[i].created && t->slots[i].registered && t->slots[i].handle == handle) {
            t->slots[i].handle = -1;
            t->slots[i].registered = false;
            changed = true;
        }
    }
    if (changed && !tu_tracker_active(t)) {
        int f = first_registered(t);
        t->active_index = f >= 0 ? (unsigned)f : 0u;
    }
}

size_t tu_tracker_registered_count(const tu_tracker_t *t) {
    if (!t) return 0;
    size_t n = 0;
    for (int i = 0; i < TU_MAX_CONTEXTS; ++i)
        if (t->slots[i].created && t->slots[i].registered) ++n;
    return n;
}

const tu_context_t *tu_tracker_active(const tu_tracker_t *t) {
    if (!t || t->active_index >= TU_MAX_CONTEXTS) return NULL;
    const tu_context_t *c = &t->slots[t->active_index];
    return (c->created && c->registered) ? c : NULL;
}

bool tu_tracker_select_next(tu_tracker_t *t) {
    if (!t || tu_tracker_registered_count(t) < 2) return false;
    unsigned start = t->active_index;
    for (unsigned step = 1; step <= TU_MAX_CONTEXTS; ++step) {
        unsigned i = (start + step) % TU_MAX_CONTEXTS;
        if (t->slots[i].created && t->slots[i].registered) {
            t->active_index = i;
            return true;
        }
    }
    return false;
}

bool tu_tracker_select_prev(tu_tracker_t *t) {
    if (!t || tu_tracker_registered_count(t) < 2) return false;
    unsigned start = t->active_index;
    for (unsigned step = 1; step <= TU_MAX_CONTEXTS; ++step) {
        unsigned i = (start + TU_MAX_CONTEXTS - step) % TU_MAX_CONTEXTS;
        if (t->slots[i].created && t->slots[i].registered) {
            t->active_index = i;
            return true;
        }
    }
    return false;
}

bool tu_flag_is_unlocked(const uint32_t flags[4], uint32_t trophy_id) {
    if (!flags || trophy_id >= TU_MAX_TROPHIES) return false;
    return (flags[trophy_id >> 5] & (1u << (trophy_id & 31u))) != 0;
}

tu_selection_status_t tu_validate_selection(const tu_context_t *ctx, uint32_t trophy_count,
                                             uint32_t trophy_id, const uint32_t flags[4]) {
    if (!ctx || !ctx->created || !ctx->registered || ctx->handle <= 0) return TU_SEL_NO_CONTEXT;
    if (trophy_count == 0 || trophy_count > TU_MAX_TROPHIES) return TU_SEL_BAD_COUNT;
    if (trophy_id >= trophy_count || trophy_id >= TU_MAX_TROPHIES) return TU_SEL_BAD_ID;
    if (tu_flag_is_unlocked(flags, trophy_id)) return TU_SEL_ALREADY_UNLOCKED;
    return TU_SEL_OK;
}
