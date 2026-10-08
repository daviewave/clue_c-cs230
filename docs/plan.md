# Clue Text Adventure Implementation Plan

> **For implementers:** work through this plan one task at a time, test first. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A C99 Clue text adventure: nine shuffled rooms on a 3x3 grid, five characters, six items, an avatar with an inventory, a command table, and a `clue` command that wins on three matches or loses on the tenth clue.

**Architecture:** Three data modules (`items`, `rooms`, `characters`) with intrusive singly linked item lists and pointer-linked rooms, and one `adventure.c` holding the catalogues, world setup, the command table and the read-parse-dispatch loop. Randomness is seeded from `CLUE_SEED` so transcripts repeat.

**Tech Stack:** C99, gcc 16, GNU make, bash for the test runners. No external libraries.

**Spec:** `docs/spec.md` (course spec), `docs/design.md` (data structures, draw order, message wording), `docs/conventions.md` (house rules).

## Global Constraints

- Compile flag floor: `gcc -std=c99` (spec); development set `-Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wconversion -Wvla -Werror` (conventions section 2).
- Deliverable sources: `src/adventure.c`, `src/rooms.c`, `src/rooms.h`, `src/items.c`, `src/items.h`, `src/characters.c`, `src/characters.h`; `items.c` exports `add_item` and `drop_item`; `characters.c` exports `move_character`.
- Structs, pointers, `malloc`/`calloc` and `free` must all appear; every allocation is freed on every exit path (win, lose, quit, EOF).
- Exactly 9 rooms, 5 non-player characters, 6 items, at most one item per room at start, one random room + item + character as the answer.
- Randomness seeded from `CLUE_SEED` when set, else from time.
- Every function in `src/` has a header comment; no narration inside bodies.
- Commits: conventional-commit subjects, no trailers, never push.

## Review Focus

1. EOF on stdin in the middle of a game (piped input that ends without `quit`) must print `Goodbye.`, free everything and exit 0. Pinned in Task 7 (unit: `read_line` returns false at EOF) and Task 8 (e2e case `eof`).
2. An overlong input line (longer than 255 bytes) must be treated as one command, not spill its tail into the next prompt. Pinned in Task 7 (`test_read_line_drains_overlong_line`).
3. `clue` with a misspelt character must not consume one of the ten clues; otherwise a typo could lose the game. Pinned in Task 7 (`test_clue_unknown_character_is_free`).
4. `take` of an item that is already in the inventory, and `drop` of an item that is in the room but not carried, must both be refused with a message and leave both lists unchanged. Pinned in Task 7 (`test_take_and_drop_refuse_wrong_list`).
5. After the shuffle every room must still be reachable from every other (grid linking must not depend on catalogue order). Pinned in Task 2 (`test_every_room_reachable`).

---

### Task 1: Item lists (`items`)

**Files:**
- Create: `src/items.h`, `src/items.c`
- Test: `test/unit/test_items.c`

**Interfaces:**
- Produces:
  - `typedef struct Item { const char *name; struct Item *next; } Item;`
  - `Item *create_item(const char *name);` NULL on allocation failure
  - `void add_item(Item **list, Item *item);` appends at the tail
  - `Item *drop_item(Item **list, const char *name);` unlinks and returns the node, NULL when absent
  - `Item *find_item(Item *list, const char *name);`
  - `void free_items(Item *list);` frees every node, tolerates NULL

- [x] **Step 1: Write the failing test**

