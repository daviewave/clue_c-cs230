#include <ctype.h>
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
    Room *rooms[ROOM_COUNT];                /* row-major 3x3 board after the shuffle */
    Character *characters[CHARACTER_COUNT]; /* the suspects; the avatar is separate */
    Character *avatar;
    const Room *answer_room;
    const char *answer_item;                /* a name: the node moves between lists */
    const Character *answer_character;
    int clues_used;                         /* valid clue commands so far, max MAX_CLUES */
    GameState state;                        /* play() loops while GAME_PLAYING */
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

typedef void (*CommandHandler)(Game *game, const char *argument);

/* One row of the command table that help prints. */
typedef struct Command {
    const char *name;
    const char *usage;
    const char *description;
    CommandHandler handler;
} Command;

static void handle_help(Game *game, const char *argument);
static void handle_list(Game *game, const char *argument);
static void handle_look(Game *game, const char *argument);
static void handle_go(Game *game, const char *argument);
static void handle_take(Game *game, const char *argument);
static void handle_drop(Game *game, const char *argument);
static void handle_inventory(Game *game, const char *argument);
static void handle_clue(Game *game, const char *argument);
static void handle_quit(Game *game, const char *argument);

static const Command COMMANDS[] = {
    {"help", "help", "show this table of commands", handle_help},
    {"list", "list", "list every room, character and item", handle_list},
    {"look", "look", "describe the room you are in", handle_look},
    {"go", "go DIRECTION", "walk north, south, east or west", handle_go},
    {"take", "take ITEM", "pick up an item lying in the room", handle_take},
    {"drop", "drop ITEM", "put down an item you are carrying", handle_drop},
    {"inventory", "inventory", "show what you are carrying", handle_inventory},
    {"clue", "clue CHARACTER", "summon a character and test your theory", handle_clue},
    {"quit", "quit", "leave the game", handle_quit},
};

enum { COMMAND_COUNT = sizeof COMMANDS / sizeof COMMANDS[0] };

/* Reads the rest of the current line so an overlong line counts as one command. */
static void drain_line(FILE *in) {
    int c;
    do {
        c = getc(in);
    } while (c != '\n' && c != EOF);
}

/* Reads one line into buffer without its newline.
 * @return false at end of input. */
static bool read_line(char *buffer, size_t capacity, FILE *in) {
    if (fgets(buffer, (int)capacity, in) == NULL) {
        return false;
    }
    size_t length = strcspn(buffer, "\n");
    if (buffer[length] == '\n') {
        buffer[length] = '\0';
    } else {
        drain_line(in);
    }
    return true;
}

/* Lowercases text in place so commands and names are case-insensitive. */
static void lowercase(char *text) {
    for (; *text != '\0'; text++) {
        *text = (char)tolower((unsigned char)*text);
    }
}

/* @return the first non-blank character at or after text. */
static char *skip_blanks(char *text) {
    while (isspace((unsigned char)*text)) {
        text++;
    }
    return text;
}

/* Cuts trailing blanks off text. */
static void trim_trailing_blanks(char *text) {
    size_t length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1])) {
        text[--length] = '\0';
    }
}

/* Splits line in place into the verb and the rest; both point into line and
 * the argument is "" when there is none. */
static void split_command(char *line, const char **verb, const char **argument) {
    char *cursor = skip_blanks(line);
    *verb = cursor;
    while (*cursor != '\0' && !isspace((unsigned char)*cursor)) {
        cursor++;
    }
    if (*cursor != '\0') {
        *cursor = '\0';
        cursor = skip_blanks(cursor + 1);
    }
    trim_trailing_blanks(cursor);
    *argument = cursor;
}

/* @return the table row for verb, or NULL. */
static const Command *find_command(const char *verb) {
    for (size_t i = 0; i < COMMAND_COUNT; i++) {
        if (strcmp(COMMANDS[i].name, verb) == 0) {
            return &COMMANDS[i];
        }
    }
    return NULL;
}

/* Prints the usage line of a command from the table. */
static void print_usage(const char *command_name) {
    const Command *command = find_command(command_name);
    if (command != NULL) {
        printf("Usage: %s\n", command->usage);
    }
}

