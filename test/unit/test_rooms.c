#include "../../src/rooms.h"
#include "check.h"
#include "helpers.h"
#include <stdbool.h>
#include <stdlib.h>

enum { SIDE = 3, COUNT = SIDE * SIDE };

static const char *const NAMES[COUNT] = {
    "a", "b", "c", "d", "e", "f", "g", "h", "i"
};

static void build(Room *rooms[]) {
    for (size_t i = 0; i < COUNT; i++) {
        rooms[i] = create_room(NAMES[i], "desc");
    }
    link_grid(rooms, SIDE);
}

static void destroy(Room *rooms[]) {
    for (size_t i = 0; i < COUNT; i++) {
        free_room(rooms[i]);
    }
}

static int exit_count(const Room *room) {
    int count = 0;
    for (int d = 0; d < DIRECTION_COUNT; d++) {
        if (room_in_direction(room, (Direction)d) != NULL) {
            count++;
        }
    }
    return count;
}

static void test_corner_edge_centre_exit_counts(void) {
    Room *rooms[COUNT];
    build(rooms);
    CHECK_EQ_INT(exit_count(rooms[0]), 2);
    CHECK_EQ_INT(exit_count(rooms[2]), 2);
    CHECK_EQ_INT(exit_count(rooms[6]), 2);
    CHECK_EQ_INT(exit_count(rooms[8]), 2);
    CHECK_EQ_INT(exit_count(rooms[1]), 3);
    CHECK_EQ_INT(exit_count(rooms[3]), 3);
    CHECK_EQ_INT(exit_count(rooms[5]), 3);
    CHECK_EQ_INT(exit_count(rooms[7]), 3);
    CHECK_EQ_INT(exit_count(rooms[4]), 4);
    destroy(rooms);
}

static void test_links_are_symmetric(void) {
    Room *rooms[COUNT];
    build(rooms);
    CHECK(rooms[4]->exits[DIRECTION_NORTH] == rooms[1]);
    CHECK(rooms[1]->exits[DIRECTION_SOUTH] == rooms[4]);
    CHECK(rooms[4]->exits[DIRECTION_EAST] == rooms[5]);
    CHECK(rooms[5]->exits[DIRECTION_WEST] == rooms[4]);
    CHECK(rooms[0]->exits[DIRECTION_NORTH] == NULL);
    CHECK(rooms[0]->exits[DIRECTION_WEST] == NULL);
    CHECK(room_in_direction(rooms[0], DIRECTION_NONE) == NULL);
    destroy(rooms);
}

static void visit(const Room *room, Room *const rooms[], bool seen[]) {
    for (size_t i = 0; i < COUNT; i++) {
        if (rooms[i] == room) {
            if (seen[i]) {
                return;
            }
            seen[i] = true;
        }
    }
    for (int d = 0; d < DIRECTION_COUNT; d++) {
        const Room *next = room_in_direction(room, (Direction)d);
        if (next != NULL) {
            visit(next, rooms, seen);
        }
    }
}

static size_t pick_zero(size_t bound) {
    (void)bound;
    return 0;
}

static size_t pick_last(size_t bound) {
    return bound - 1;
}

static void test_every_room_reachable(void) {
    Room *rooms[COUNT];
    for (size_t i = 0; i < COUNT; i++) {
        rooms[i] = create_room(NAMES[i], "desc");
    }
    shuffle_rooms(rooms, COUNT, pick_zero);
    link_grid(rooms, SIDE);
    bool seen[COUNT] = {false};
    visit(rooms[4], rooms, seen);
    for (size_t i = 0; i < COUNT; i++) {
        CHECK(seen[i]);
    }
    destroy(rooms);
}

static void test_shuffle_is_deterministic_for_a_given_picker(void) {
    Room *rooms[3];
    for (size_t i = 0; i < 3; i++) {
        rooms[i] = create_room(NAMES[i], "desc");
    }
    shuffle_rooms(rooms, 3, pick_last);
    CHECK_EQ_STR(rooms[0]->name, "a");
    CHECK_EQ_STR(rooms[1]->name, "b");
    CHECK_EQ_STR(rooms[2]->name, "c");
    shuffle_rooms(rooms, 3, pick_zero);
    CHECK_EQ_STR(rooms[0]->name, "b");
    CHECK_EQ_STR(rooms[1]->name, "c");
    CHECK_EQ_STR(rooms[2]->name, "a");
    for (size_t i = 0; i < 3; i++) {
        free_room(rooms[i]);
    }
}

static void test_direction_parsing_and_names(void) {
    CHECK_EQ_INT(parse_direction("north"), DIRECTION_NORTH);
    CHECK_EQ_INT(parse_direction("west"), DIRECTION_WEST);
    CHECK_EQ_INT(parse_direction("up"), DIRECTION_NONE);
    CHECK_EQ_INT(parse_direction(""), DIRECTION_NONE);
    CHECK_EQ_STR(direction_name(DIRECTION_SOUTH), "south");
    CHECK_EQ_STR(direction_name(DIRECTION_EAST), "east");
}

static void test_find_room_and_free_with_items(void) {
    Room *rooms[COUNT];
    build(rooms);
    CHECK(find_room(rooms, COUNT, "e") == rooms[4]);
    CHECK(find_room(rooms, COUNT, "zzz") == NULL);
    add_item(&rooms[0]->items, create_item("knife"));
    add_item(&rooms[0]->items, create_item("rope"));
    CHECK_EQ_INT(count_items(rooms[0]->items), 2);
    destroy(rooms);
    free_room(NULL);
}

int main(void) {
    test_corner_edge_centre_exit_counts();
    test_links_are_symmetric();
    test_every_room_reachable();
    test_shuffle_is_deterministic_for_a_given_picker();
    test_direction_parsing_and_names();
    test_find_room_and_free_with_items();
    CHECK_REPORT("test_rooms");
}