```c
/* test/unit/test_items.c */
#include "../../src/items.h"
#include "check.h"
#include <stdlib.h>

static void test_add_appends_in_order(void) {
    Item *list = NULL;
    add_item(&list, create_item("knife"));
    add_item(&list, create_item("rope"));
    CHECK_EQ_INT(count_items(list), 2);
    CHECK_EQ_STR(list->name, "knife");
    CHECK_EQ_STR(list->next->name, "rope");
    CHECK(list->next->next == NULL);
    free_items(list);
}

static void test_find_present_and_missing(void) {
    Item *list = NULL;
    add_item(&list, create_item("knife"));
    CHECK(find_item(list, "knife") == list);
    CHECK(find_item(list, "rope") == NULL);
    CHECK(find_item(NULL, "knife") == NULL);
    free_items(list);
}

static void test_drop_head_middle_tail_and_missing(void) {
    Item *list = NULL;
    add_item(&list, create_item("a"));
    add_item(&list, create_item("b"));
    add_item(&list, create_item("c"));
    Item *b = drop_item(&list, "b");
    CHECK(b != NULL && b->next == NULL);
    CHECK_EQ_INT(count_items(list), 2);
    Item *a = drop_item(&list, "a");
    CHECK(a != NULL);
    CHECK_EQ_STR(list->name, "c");
    Item *c = drop_item(&list, "c");
    CHECK(c != NULL);
    CHECK(list == NULL);
    CHECK(drop_item(&list, "zzz") == NULL);
    free(a);
    free(b);
    free(c);
}

static void test_move_between_lists_keeps_one_owner(void) {
    Item *room = NULL;
    Item *inventory = NULL;
    add_item(&room, create_item("pipe"));
    add_item(&inventory, drop_item(&room, "pipe"));
    CHECK_EQ_INT(count_items(room), 0);
    CHECK_EQ_INT(count_items(inventory), 1);
    free_items(room);
    free_items(inventory);
}

static void test_count_and_free_empty(void) {
    CHECK_EQ_INT(count_items(NULL), 0);
    free_items(NULL);
}

int main(void) {
    test_add_appends_in_order();
    test_find_present_and_missing();
    test_drop_head_middle_tail_and_missing();
    test_move_between_lists_keeps_one_owner();
    test_count_and_free_empty();
    CHECK_REPORT("test_items");
}
```

- [x] **Step 2: Run test to verify it fails**

Run: `make build/test/test_items`
Expected: compile error, `items.h` not found.

- [x] **Step 3: Write minimal implementation**

```c
/* src/items.h */
#ifndef ITEMS_H
#define ITEMS_H

#include <stddef.h>

/* One item; the node is also the list link, so an item is in exactly one list at a time. */
typedef struct Item {
    const char *name;
    struct Item *next;
} Item;

Item *create_item(const char *name);
void add_item(Item **list, Item *item);
Item *drop_item(Item **list, const char *name);
Item *find_item(Item *list, const char *name);
size_t count_items(const Item *list);
void free_items(Item *list);

#endif
```

```c
/* src/items.c */
#include "items.h"

#include <stdlib.h>
#include <string.h>

/* Allocates an item that belongs to no list yet.
 * @return the new node, or NULL when memory is exhausted. */
Item *create_item(const char *name) {
    Item *item = calloc(1, sizeof *item);
    if (item != NULL) {
        item->name = name;
    }
    return item;
}

/* Appends item at the tail of *list so listings keep insertion order. */
void add_item(Item **list, Item *item) {
    Item **tail = list;
    while (*tail != NULL) {
        tail = &(*tail)->next;
    }
    item->next = NULL;
    *tail = item;
}

/* Unlinks the first item called name from *list.
 * @return the detached node (next == NULL), or NULL when no item has that name. */
Item *drop_item(Item **list, const char *name) {
    Item **link = list;
    while (*link != NULL && strcmp((*link)->name, name) != 0) {
        link = &(*link)->next;
    }
    Item *found = *link;
    if (found != NULL) {
        *link = found->next;
        found->next = NULL;
    }
    return found;
}

/* @return the first item called name, or NULL. */
Item *find_item(Item *list, const char *name) {
    for (Item *item = list; item != NULL; item = item->next) {
        if (strcmp(item->name, name) == 0) {
            return item;
        }
    }
    return NULL;
}

/* @return the number of nodes in list. */
size_t count_items(const Item *list) {
    size_t count = 0;
    for (const Item *item = list; item != NULL; item = item->next) {
        count++;
    }
    return count;
}

/* Frees every node of list; the head pointer is dangling afterwards. */
void free_items(Item *list) {
    while (list != NULL) {
        Item *next = list->next;
        free(list);
        list = next;
    }
}
```

- [x] **Step 4: Run test to verify it passes**

Run: `make build/test/test_items && build/test/test_items`
Expected: `test_items: 20 checks, 0 failures`, exit 0.

- [x] **Step 5: Commit**

```bash
git add src/items.h src/items.c test/unit/test_items.c
git commit -m "feat: add item linked list with add_item and drop_item"
```

---

### Task 2: Rooms, directions, grid linking and shuffle (`rooms`)

**Files:**
- Create: `src/rooms.h`, `src/rooms.c`
- Test: `test/unit/test_rooms.c`