/* Prints one line per command; the table is the help text. */
static void handle_help(Game *game, const char *argument) {
    (void)game;
    (void)argument;
    puts("Commands:");
    for (size_t i = 0; i < COMMAND_COUNT; i++) {
        printf("  %-16s %s\n", COMMANDS[i].usage, COMMANDS[i].description);
    }
}

/* Prints a label followed by the comma-separated names. */
static void print_names(const char *label, const char *const names[], size_t count) {
    printf("%s:", label);
    for (size_t i = 0; i < count; i++) {
        printf("%s%s", i == 0 ? " " : ", ", names[i]);
    }
    putchar('\n');
}

/* Lists the catalogues of rooms, characters and items. */
static void handle_list(Game *game, const char *argument) {
    (void)game;
    (void)argument;
    print_names("Rooms", ROOM_NAMES, ROOM_COUNT);
    print_names("Characters", CHARACTER_NAMES, CHARACTER_COUNT);
    print_names("Items", ITEM_NAMES, ITEM_COUNT);
}

/* Prints the room behind each of the four exits, or "nothing" for a wall. */
static void print_exits(const Room *room) {
    for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
        const Room *next = room_in_direction(room, (Direction)direction);
        printf("  %-6s %s\n", direction_name((Direction)direction),
               next != NULL ? next->name : "nothing");
    }
}

/* Prints the suspects standing in the avatar's room. */
static void print_characters_here(const Game *game) {
    bool any = false;
    fputs("Characters here:", stdout);
    for (size_t i = 0; i < CHARACTER_COUNT; i++) {
        if (game->characters[i]->room == game->avatar->room) {
            printf("%s%s", any ? ", " : " ", game->characters[i]->name);
            any = true;
        }
    }
    puts(any ? "" : " none");
}

/* Prints label and the names in items, or empty_text when there are none. */
static void print_items(const char *label, const Item *items, const char *empty_text) {
    fputs(label, stdout);
    if (items == NULL) {
        printf(" %s\n", empty_text);
        return;
    }
    for (const Item *item = items; item != NULL; item = item->next) {
        printf("%s%s", item == items ? " " : ", ", item->name);
    }
    putchar('\n');
}

/* Describes the current room: exits, characters and items. */
static void handle_look(Game *game, const char *argument) {
    (void)argument;
    const Room *room = game->avatar->room;
    printf("== %s ==\n%s\n", room->name, room->description);
    print_exits(room);
    print_characters_here(game);
    print_items("Items here:", room->items, "none");
}

/* Moves the avatar through the room pointer in the given direction. */
static void handle_go(Game *game, const char *argument) {
    if (*argument == '\0') {
        print_usage("go");
        return;
    }
    Direction direction = parse_direction(argument);
    if (direction == DIRECTION_NONE) {
        printf("There is no direction '%s'. Use north, south, east or west.\n", argument);
        return;
    }
    Room *destination = room_in_direction(game->avatar->room, direction);
    if (destination == NULL) {
        printf("You cannot go %s from here.\n", direction_name(direction));
        return;
    }
    move_character(game->avatar, destination);
    handle_look(game, "");
}

/* Moves an item from the room's list to the inventory. */
static void handle_take(Game *game, const char *argument) {
    if (*argument == '\0') {
        print_usage("take");
        return;
    }
    Item *item = drop_item(&game->avatar->room->items, argument);
    if (item == NULL) {
        printf("There is no '%s' here.\n", argument);
        return;
    }
    add_item(&game->avatar->inventory, item);
    printf("You take the %s.\n", item->name);
}

/* Moves an item from the inventory to the room's list. */
static void handle_drop(Game *game, const char *argument) {
    if (*argument == '\0') {
        print_usage("drop");
        return;
    }
    Item *item = drop_item(&game->avatar->inventory, argument);
    if (item == NULL) {
        printf("You are not carrying '%s'.\n", argument);
        return;
    }
    add_item(&game->avatar->room->items, item);
    printf("You drop the %s.\n", item->name);
}

/* Lists what the avatar carries. */
static void handle_inventory(Game *game, const char *argument) {
    (void)argument;
    print_items("You are carrying:", game->avatar->inventory, "nothing");
}

