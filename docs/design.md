# Clue text adventure: design

This document holds every decision a reviewer could question and the reason
behind it. The code carries only one-line pointers here. The spec is in
`docs/spec.md`; the house rules are in `docs/conventions.md`.

## 1. Module map

The spec fixes the file names, so the split follows the spec exactly:

| File | Owns | Exposes |
| --- | --- | --- |
| `src/items.c/.h` | the `Item` node and singly linked item lists | `create_item`, `add_item`, `drop_item`, `find_item`, `count_items`, `free_items` |
| `src/rooms.c/.h` | the `Room` struct, the four-way links, the 3x3 grid wiring, the shuffle, the `Direction` enum | `create_room`, `link_grid`, `shuffle_rooms`, `room_in_direction`, `parse_direction`, `direction_name`, `find_room`, `free_room` |
| `src/characters.c/.h` | the `Character` struct (also used for the avatar) | `create_character`, `move_character`, `find_character`, `free_character` |
| `src/adventure.c` | the catalogues, world setup, the command table, the read-parse-dispatch loop, `main` | nothing (every helper is `static`) |

Rule of dependency: `items` depends on nothing, `rooms` depends on `items`,
`characters` depends on `rooms`, `adventure` depends on all three. No cycles,
so each module can be unit-tested against the ones below it.

## 2. Data structures

### Item (`items.h`)

```c
typedef struct Item {
    const char *name;       /* single lowercase token, points at a static catalogue string */
    struct Item *next;
} Item;
```

The item node is itself the list node (intrusive list). The list head is an
`Item *` owned by whoever holds it: a `Room` or a `Character`.

Decision: names point at string literals in the catalogue instead of being
copied with `malloc`. Every item node is still allocated with `calloc` and
freed with `free`, which satisfies the code requirement, while the number of
allocations to audit by hand stays small (no sanitizer on this machine,
see `docs/conventions.md` section 2).

Ownership invariant: **every item node is in exactly one list at all times**
(a room's list or the avatar's inventory). `take` and `drop` move nodes with
`drop_item` + `add_item`, never allocate, never free. Therefore teardown frees
each node exactly once by freeing every room's list and the avatar's inventory.

`add_item` appends at the tail so listings keep insertion order and e2e
transcripts stay stable. `drop_item` unlinks by name with the pointer-to-pointer
idiom (no special case for the head) and returns the node so the caller can
re-add it elsewhere; it returns `NULL` when the name is not in the list.

### Room (`rooms.h`)

```c
typedef enum { DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_WEST,
               DIRECTION_COUNT, DIRECTION_NONE } Direction;

typedef struct Room {
    const char *name;
    const char *description;
    struct Room *exits[DIRECTION_COUNT];   /* NULL where the map ends */
    Item *items;
} Room;
```

Decision: exits are an array indexed by `Direction` rather than four named
fields, so `go DIRECTION` is one lookup (`room->exits[direction]`) with no
`switch`, and `look` prints the four exits in a loop. The spec's "via room
pointer" requirement is met literally: the avatar moves by following the
pointer stored in the current room.

`link_grid(Room *rooms[], size_t side)` wires an array laid out row-major
(`index = row * side + column`) so that rooms on the edge get `NULL` off the
map. Corners end up with 2 exits, edges with 3, the centre with 4; the unit
tests assert exactly that.

`shuffle_rooms(Room *rooms[], size_t count, size_t (*pick)(size_t bound))`
is Fisher–Yates over the pointer array. The random source is injected as a
function pointer so the module does not call `rand()` itself: the unit test
injects a deterministic `pick` and asserts the exact permutation, and the
program injects `random_below` from `adventure.c`, the single place that
touches `rand()`.

### Character (`characters.h`)

```c
typedef struct Character {
    const char *name;
    Room *room;         /* where the character stands; never NULL after setup */
    Item *inventory;    /* NULL for non-player characters */
} Character;
```