**Interfaces:**
- Consumes: `Item`, `free_items` from Task 1.
- Produces:
  - `typedef enum { DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_WEST, DIRECTION_COUNT, DIRECTION_NONE } Direction;`
  - `typedef struct Room { const char *name; const char *description; struct Room *exits[DIRECTION_COUNT]; Item *items; } Room;`
  - `typedef size_t (*RandomPicker)(size_t bound);` returns a value in `[0, bound)`
  - `Room *create_room(const char *name, const char *description);`
  - `void link_grid(Room *rooms[], size_t side);` row-major `side x side`
  - `void shuffle_rooms(Room *rooms[], size_t count, RandomPicker pick);`
  - `Room *room_in_direction(const Room *room, Direction direction);`
  - `Direction parse_direction(const char *word);` `DIRECTION_NONE` when unknown
  - `const char *direction_name(Direction direction);`
  - `Room *find_room(Room *const rooms[], size_t count, const char *name);`
  - `void free_room(Room *room);` frees the item list too, tolerates NULL

- [x] **Step 1: Write the failing test**

```c
/* test/unit/test_rooms.c */
#include "../../src/rooms.h"
#include "check.h"
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
    destroy(rooms);
}

static void visit(const Room *room, const Room *const rooms[], bool seen[]) {
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
    visit(rooms[4], (const Room *const *)rooms, seen);
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
```

- [x] **Step 2: Run test to verify it fails**

Run: `make build/test/test_rooms`
Expected: compile error, `rooms.h` not found.

- [x] **Step 3: Write minimal implementation**

```c
/* src/rooms.h */
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
```

```c
/* src/rooms.c */
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

/* @return the room called name among rooms, or NULL. */
Room *find_room(Room *const rooms[], size_t count, const char *name) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(rooms[i]->name, name) == 0) {
            return rooms[i];
        }
    }
    return NULL;
}

/* Frees the room and every item still lying in it. */
void free_room(Room *room) {
    if (room != NULL) {
        free_items(room->items);
        free(room);
    }
}
```

- [x] **Step 4: Run test to verify it passes**

Run: `make build/test/test_rooms && build/test/test_rooms`
Expected: `test_rooms: 36 checks, 0 failures`.

- [x] **Step 5: Commit**

```bash
git add src/rooms.h src/rooms.c test/unit/test_rooms.c
git commit -m "feat: add rooms with grid linking, directions and shuffle"
```

---

### Task 3: Characters and the avatar (`characters`)

**Files:**
- Create: `src/characters.h`, `src/characters.c`
- Test: `test/unit/test_characters.c`

**Interfaces:**
- Consumes: `Room`, `Item`, `free_items`.
- Produces:
  - `typedef struct Character { const char *name; Room *room; Item *inventory; } Character;`
  - `Character *create_character(const char *name);`
  - `void move_character(Character *character, Room *room);`
  - `Character *find_character(Character *const characters[], size_t count, const char *name);`
  - `void free_character(Character *character);` frees the inventory too, tolerates NULL

- [x] **Step 1: Write the failing test**

```c
/* test/unit/test_characters.c */
#include "../../src/characters.h"
#include "check.h"
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
```

- [x] **Step 2: Run test to verify it fails**

Run: `make build/test/test_characters`
Expected: compile error, `characters.h` not found.

- [x] **Step 3: Write minimal implementation**

```c
/* src/characters.h */
#ifndef CHARACTERS_H
#define CHARACTERS_H

#include <stddef.h>

#include "items.h"
#include "rooms.h"

/* A suspect or the player's avatar; only the avatar carries an inventory. */
typedef struct Character {
    const char *name;
    Room *room;
    Item *inventory;
} Character;

Character *create_character(const char *name);
void move_character(Character *character, Room *room);
Character *find_character(Character *const characters[], size_t count, const char *name);
void free_character(Character *character);

#endif
```

```c
/* src/characters.c */
#include "characters.h"

#include <stdlib.h>
#include <string.h>

/* Allocates a character standing in no room with nothing in hand.
 * @return the new character, or NULL when memory is exhausted. */
Character *create_character(const char *name) {
    Character *character = calloc(1, sizeof *character);
    if (character != NULL) {
        character->name = name;
    }
    return character;
}

/* Puts the character in room; the inventory travels with it. */
void move_character(Character *character, Room *room) {
    character->room = room;
}

/* @return the character called name among characters, or NULL. */
Character *find_character(Character *const characters[], size_t count, const char *name) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(characters[i]->name, name) == 0) {
            return characters[i];
        }
    }
    return NULL;
}

/* Frees the character and everything it carries. */
void free_character(Character *character) {
    if (character != NULL) {
        free_items(character->inventory);
        free(character);
    }
}
```

