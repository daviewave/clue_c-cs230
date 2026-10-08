#include "rooms.h"

#include <stdlib.h>
#include <string.h>

static const char *const DIRECTION_NAMES[DIRECTION_COUNT] = {
    "north", "south", "east", "west"
};

/* Allocates a room with no exits and no items.
 * @return the new room, or NULL when memory is exhausted. */
Room *create_room(const char *name, const char *description) {
    Room *room = calloc(1, sizeof *room);
    if (room != NULL) {
        room->name = name;
        room->description = description;
    }
    return room;
}

/* @return the room at (row, column) of a row-major side x side array. */
static Room *room_at(Room *rooms[], size_t side, size_t row, size_t column) {
    return rooms[row * side + column];
}

/* Points the four exits of the room at (row, column) at its grid neighbours. */
static void link_room(Room *rooms[], size_t side, size_t row, size_t column) {
    Room *room = room_at(rooms, side, row, column);
    room->exits[DIRECTION_NORTH] = row > 0 ? room_at(rooms, side, row - 1, column) : NULL;
    room->exits[DIRECTION_SOUTH] = row + 1 < side ? room_at(rooms, side, row + 1, column) : NULL;
    room->exits[DIRECTION_WEST] = column > 0 ? room_at(rooms, side, row, column - 1) : NULL;
    room->exits[DIRECTION_EAST] = column + 1 < side ? room_at(rooms, side, row, column + 1) : NULL;
}

/* Wires rooms laid out row-major as a side x side grid; edge rooms get NULL off the map. */
void link_grid(Room *rooms[], size_t side) {
    for (size_t row = 0; row < side; row++) {
        for (size_t column = 0; column < side; column++) {
            link_room(rooms, side, row, column);
        }
    }
}

/* Fisher-Yates shuffle of the pointer array; see docs/design.md section 2. */
void shuffle_rooms(Room *rooms[], size_t count, RandomPicker pick) {
    for (size_t remaining = count; remaining > 1; remaining--) {
        size_t chosen = pick(remaining);
        Room *swap = rooms[remaining - 1];
        rooms[remaining - 1] = rooms[chosen];
        rooms[chosen] = swap;
    }
}

/* @return the room behind that exit, or NULL for a wall or an invalid direction. */
Room *room_in_direction(const Room *room, Direction direction) {
    return direction < DIRECTION_COUNT ? room->exits[direction] : NULL;
}

/* @return the direction spelled by word, or DIRECTION_NONE. */
Direction parse_direction(const char *word) {
    for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
        if (strcmp(DIRECTION_NAMES[direction], word) == 0) {
            return (Direction)direction;
        }
    }
    return DIRECTION_NONE;
}

/* @return the lowercase name of a direction. */
const char *direction_name(Direction direction) {
    return direction < DIRECTION_COUNT ? DIRECTION_NAMES[direction] : "nowhere";
}

/* Frees the room and every item still lying in it. */
void free_room(Room *room) {
    if (room != NULL) {
        free_items(room->items);
        free(room);
    }
}
