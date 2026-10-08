#define _POSIX_C_SOURCE 200809L
#define main adventure_main
int adventure_main(void);
#include "../../src/adventure.c"
#undef main
#include "check.h"
#include "helpers.h"

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

static Room *room_with_item(Game *game) {
    for (size_t i = 0; i < ROOM_COUNT; i++) {
        if (game->rooms[i]->items != NULL) {
            return game->rooms[i];
        }
    }
    return NULL;
}

static void test_go_follows_pointers_and_refuses_walls(void) {
    Game game = {0};
    srand(3);
    CHECK(setup_game(&game));
    move_character(game.avatar, game.rooms[0]);
    char north[] = "go north";
    run_command(&game, north);
    CHECK(game.avatar->room == game.rooms[0]);
    char east[] = "go east";
    run_command(&game, east);
    CHECK(game.avatar->room == game.rooms[1]);
    char up[] = "go up";
    run_command(&game, up);
    CHECK(game.avatar->room == game.rooms[1]);
    char bare[] = "go";
    run_command(&game, bare);
    CHECK(game.avatar->room == game.rooms[1]);
    teardown_game(&game);
}

static void test_take_and_drop_round_trip(void) {
    Game game = {0};
    srand(3);
    CHECK(setup_game(&game));
    Room *room = room_with_item(&game);
    const char *name = room->items->name;
    move_character(game.avatar, room);
    char take[LINE_CAPACITY];
    snprintf(take, sizeof take, "take %s", name);
    run_command(&game, take);
    CHECK(room->items == NULL);
    CHECK(find_item(game.avatar->inventory, name) != NULL);
    char drop[LINE_CAPACITY];
    snprintf(drop, sizeof drop, "drop %s", name);
    run_command(&game, drop);
    CHECK(game.avatar->inventory == NULL);
    CHECK(find_item(room->items, name) != NULL);
    teardown_game(&game);
}

static void test_take_and_drop_refuse_wrong_list(void) {
    Game game = {0};
    srand(3);
    CHECK(setup_game(&game));
    Room *room = room_with_item(&game);
    const char *name = room->items->name;
    move_character(game.avatar, room);
    char drop[LINE_CAPACITY];
    snprintf(drop, sizeof drop, "drop %s", name);
    run_command(&game, drop);
    CHECK_EQ_INT(count_items(room->items), 1);
    CHECK(game.avatar->inventory == NULL);
    char take[LINE_CAPACITY];
    snprintf(take, sizeof take, "take %s", name);
    run_command(&game, take);
    char again[LINE_CAPACITY];
    snprintf(again, sizeof again, "take %s", name);
    run_command(&game, again);
    CHECK_EQ_INT(count_items(game.avatar->inventory), 1);
    CHECK(room->items == NULL);
    char ghost[] = "take unicorn";
    run_command(&game, ghost);
    CHECK_EQ_INT(count_items(game.avatar->inventory), 1);
    teardown_game(&game);
}

typedef enum { ITEM_ELSEWHERE, ITEM_IN_ROOM, ITEM_CARRIED } ItemPlacement;

static Item **list_for(Game *game, ItemPlacement placement, Room *elsewhere) {
    if (placement == ITEM_CARRIED) {
        return &game->avatar->inventory;
    }
    return placement == ITEM_IN_ROOM ? &game->avatar->room->items : &elsewhere->items;
}

static void rig_answer(Game *game, bool room, bool character, ItemPlacement item) {
    Room *here = game->avatar->room;
    Room *elsewhere = here == game->rooms[0] ? game->rooms[1] : game->rooms[0];
    game->answer_room = room ? here : elsewhere;
    game->answer_character = game->characters[0];
    move_character(game->characters[0], character ? here : elsewhere);
    game->answer_item = ITEM_NAMES[0];
    Item *node = NULL;
    for (size_t i = 0; i < ROOM_COUNT && node == NULL; i++) {
        node = drop_item(&game->rooms[i]->items, ITEM_NAMES[0]);
    }
    add_item(list_for(game, item, elsewhere), node);
}

static void test_evaluate_clue_reports_each_match(void) {
    Game game = {0};
    srand(5);
    CHECK(setup_game(&game));
    rig_answer(&game, true, false, ITEM_CARRIED);
    ClueResult result = evaluate_clue(&game);
    CHECK(result.room);
    CHECK(!result.character);
    CHECK(result.item);
    teardown_game(&game);
}

static void test_evaluate_clue_sees_item_lying_in_the_room(void) {
    Game game = {0};
    srand(5);
    CHECK(setup_game(&game));
    rig_answer(&game, false, true, ITEM_IN_ROOM);
    ClueResult result = evaluate_clue(&game);
    CHECK(!result.room);
    CHECK(result.character);
    CHECK(result.item);
    CHECK(game.avatar->inventory == NULL);
    teardown_game(&game);
}

static void test_clue_moves_character_and_wins_on_three_matches(void) {
    Game game = {0};
    srand(5);
    CHECK(setup_game(&game));
    rig_answer(&game, true, false, ITEM_CARRIED);
    char line[] = "clue scarlet";
    run_command(&game, line);
    CHECK(game.characters[0]->room == game.avatar->room);
    CHECK_EQ_INT(game.clues_used, 1);
    CHECK_EQ_INT(game.state, GAME_WON);
    teardown_game(&game);
}

static void test_clue_loses_on_tenth_without_all_matches(void) {
    Game game = {0};
    srand(5);
    CHECK(setup_game(&game));
    rig_answer(&game, false, false, ITEM_ELSEWHERE);
    for (int i = 0; i < MAX_CLUES; i++) {
        char line[] = "clue mustard";
        CHECK_EQ_INT(game.state, GAME_PLAYING);
        run_command(&game, line);
    }
    CHECK_EQ_INT(game.clues_used, MAX_CLUES);
    CHECK_EQ_INT(game.state, GAME_LOST);
    teardown_game(&game);
}

static void test_clue_unknown_character_is_free(void) {
    Game game = {0};
    srand(5);
    CHECK(setup_game(&game));
    char typo[] = "clue plumb";
    run_command(&game, typo);
    char bare[] = "clue";
    run_command(&game, bare);
    CHECK_EQ_INT(game.clues_used, 0);
    CHECK_EQ_INT(game.state, GAME_PLAYING);
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
    test_go_follows_pointers_and_refuses_walls();
    test_take_and_drop_round_trip();
    test_take_and_drop_refuse_wrong_list();
    test_evaluate_clue_reports_each_match();
    test_evaluate_clue_sees_item_lying_in_the_room();
    test_clue_moves_character_and_wins_on_three_matches();
    test_clue_loses_on_tenth_without_all_matches();
    test_clue_unknown_character_is_free();
    CHECK_REPORT("test_adventure");
}