/* Which of the three parts of the answer the avatar's room currently satisfies. */
typedef struct ClueResult {
    bool room;
    bool character;
    bool item;
} ClueResult;

/* Compares the avatar's surroundings with the answer; the inventory counts as the room. */
static ClueResult evaluate_clue(const Game *game) {
    const Room *here = game->avatar->room;
    ClueResult result;
    result.room = here == game->answer_room;
    result.character = game->answer_character->room == here;
    result.item = find_item(here->items, game->answer_item) != NULL ||
                  find_item(game->avatar->inventory, game->answer_item) != NULL;
    return result;
}

/* Prints the spec's match lines, or a line saying nothing matched. */
static void print_matches(ClueResult result) {
    if (result.room) {
        puts("Room Match");
    }
    if (result.character) {
        puts("Character Match");
    }
    if (result.item) {
        puts("Item Match");
    }
    if (!result.room && !result.character && !result.item) {
        puts("No matches.");
    }
}

/* Decides win, loss or carry on after a clue has been counted. */
static void resolve_clue(Game *game, ClueResult result) {
    if (result.room && result.character && result.item) {
        printf("You solved the mystery: it was %s in the %s with the %s. You win!\n",
               game->answer_character->name, game->answer_room->name, game->answer_item);
        game->state = GAME_WON;
    } else if (game->clues_used >= MAX_CLUES) {
        printf("You have used all %d clues. It was %s in the %s with the %s. You lose.\n",
               MAX_CLUES, game->answer_character->name, game->answer_room->name,
               game->answer_item);
        game->state = GAME_LOST;
    } else {
        printf("Clues used: %d of %d.\n", game->clues_used, MAX_CLUES);
    }
}

/* Summons a character, counts the clue and reports the matches (docs/design.md section 7). */
static void handle_clue(Game *game, const char *argument) {
    if (*argument == '\0') {
        print_usage("clue");
        return;
    }
    Character *character = find_character(game->characters, CHARACTER_COUNT, argument);
    if (character == NULL) {
        printf("There is no character named '%s'.\n", argument);
        return;
    }
    move_character(character, game->avatar->room);
    game->clues_used++;
    printf("%s arrives in the %s.\n", character->name, game->avatar->room->name);
    ClueResult result = evaluate_clue(game);
    print_matches(result);
    resolve_clue(game, result);
}

/* Ends the game at the player's request. */
static void handle_quit(Game *game, const char *argument) {
    (void)argument;
    puts("Goodbye.");
    game->state = GAME_QUIT;
}

/* Lowercases, splits and dispatches one line of input. */
static void run_command(Game *game, char *line) {
    const char *verb = NULL;
    const char *argument = NULL;
    lowercase(line);
    split_command(line, &verb, &argument);
    if (*verb == '\0') {
        return;
    }
    const Command *command = find_command(verb);
    if (command == NULL) {
        printf("Unknown command '%s'. Type 'help' for the list of commands.\n", verb);
        return;
    }
    command->handler(game, argument);
}

/* Prompts and runs commands until the game is won, lost, quit or input ends. */
static void play(Game *game) {
    char line[LINE_CAPACITY];
    while (game->state == GAME_PLAYING) {
        fputs("> ", stdout);
        fflush(stdout);
        if (!read_line(line, sizeof line, stdin)) {
            puts("\nGoodbye.");
            game->state = GAME_QUIT;
            return;
        }
        run_command(game, line);
    }
}

/* Prints the premise once at start. */
static void print_banner(void) {
    puts("Welcome to Clue.");
    puts("Someone was murdered in this mansion. Find out who did it, where, and with what.");
    puts("Type 'help' for the list of commands.");
}

/* Seeds, builds the world, plays until the game ends and frees everything. */
int main(void) {
    Game game = {0};
    seed_random();
    if (!setup_game(&game)) {
        fputs("adventure: out of memory\n", stderr);
        teardown_game(&game);
        return EXIT_FAILURE;
    }
    print_banner();
    handle_look(&game, "");
    play(&game);
    teardown_game(&game);
    return EXIT_SUCCESS;
}
