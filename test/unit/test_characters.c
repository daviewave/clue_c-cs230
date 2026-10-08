#include "../../src/characters.h"
#include "check.h"
#include "helpers.h"
#include <stdlib.h>

static void test_create_starts_nowhere_with_empty_inventory(void) {
    Character *scarlet = create_character("scarlet");
    CHECK(scarlet != NULL);
    CHECK_EQ_STR(scarlet->name, "scarlet");
    CHECK(scarlet->room == NULL);
    CHECK(scarlet->inventory == NULL);
    free_character(scarlet);
}

static void test_move_between_rooms(void) {
    Room *kitchen = create_room("kitchen", "k");
    Room *hall = create_room("hall", "h");
    Character *plum = create_character("plum");
    move_character(plum, kitchen);
    CHECK(plum->room == kitchen);
    move_character(plum, hall);
    CHECK(plum->room == hall);
    move_character(plum, hall);
    CHECK(plum->room == hall);
    free_character(plum);
    free_room(kitchen);
    free_room(hall);
}

static void test_find_character(void) {
    Character *cast[2] = { create_character("white"), create_character("green") };
    CHECK(find_character(cast, 2, "green") == cast[1]);
    CHECK(find_character(cast, 2, "plum") == NULL);
    CHECK(find_character(cast, 0, "white") == NULL);
    free_character(cast[0]);
    free_character(cast[1]);
}

static void test_free_releases_inventory(void) {
    Character *avatar = create_character("you");
    add_item(&avatar->inventory, create_item("rope"));
    add_item(&avatar->inventory, create_item("wrench"));
    CHECK_EQ_INT(count_items(avatar->inventory), 2);
    free_character(avatar);
    free_character(NULL);
}

int main(void) {
    test_create_starts_nowhere_with_empty_inventory();
    test_move_between_rooms();
    test_find_character();
    test_free_releases_inventory();
    CHECK_REPORT("test_characters");
}
