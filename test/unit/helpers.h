#ifndef HELPERS_H
#define HELPERS_H

#include <stddef.h>
#include <string.h>

#include "../../src/items.h"
#include "../../src/rooms.h"

/* Test-only list and board queries the program itself never needs. */

static inline size_t count_items(const Item *list) {
    size_t count = 0;
    for (const Item *item = list; item != NULL; item = item->next) {
        count++;
    }
    return count;
}

static inline Room *find_room(Room *const rooms[], size_t count, const char *name) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(rooms[i]->name, name) == 0) {
            return rooms[i];
        }
    }
    return NULL;
}

#endif
