#define main adventure_main
int adventure_main(void);
#include "../../src/adventure.c"
#undef main
#include "check.h"

static size_t total_items_in_rooms(const Game *game) {
    size_t total = 0;
    for (size_t i = 0; i < ROOM_COUNT; i++) {
        total += count_items(game->rooms[i]->items);
    }
    return total;
}

static void test_setup_builds_a_complete_world(void) {
    Game game = {0};
    srand(7);
    CHECK(setup_game(&game));
    for (size_t i = 0; i < ROOM_COUNT; i++) {
        CHECK(find_room(game.rooms, ROOM_COUNT, ROOM_NAMES[i]) != NULL);
        CHECK(count_items(game.rooms[i]->items) <= 1);
    }
    CHECK_EQ_INT(total_items_in_rooms(&game), ITEM_COUNT);
    for (size_t i = 0; i < CHARACTER_COUNT; i++) {
        CHECK(game.characters[i]->room != NULL);
        CHECK_EQ_STR(game.characters[i]->name, CHARACTER_NAMES[i]);
    }
    CHECK(game.avatar != NULL && game.avatar->room != NULL);
    CHECK(game.avatar->inventory == NULL);
    CHECK(game.answer_room != NULL);
    CHECK(game.answer_item != NULL);
    CHECK(game.answer_character != NULL);
    CHECK_EQ_INT(game.clues_used, 0);
    CHECK_EQ_INT(game.state, GAME_PLAYING);
    teardown_game(&game);
    CHECK(game.avatar == NULL);
}

static void test_setup_is_reproducible_for_a_seed(void) {
    Game first = {0};
    Game second = {0};
    srand(42);
    CHECK(setup_game(&first));
    srand(42);
    CHECK(setup_game(&second));
    for (size_t i = 0; i < ROOM_COUNT; i++) {
        CHECK_EQ_STR(first.rooms[i]->name, second.rooms[i]->name);
    }
    CHECK_EQ_STR(first.answer_item, second.answer_item);
    CHECK_EQ_STR(first.answer_room->name, second.answer_room->name);
    teardown_game(&first);
    teardown_game(&second);
}

static void test_teardown_tolerates_partial_setup(void) {
    Game game = {0};
    teardown_game(&game);
    game.rooms[0] = create_room(ROOM_NAMES[0], ROOM_DESCRIPTIONS[0]);
    teardown_game(&game);
    CHECK(game.rooms[0] == NULL);
}

int main(void) {
    test_setup_builds_a_complete_world();
    test_setup_is_reproducible_for_a_seed();
    test_teardown_tolerates_partial_setup();
    CHECK_REPORT("test_adventure");
}
