#ifndef ROOMS_H
#define ROOMS_H

#include <stddef.h>

#include "items.h"

/* The four exits in display order; DIRECTION_NONE is the parse failure value. */
typedef enum {
    DIRECTION_NORTH,
    DIRECTION_SOUTH,
    DIRECTION_EAST,
    DIRECTION_WEST,
    DIRECTION_COUNT,
    DIRECTION_NONE
} Direction;

/* A location on the 3x3 board; exits[d] is NULL where the map ends. */
typedef struct Room {
    const char *name;
    const char *description;
    struct Room *exits[DIRECTION_COUNT];
    Item *items;
} Room;

/* A source of random indexes in [0, bound), injected so tests stay deterministic. */
typedef size_t (*RandomPicker)(size_t bound);

Room *create_room(const char *name, const char *description);
void link_grid(Room *rooms[], size_t side);
void shuffle_rooms(Room *rooms[], size_t count, RandomPicker pick);
Room *room_in_direction(const Room *room, Direction direction);
Direction parse_direction(const char *word);
const char *direction_name(Direction direction);
Room *find_room(Room *const rooms[], size_t count, const char *name);
void free_room(Room *room);

#endif
