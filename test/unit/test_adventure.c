#define _POSIX_C_SOURCE 200809L
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

static void test_lowercase_and_split(void) {
    char line[] = "  TAKE   Knife  ";
    const char *verb = NULL;
    const char *argument = NULL;
    lowercase(line);
    split_command(line, &verb, &argument);
    CHECK_EQ_STR(verb, "take");
    CHECK_EQ_STR(argument, "knife");
}

static void test_split_without_argument(void) {
    char line[] = "look";
    const char *verb = NULL;
    const char *argument = NULL;
    split_command(line, &verb, &argument);
    CHECK_EQ_STR(verb, "look");
    CHECK_EQ_STR(argument, "");
    char blank[] = "   ";
    split_command(blank, &verb, &argument);
    CHECK_EQ_STR(verb, "");
    CHECK_EQ_STR(argument, "");
}

static void test_find_command(void) {
    CHECK(find_command("help") != NULL);
    CHECK(find_command("clue") != NULL);
    CHECK(find_command("dance") == NULL);
    CHECK_EQ_INT(COMMAND_COUNT, 9);
}

static FILE *open_input(const char *text) {
    FILE *in = fmemopen((void *)text, strlen(text), "r");
    CHECK(in != NULL);
    return in;
}

static void test_read_line_strips_newline_and_reports_eof(void) {
    char buffer[LINE_CAPACITY];
    FILE *in = open_input("look\ngo north");
    CHECK(read_line(buffer, sizeof buffer, in));
    CHECK_EQ_STR(buffer, "look");
    CHECK(read_line(buffer, sizeof buffer, in));
    CHECK_EQ_STR(buffer, "go north");
    CHECK(!read_line(buffer, sizeof buffer, in));
    fclose(in);
}

static void test_read_line_drains_overlong_line(void) {
    char buffer[8];
    FILE *in = open_input("abcdefghijklmnop\nnext\n");
    CHECK(read_line(buffer, sizeof buffer, in));
    CHECK_EQ_STR(buffer, "abcdefg");
    CHECK(read_line(buffer, sizeof buffer, in));
    CHECK_EQ_STR(buffer, "next");
    fclose(in);
}

static void test_quit_sets_state(void) {
    Game game = {0};
    srand(1);
    CHECK(setup_game(&game));
    char line[] = "QUIT";
    run_command(&game, line);
    CHECK_EQ_INT(game.state, GAME_QUIT);
    teardown_game(&game);
}

int main(void) {
    test_setup_builds_a_complete_world();
    test_setup_is_reproducible_for_a_seed();
    test_teardown_tolerates_partial_setup();
    test_lowercase_and_split();
    test_split_without_argument();
    test_find_command();
    test_read_line_strips_newline_and_reports_eof();
    test_read_line_drains_overlong_line();
    test_quit_sets_state();
    CHECK_REPORT("test_adventure");
}