- [x] **Step 4: Run test to verify it passes**

Run: `make build/test/test_characters && build/test/test_characters`
Expected: `test_characters: 11 checks, 0 failures`.

- [x] **Step 5: Commit**

```bash
git add src/characters.h src/characters.c test/unit/test_characters.c
git commit -m "feat: add characters with move_character"
```

---

### Task 4: World setup, teardown and seeding (`adventure.c` part 1)

**Files:**
- Create: `src/adventure.c`
- Test: `test/unit/test_adventure.c` (include trick, see `docs/conventions.md` section 6)

**Interfaces:**
- Consumes: everything from Tasks 1-3.
- Produces (all `static` in `adventure.c`, reachable from the test through the include trick):
  - `enum { GRID_SIDE = 3, ROOM_COUNT = 9, CHARACTER_COUNT = 5, ITEM_COUNT = 6, MAX_CLUES = 10, LINE_CAPACITY = 256 };`
  - `typedef enum { GAME_PLAYING, GAME_WON, GAME_LOST, GAME_QUIT } GameState;`
  - `typedef struct Game { Room *rooms[ROOM_COUNT]; Character *characters[CHARACTER_COUNT]; Character *avatar; const Room *answer_room; const char *answer_item; const Character *answer_character; int clues_used; GameState state; } Game;`
  - `static const char *const ROOM_NAMES[ROOM_COUNT]`, `ROOM_DESCRIPTIONS[ROOM_COUNT]`, `CHARACTER_NAMES[CHARACTER_COUNT]`, `ITEM_NAMES[ITEM_COUNT]`
  - `static void seed_random(void);` `static size_t random_below(size_t bound);`
  - `static bool setup_game(Game *game);` expects a zero-initialised `Game`; false on allocation failure (partial state is still safe to tear down)
  - `static void teardown_game(Game *game);`
  - `int main(void)` placeholder that sets up, tears down and returns `EXIT_SUCCESS`

- [x] **Step 1: Write the failing test**

```c
/* test/unit/test_adventure.c */
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
```

- [x] **Step 2: Run test to verify it fails**

Run: `make build/test/test_adventure`
Expected: compile error, `adventure.c` not found.

- [x] **Step 3: Write minimal implementation**

```c
/* src/adventure.c */
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
```

- [x] **Step 4: Run test to verify it passes**

Run: `make build/test/test_adventure && build/test/test_adventure && make && build/adventure; echo $?`
Expected: `test_adventure: 46 checks, 0 failures`; the binary exits 0.

- [x] **Step 5: Commit**

```bash
git add src/adventure.c test/unit/test_adventure.c
git commit -m "feat: build and tear down the shuffled world with seeded randomness"
```

---

### Task 5: Line reading, parsing and the command table (`adventure.c` part 2)

**Files:**
- Modify: `src/adventure.c`
- Test: `test/unit/test_adventure.c`

**Interfaces:**
- Produces (static):
  - `typedef void (*CommandHandler)(Game *game, const char *argument);`
  - `typedef struct Command { const char *name; const char *usage; const char *description; CommandHandler handler; } Command;`
  - `static const Command COMMANDS[]` with help, list, look, go, take, drop, inventory, clue, quit
  - `static bool read_line(char *buffer, size_t capacity, FILE *in);`
  - `static void lowercase(char *text);`
  - `static void split_command(char *line, const char **verb, const char **argument);`
  - `static const Command *find_command(const char *verb);`
  - `static void print_usage(const char *command_name);`
  - `static void run_command(Game *game, char *line);`
  - `static void play(Game *game);` the loop; `main` calls it
  - `handle_help`, `handle_list`, `handle_quit` complete; the other handlers print `Usage:` placeholders replaced in Tasks 6 and 7

- [x] **Step 1: Write the failing tests** (append to `test_adventure.c`, register in `main`)

```c
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
    FILE *in = tmpfile();
    CHECK(in != NULL);
    fputs(text, in);
    rewind(in);
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
```

- [x] **Step 2: Run test to verify it fails**

Run: `make build/test/test_adventure`
Expected: compile errors, `lowercase`, `split_command`, `find_command`, `read_line`, `run_command` undeclared.

- [x] **Step 3: Write minimal implementation** (insert after `teardown_game`, replace `main`)