Decision: the avatar is a `Character` too. The spec wants an avatar with an
inventory and five other characters that can be moved by `clue`; one struct
with an `inventory` field covers both, and `move_character` serves both the
`go` command (moving the avatar) and the `clue` command (summoning a
character). Characters do not know which room lists them; `look` finds the
characters in a room by comparing `character->room` with the current room
over the five-element array, which is cheaper than maintaining a per-room
character list for a fixed, tiny population.

### Game (`adventure.c`, private)

```c
typedef struct Game {
    Room *rooms[ROOM_COUNT];
    Character *characters[CHARACTER_COUNT];
    Character *avatar;
    const Room *answer_room;
    const char *answer_item;        /* name; the node moves between lists */
    const Character *answer_character;
    int clues_used;
    GameState state;                /* PLAYING, WON, LOST, QUIT */
} Game;
```

Lives on `main`'s stack; passed by pointer to every handler. There is no
global mutable state. The answer item is kept by name because the item node
travels between lists during play and the match test is "is an item of that
name in the room list or the inventory list".

### Command table (`adventure.c`)

```c
typedef void (*CommandHandler)(Game *game, const char *argument);
typedef struct Command {
    const char *name;
    const char *usage;
    CommandHandler handler;
} Command;
static const Command COMMANDS[] = { {"help", "help", handle_help}, ... };
```

`help` prints the table it is part of, so the handlers are prototyped above
the table. Lookup is a linear scan over nine entries. Commands are matched on
the already-lowercased verb.

## 3. Catalogues

All names are single lowercase tokens so that `take knife`, `go north` and
`clue plum` split on the first space with nothing else to parse.

- Rooms (9): kitchen, ballroom, conservatory, dining, billiard, library,
  lounge, hall, study.
- Characters (5): scarlet, mustard, white, green, peacock.
- Items (6): candlestick, knife, pipe, revolver, rope, wrench.

The catalogues are `static const` arrays in `adventure.c`; `list` prints
them, and setup instantiates from them.

## 4. World setup and the random draw order

`seed_random` reads `CLUE_SEED`; when it is set and parses as an unsigned
integer the program calls `srand` with it, otherwise it seeds from
`time(NULL)`. The draw order below is part of the contract with the e2e
tests: changing it changes every expected transcript.

1. Create the 9 rooms in catalogue order, then `shuffle_rooms` (Fisher–Yates,
   8 draws). The shuffled array is the row-major 3x3 board; `link_grid` wires it.
2. Create the 5 characters; each is placed with `move_character` into
   `rooms[random_below(9)]` (5 draws). Sharing a room is allowed.
3. Copy the room pointer array, `shuffle_rooms` it again (8 draws) and give
   item `k` to the room at position `k` of the copy. Six distinct rooms get
   one item each; three rooms start empty. This is the "at most one item per
   room" requirement.
4. Create the avatar and place it in `rooms[random_below(9)]` (1 draw).
5. Pick the answer: `rooms[random_below(9)]`, item name
   `ITEM_NAMES[random_below(6)]`, `characters[random_below(5)]` (3 draws).

`random_below(bound)` is `(size_t)rand() % bound`. The modulo bias for bounds
up to 9 against `RAND_MAX` of 2^31-1 is below 1e-8 and irrelevant for a game.

Expected e2e transcripts depend on glibc's `rand()` sequence for a given seed.
The course VM and this machine both run glibc; `docs/research.md` records the
standard's lack of a guarantee.

## 5. The command loop

```
print banner and starting room
while state == PLAYING:
    prompt "> "
    read_line -> on EOF: print "Goodbye." and leave the loop (state = QUIT)
    lowercase, split into verb and argument (first whitespace run)
    empty verb -> continue
    find command -> not found: "Unknown command 'x'. Type 'help' for the list of commands."
    handler(game, argument)
teardown, return EXIT_SUCCESS
```

`read_line` uses `fgets` into a 256-byte buffer. When the line does not end
in a newline the rest of the line is drained with `getchar` so an overlong
line counts as one command. The verb/argument split walks the buffer in place
without `strtok`. Lowercasing is applied to the whole line, so `GO North`
works and item/character names match the catalogue.

Commands that take no argument ignore any argument. Commands that need one
print their usage line when it is missing.

## 6. Command semantics and messages

