#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "characters.h"
#include "items.h"
#include "rooms.h"

enum {
    GRID_SIDE = 3,
    ROOM_COUNT = GRID_SIDE * GRID_SIDE,
    CHARACTER_COUNT = 5,
    ITEM_COUNT = 6,
    MAX_CLUES = 10,
    LINE_CAPACITY = 256
};

typedef enum { GAME_PLAYING, GAME_WON, GAME_LOST, GAME_QUIT } GameState;

/* The whole world; lives on main's stack and is passed to every handler. */
typedef struct Game {
    Room *rooms[ROOM_COUNT];
    Character *characters[CHARACTER_COUNT];
    Character *avatar;
    const Room *answer_room;
    const char *answer_item;
    const Character *answer_character;
    int clues_used;
    GameState state;
} Game;

static const char *const ROOM_NAMES[ROOM_COUNT] = {
    "kitchen", "ballroom", "conservatory", "dining", "billiard",
    "library", "lounge", "hall", "study"
};

static const char *const ROOM_DESCRIPTIONS[ROOM_COUNT] = {
    "Copper pans hang over a cold stove. Something was chopped here recently.",
    "A chandelier lights a floor polished for dancing nobody came to do.",
    "Glass walls, damp air and ferns that hide more than they should.",
    "A long table set for twelve, one chair pushed back in a hurry.",
    "A green felt table under a low lamp; the cue rack is missing a cue.",
    "Floor-to-ceiling shelves and a ladder on rails. One book lies open.",
    "Deep armchairs around a dead fire; the ashtray is still warm.",
    "A grand staircase, a grandfather clock and muddy footprints.",
    "A desk buried in letters, a safe that is not quite shut."
};

static const char *const CHARACTER_NAMES[CHARACTER_COUNT] = {
    "scarlet", "mustard", "white", "green", "peacock"
};

static const char *const ITEM_NAMES[ITEM_COUNT] = {
    "candlestick", "knife", "pipe", "revolver", "rope", "wrench"
};

/* Seeds rand() from CLUE_SEED when it holds a number, else from the clock
 * (docs/design.md section 4). */
static void seed_random(void) {
    const char *seed_text = getenv("CLUE_SEED");
    unsigned seed = (unsigned)time(NULL);
    if (seed_text != NULL && *seed_text != '\0') {
        char *end = NULL;
        unsigned long parsed = strtoul(seed_text, &end, 10);
        if (*end == '\0') {
            seed = (unsigned)parsed;
        }
    }
    srand(seed);
}

/* @return a pseudo-random index in [0, bound); the only caller of rand() in the game. */
static size_t random_below(size_t bound) {
    return (size_t)rand() % bound;
}

/* Creates the nine rooms, shuffles their positions and wires the 3x3 grid. */
static bool create_rooms(Game *game) {
    for (size_t i = 0; i < ROOM_COUNT; i++) {
        game->rooms[i] = create_room(ROOM_NAMES[i], ROOM_DESCRIPTIONS[i]);
        if (game->rooms[i] == NULL) {
            return false;
        }
    }
    shuffle_rooms(game->rooms, ROOM_COUNT, random_below);
    link_grid(game->rooms, GRID_SIDE);
    return true;
}

/* Creates the five suspects, each in a random room (rooms may be shared). */
static bool create_characters(Game *game) {
    for (size_t i = 0; i < CHARACTER_COUNT; i++) {
        game->characters[i] = create_character(CHARACTER_NAMES[i]);
        if (game->characters[i] == NULL) {
            return false;
        }
        move_character(game->characters[i], game->rooms[random_below(ROOM_COUNT)]);
    }
    return true;
}

/* Puts each item in a different random room, so no room starts with two. */
static bool place_items(Game *game) {
    Room *order[ROOM_COUNT];
    memcpy(order, game->rooms, sizeof order);
    shuffle_rooms(order, ROOM_COUNT, random_below);
    for (size_t i = 0; i < ITEM_COUNT; i++) {
        Item *item = create_item(ITEM_NAMES[i]);
        if (item == NULL) {
            return false;
        }
        add_item(&order[i]->items, item);
    }
    return true;
}

/* Creates the player's avatar in a random starting room. */
static bool create_avatar(Game *game) {
    game->avatar = create_character("you");
    if (game->avatar == NULL) {
        return false;
    }
    move_character(game->avatar, game->rooms[random_below(ROOM_COUNT)]);
    return true;
}

/* Draws the secret room, item and character. */
static void pick_answer(Game *game) {
    game->answer_room = game->rooms[random_below(ROOM_COUNT)];
    game->answer_item = ITEM_NAMES[random_below(ITEM_COUNT)];
    game->answer_character = game->characters[random_below(CHARACTER_COUNT)];
}

/* Builds the world into a zeroed Game in the draw order of docs/design.md section 4.
 * @return false when an allocation failed; teardown_game is still safe to call. */
static bool setup_game(Game *game) {
    game->state = GAME_PLAYING;
    if (!create_rooms(game) || !create_characters(game) || !place_items(game) ||
        !create_avatar(game)) {
        return false;
    }
    pick_answer(game);
    return true;
}

/* Frees every allocation the game owns, including items wherever they lie. */
static void teardown_game(Game *game) {
    for (size_t i = 0; i < ROOM_COUNT; i++) {
        free_room(game->rooms[i]);
        game->rooms[i] = NULL;
    }
    for (size_t i = 0; i < CHARACTER_COUNT; i++) {
        free_character(game->characters[i]);
        game->characters[i] = NULL;
    }
    free_character(game->avatar);
    game->avatar = NULL;
}

/* Builds the world and tears it down; the command loop arrives in later tasks. */
int main(void) {
    Game game = {0};
    seed_random();
    if (!setup_game(&game)) {
        fputs("adventure: out of memory\n", stderr);
        teardown_game(&game);
        return EXIT_FAILURE;
    }
    teardown_game(&game);
    return EXIT_SUCCESS;
}