```c
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
```

Add `#include <ctype.h>` at the top. Until Tasks 6 and 7 land, `handle_look`,
`handle_go`, `handle_take`, `handle_drop`, `handle_inventory` and
`handle_clue` are stubs that call `print_usage` with their own name.

- [x] **Step 4: Run test to verify it passes**

Run: `make build/test/test_adventure && build/test/test_adventure >/dev/null && printf 'HELP\nlist\nfoo\nquit\n' | CLUE_SEED=1 build/adventure`
Expected: `0 failures`; the transcript shows the help table, the three list lines, the unknown-command hint and `Goodbye.`.

- [x] **Step 5: Commit**

```bash
git add src/adventure.c test/unit/test_adventure.c
git commit -m "feat: add the command table, line parsing and the game loop"
```

---

### Task 6: look, go, take, drop, inventory (`adventure.c` part 3)

**Files:**
- Modify: `src/adventure.c`
- Test: `test/unit/test_adventure.c`

**Interfaces:**
- Produces (static): full `handle_look`, `handle_go`, `handle_take`, `handle_drop`, `handle_inventory`; helpers `print_exits`, `print_characters_here`, `print_items`.

- [x] **Step 1: Write the failing tests** (append, register in `main`)

```c
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
    run_command(&game, take);
    CHECK_EQ_INT(count_items(game.avatar->inventory), 1);
    CHECK(room->items == NULL);
    char ghost[] = "take unicorn";
    run_command(&game, ghost);
    CHECK_EQ_INT(count_items(game.avatar->inventory), 1);
    teardown_game(&game);
}
```

- [x] **Step 2: Run test to verify it fails**

Run: `make build/test/test_adventure && build/test/test_adventure >/dev/null`
Expected: FAIL lines for `game.avatar->room == game.rooms[1]` and the take/drop checks (stubs do nothing).

- [x] **Step 3: Write the implementation** (replace the stubs)

```c
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
```

- [x] **Step 4: Run test to verify it passes**

Run: `make build/test/test_adventure && build/test/test_adventure >/dev/null && printf 'look\ngo north\ngo south\ninventory\n' | CLUE_SEED=1 build/adventure`
Expected: `0 failures`; the transcript shows the room description with four exit lines, then either a move or `You cannot go ...`.

- [x] **Step 5: Commit**

```bash
git add src/adventure.c test/unit/test_adventure.c
git commit -m "feat: add look, go, take, drop and inventory commands"
```

---

### Task 7: The clue command, winning and losing (`adventure.c` part 4)

**Files:**
- Modify: `src/adventure.c`
- Test: `test/unit/test_adventure.c`

**Interfaces:**
- Produces (static): `typedef struct ClueResult { bool room; bool character; bool item; } ClueResult;`, `evaluate_clue`, `print_matches`, `resolve_clue`, full `handle_clue`.

- [x] **Step 1: Write the failing tests** (append, register in `main`)

```c
static void rig_answer(Game *game, bool room, bool character, bool item) {
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
    add_item(item ? &game->avatar->inventory : &elsewhere->items, node);
}

static void test_evaluate_clue_reports_each_match(void) {
    Game game = {0};
    srand(5);
    CHECK(setup_game(&game));
    rig_answer(&game, true, false, true);
    ClueResult result = evaluate_clue(&game);
    CHECK(result.room);
    CHECK(!result.character);
    CHECK(result.item);
    teardown_game(&game);
}

static void test_clue_moves_character_and_wins_on_three_matches(void) {
    Game game = {0};
    srand(5);
    CHECK(setup_game(&game));
    rig_answer(&game, true, false, true);
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
    rig_answer(&game, false, false, false);
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
```

- [x] **Step 2: Run test to verify it fails**

Run: `make build/test/test_adventure`
Expected: compile error, `ClueResult` and `evaluate_clue` undeclared.

- [x] **Step 3: Write the implementation** (replace the `handle_clue` stub)

```c
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
        printf("That was your %dth clue. It was %s in the %s with the %s. You lose.\n",
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
```

- [x] **Step 4: Run test to verify it passes**

Run: `make build/test/test_adventure && build/test/test_adventure >/dev/null && make && make check`
Expected: `0 failures`; `make check` clean.

- [x] **Step 5: Commit**

```bash
git add src/adventure.c test/unit/test_adventure.c
git commit -m "feat: add the clue command with win and loss detection"
```

---