| Command | Behaviour |
| --- | --- |
| `help` | one line per command: `  <usage padded>  <description>` |
| `list` | three lines: `Rooms: ...`, `Characters: ...`, `Items: ...` from the catalogues |
| `look` | room name, description, four `North:/South:/East:/West:` lines (`nothing` where there is no room), `Characters here:` and `Items here:` (`none` when empty) |
| `go DIR` | `go` alone -> usage; unknown word -> `There is no direction 'x'. Use north, south, east or west.`; wall -> `You cannot go <dir> from here.`; otherwise move and `look` |
| `take ITEM` | item in room -> moved to inventory, `You take the knife.`; else `There is no 'knife' here.` |
| `drop ITEM` | item in inventory -> moved to room, `You drop the knife.`; else `You are not carrying 'knife'.` |
| `inventory` | `You are carrying: knife, rope` or `You are carrying: nothing` |
| `clue CHAR` | see section 7 |
| `quit` | `Goodbye.`, state = QUIT |

## 7. The clue command

```
handle_clue(game, name):
    empty name            -> usage, no clue consumed
    unknown character     -> "There is no character named 'x'.", no clue consumed
    move_character(character, avatar->room)
    clues_used++
    room_match      = avatar->room == answer_room
    character_match = answer_character->room == avatar->room
    item_match      = find_item(avatar->room->items, answer_item)
                   || find_item(avatar->inventory, answer_item)
    print "Room Match" / "Character Match" / "Item Match" for each true flag
    (print "No matches." when none, so the player gets a line back)
    all three        -> "You solved the mystery: <char> in the <room> with the <item>. You win!", state = WON
    clues_used == 10 -> "That was your 10th clue. The answer was <...>. You lose.", state = LOST
    else             -> "Clues used: n of 10."
```

Only a valid clue (a real character name) consumes one of the ten; an
invalid name is a typo, not a guess. `Character Match` is true whenever the
named character is the answer character (it was just moved into the room) or
the answer character already stood there; this is exactly what the spec's
"if the character of the answer is in the room" says.

## 8. Memory discipline

No sanitizer or valgrind exists on this machine, so the allocations are kept
few and their lifetimes obvious:

| Allocation | Made in | Freed in |
| --- | --- | --- |
| 9 `Room` | `create_room` (setup) | `free_room` via `teardown_game` |
| 6 `Item` | `create_item` (setup) | `free_items` on each room list and the avatar inventory via `free_room` / `free_character` |
| 5 + 1 `Character` | `create_character` (setup) | `free_character` via `teardown_game` |

`teardown_game` runs on every way out of the loop: win, lose, `quit`, and EOF
on stdin. If any `calloc` fails during setup, `setup_game` tears down what it
has already built and `main` exits with `EXIT_FAILURE`. `free_character` frees
the inventory list; `free_room` frees the item list; both tolerate `NULL`.
`make check` (`gcc -fanalyzer`) must be clean, and the review step audits
every path by hand.

## 9. Testing strategy

- `test/unit/test_items.c`, `test_rooms.c`, `test_characters.c` link against
  the module objects and cover every exported function including empty-list
  and missing-name cases, grid link counts, reachability, deterministic
  shuffle permutations, and character moves.
- `test/unit/test_adventure.c` uses the include trick (`#define main
  adventure_main` then `#include "../../src/adventure.c"`) to reach the
  static parser, command lookup, setup invariants and clue evaluation.
- `test/e2e/run_e2e.sh` runs every `test/e2e/cases/<name>.in` through the
  binary under `timeout` with `CLUE_SEED` from `<name>.seed` (default 1) and
  diffs stdout against `<name>.expected`. The cases listed in the plan cover
  help, list, look in a corner room, go into a wall, take/drop round trip,
  inventory, a winning game, a losing game after 10 clues, unknown command,
  and EOF mid-game.

## 10. Deviations from the house conventions

None. The spec's file list leaves no room for a separate parser or random
module, so the parser lives in `adventure.c` as static functions and is
tested through the include trick, and the random source is injected into
`rooms.c` as a function pointer instead of creating a `random.c`.