### Task 8: End-to-end transcript suite

**Files:**
- Create: `test/e2e/run_e2e.sh`, `test/e2e/cases/<name>.in`, `<name>.expected`, optional `<name>.seed`
- Modify: `test/run_tests.sh` already calls `test/e2e/run_e2e.sh`

**Interfaces:**
- Consumes: `build/adventure`, `CLUE_SEED`.

- [ ] **Step 1: Write the runner**

```bash
#!/usr/bin/env bash
# Runs every test/e2e/cases/<name>.in through build/adventure with CLUE_SEED
# from <name>.seed (default 1) and diffs stdout+stderr against <name>.expected.
set -euo pipefail
cd "$(dirname "$0")/../.."
binary=build/adventure
cases=test/e2e/cases
scratch=$(mktemp -d "${TMPDIR:-/tmp}/clue-e2e.XXXXXX")
trap 'rm -rf "$scratch"' EXIT
status=0
for input in "$cases"/*.in; do
    name=$(basename "$input" .in)
    seed=1
    [[ -f "$cases/$name.seed" ]] && seed=$(<"$cases/$name.seed")
    expected_exit=0
    [[ -f "$cases/$name.exit" ]] && expected_exit=$(<"$cases/$name.exit")
    actual_exit=0
    CLUE_SEED=$seed timeout 5 "$binary" <"$input" >"$scratch/$name.out" 2>&1 || actual_exit=$?
    if [[ $actual_exit -eq $expected_exit ]] && diff -u "$cases/$name.expected" "$scratch/$name.out" >"$scratch/$name.diff"; then
        echo "PASS e2e/$name"
    else
        echo "FAIL e2e/$name (exit $actual_exit, expected $expected_exit)"
        cat "$scratch/$name.diff"
        status=1
    fi
done
exit $status
```

- [ ] **Step 2: Write the cases**

Explore the seed-1 layout with `printf 'look\nlist\n' | CLUE_SEED=1 build/adventure`
and pick seeds for the cases that need a particular layout. Each case is an
`.in` script; the `.expected` file is the reviewed transcript. Required cases:

| Case | Input | Must show |
| --- | --- | --- |
| `help` | `help`, `quit` | the nine-row table |
| `list` | `list`, `quit` | the three catalogue lines |
| `look_corner` | `look`, `quit` (seed chosen so the start room is a corner) | two `nothing` exits |
| `go_wall` | `go` into a wall, `go up`, `go`, `quit` | `You cannot go`, `There is no direction`, `Usage: go DIRECTION` |
| `take_drop` | walk to an item, `take`, `inventory`, `drop`, `look`, `take unicorn`, `drop rope` | round trip and both refusals |
| `inventory` | `inventory`, `quit` | `You are carrying nothing.` |
| `win` | walk to the answer room, carry the answer item, `clue <answer character>` | three matches and `You win!` |
| `lose` | ten `clue scarlet` in a wrong room | `Clues used: 9 of 10.` then `You lose.` |
| `unknown` | `dance`, `GO NORTH`, `quit` | the hint and case-insensitive handling |
| `eof` | `look` with no `quit` | `Goodbye.` and exit 0 |

Generate each `.expected` with the same command the runner uses, then read it
and confirm it shows the listed behaviour before committing it.

- [ ] **Step 3: Run the suite to verify it passes**

Run: `make test`
Expected: `PASS` for every unit binary and every e2e case, exit 0.

- [ ] **Step 4: Commit**

```bash
git add test/e2e
git commit -m "test: add end-to-end transcript cases under fixed CLUE_SEED"
```

---

### Task 9: Gate, review fixes and README

**Files:**
- Modify: `README.txt`

- [ ] **Step 1: Run the gate**

Run: `make clean && make && make check && make test && make dist`
Expected: every step exits 0; `dist/adventure` is built by the flat Makefile.

- [ ] **Step 2: Independent review**

A reviewer reads `docs/spec.md` and `src/`, lists every unmet rubric bullet
and audits every `calloc` against its `free` on every exit path (win, lose,
quit, EOF, setup failure). Fix each finding, re-run the gate.

- [ ] **Step 3: Finish README.txt**

Overview, build/run, the requirements map (every spec requirement bullet and
every rubric bullet with file and function), design notes, `Video: <VIDEO URL TO BE ADDED>`.

- [ ] **Step 4: Commit**

```bash
git add README.txt
git commit -m "docs: finish README with the requirements map"
```
