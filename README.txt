Clue text adventure (CS230 project 2)
=====================================

Overview
--------
A text adventure in C99 that plays a round of Clue. Nine rooms are shuffled
onto a 3x3 board and linked by north/south/east/west pointers; five suspects
and six items are scattered over the rooms; a secret room, item and suspect
are drawn as the answer. The player types commands at a "> " prompt: look
around, walk between rooms, take and drop items, and accuse with
"clue CHARACTER". A clue summons that character into the player's room and
reports "Room Match", "Character Match" and "Item Match" for whichever parts
of the answer are present (an item in the inventory counts as in the room).
Three matches on one clue win the game; a tenth clue without all three loses.

The code is split exactly as the spec asks:

    src/items.c, items.h            Item struct and the singly linked item list
                                    (create_item, add_item, drop_item, find_item,
                                    count_items, free_items)
    src/rooms.c, rooms.h            Room struct, Direction enum, 3x3 grid linking,
                                    Fisher-Yates shuffle, direction parsing
    src/characters.c, characters.h  Character struct (also used for the avatar),
                                    move_character, find_character
    src/adventure.c                 catalogues, world setup and teardown, the
                                    command table, input parsing, the game loop,
                                    the clue logic and main

Every struct instance in the world (rooms, items, characters, avatar) is
allocated with calloc and freed on every way out of the game: win, lose,
"quit", end of input (Ctrl-D) and allocation failure during setup.

Build and run
-------------
    make              builds build/adventure with gcc -std=c99 and the full
                      warning set (-Wall -Wextra -Wpedantic ... -Werror)
    make run          builds and plays
    make test         builds and runs the unit tests and the end-to-end
                      transcript tests (test/run_tests.sh)
    make check        gcc -fanalyzer over every source file
    make dist         flat Gradescope bundle in dist/ (sources, README.txt and
                      a generated Makefile) and proves it builds there

In the Gradescope bundle (or after `make dist`, inside dist/):

    make
    ./adventure

Randomness: set CLUE_SEED to an unsigned integer to replay the same board,
placements and answer (`CLUE_SEED=1 ./adventure`). Without it the game seeds
from the clock. The video shows the seed used, so the game is reproducible.

Commands (type `help` in the game):

    help              show this table of commands
    list              list every room, character and item
    look              describe the room you are in
    go DIRECTION      walk north, south, east or west
    take ITEM         pick up an item lying in the room
    drop ITEM         put down an item you are carrying
    inventory         show what you are carrying
    clue CHARACTER    summon a character and test your theory
    quit              leave the game

Commands are case-insensitive. Rooms: kitchen, ballroom, conservatory,
dining, billiard, library, lounge, hall, study. Characters: scarlet, mustard,
white, green, peacock. Items: candlestick, knife, pipe, revolver, rope, wrench.

Requirements map
----------------
Game requirements (spec section "Game Requirements"):

  9 rooms on a 3x3 board, including the starting room
      adventure.c: ROOM_NAMES / ROOM_DESCRIPTIONS (9 entries), GRID_SIDE = 3,
      create_rooms(); rooms.c: link_grid() wires the row-major 3x3 grid.
  Room locations randomly initialised before the game starts
      adventure.c: create_rooms() calls shuffle_rooms() with random_below();
      rooms.c: shuffle_rooms() is a Fisher-Yates shuffle of the room array,
      then link_grid() gives every room its north/south/east/west pointers.
  5 characters other than the avatar, each in a random room
      adventure.c: CHARACTER_NAMES (5 entries), create_characters() places
      each with move_character(..., rooms[random_below(ROOM_COUNT)]).
  At least 6 items, random rooms, at most one item per room
      adventure.c: ITEM_NAMES (6 entries), place_items() shuffles a copy of
      the room array and gives item k to the k-th room of the copy, so six
      distinct rooms each hold exactly one item.
  Random answer: a room, an item and a character
      adventure.c: pick_answer() fills answer_room, answer_item,
      answer_character.
  Each room has a linked list of items
      rooms.h: Room.items is an Item * list head; items.h: Item.next.
  Avatar with an inventory (a linked list of items)
      characters.h: Character.inventory; adventure.c: create_avatar() builds
      the avatar as a Character and Game.avatar holds it.
  A table of commands
      adventure.c: typedef struct Command { name, usage, description,
      handler } and the static const COMMANDS[] array; find_command() looks
      a verb up; run_command() dispatches through the handler pointer.
  "help" looks up the command table
      adventure.c: handle_help() prints one row per COMMANDS[] entry.
  "list" shows items, rooms and characters
      adventure.c: handle_list() prints ROOM_NAMES, CHARACTER_NAMES and
      ITEM_NAMES through print_names().
  "look" shows the room, the rooms in each direction, characters, items
      adventure.c: handle_look() prints name and description, then
      print_exits() (four lines, "nothing" for a wall), print_characters_here()
      and print_items().
  "go DIRECTION" moves through the room pointer (north, south, east, west)
      adventure.c: handle_go() parses with parse_direction(), follows
      room_in_direction() (rooms.c, reads Room.exits[direction]) and moves the
      avatar with move_character(); a wall prints "You cannot go ... from here."
  "take ITEM" picks up an item from the room
      adventure.c: handle_take() = drop_item() from the room list +
      add_item() to the inventory.
  "drop ITEM" drops an item from the inventory
      adventure.c: handle_drop() = drop_item() from the inventory + add_item()
      to the room list.
  "inventory" lists the avatar's items
      adventure.c: handle_inventory() via print_items().
  "clue CHARACTER" makes a guess
      adventure.c: handle_clue().
  Clue step 1: move the named character into the avatar's room
      adventure.c: handle_clue() calls move_character(character, avatar->room)
      (characters.c).
  Clue step 2: report Room Match / Character Match / Item Match
      adventure.c: evaluate_clue() computes the three flags (the inventory is
      searched as well as the room for the item; other items do not matter);
      print_matches() prints the exact spec strings.
  Clue step 3: winning state on three matches in one clue
      adventure.c: resolve_clue() sets GAME_WON and prints "You win!".
  Clue step 3: losing state after the 10th clue without all three
      adventure.c: Game.clues_used is incremented per valid clue;
      resolve_clue() sets GAME_LOST when clues_used reaches MAX_CLUES (10).
  Code organised into rooms.c/.h, items.c/.h, characters.c/.h, adventure.c
      see the Overview; add_item and drop_item live in items.c,
      move_character in characters.c, input reading (read_line, lowercase,
      split_command), command interpretation (run_command) and main in
      adventure.c.
  Makefile compiling with gcc -std=c99
      Makefile (development, STD := -std=c99) and the generated dist/Makefile
      (CFLAGS = -std=c99 -Wall -Wextra -O2).

Code requirements:

  C structs represent game objects
      Item (items.h), Room (rooms.h), Character (characters.h), Game, Command
      and ClueResult (adventure.c).
  Pointers
      every list link (Item.next), every room exit (Room.exits[]), the
      character-to-room link (Character.room), the handler function pointers
      in COMMANDS[], the RandomPicker injected into shuffle_rooms(), and the
      pointer-to-pointer walk in add_item()/drop_item().
  Dynamic allocation with malloc/calloc
      create_item(), create_room(), create_character() each calloc one node.
  Deallocation with free
      free_items() (items.c) frees a whole list; free_room() frees the room and
      its items; free_character() frees the character and its inventory;
      teardown_game() (adventure.c) calls them for everything and runs on win,
      loss, quit, end of input and setup failure.

Rubric items:

  Project requirements / Makefile that compiles the code
      Makefile; `make dist` produces the flat bundle and compiles it as proof.
  Functions declared and used properly
      every exported function is prototyped in its header; every helper in
      adventure.c is static and prototyped before the command table.
  Data structures and data types declared and used properly
      typedef'd structs and enums above; Direction indexes Room.exits[];
      GameState drives the loop in play().
  Clear naming
      functions are verbs (create_rooms, place_items, handle_take,
      evaluate_clue); the tables are nouns (ROOM_NAMES, COMMANDS).
  Global variables minimised
      no mutable globals; the only file-scope data are static const
      catalogues and the static const command table. The game state is a
      Game struct on main's stack passed by pointer.
  Control flow and algorithms
      one loop in play(); each handler validates its argument then does one
      thing; the clue logic is three small functions (evaluate_clue,
      print_matches, resolve_clue); no unreachable code (gcc -Werror with the
      full warning set and gcc -fanalyzer both pass).
  Coding style
      four-space indentation, K&R braces, one style throughout.
  Comments
      every function has a header comment stating its purpose and, where the
      signature does not say it, its parameters and return value; non-obvious
      struct fields are commented at their declaration; the design rationale
      is in docs/design.md.
  README.txt
      this file.
  Video
      see the last line.

Design notes
------------
- Each item node lives in exactly one list at a time (a room's or the
  inventory). take/drop move the node with drop_item + add_item and never
  allocate, so teardown frees every item exactly once.
- The avatar is a Character with an inventory; the suspects are Characters
  with an empty one. move_character therefore serves both "go" and "clue".
- Room exits are an array indexed by the Direction enum, so "go" is one
  lookup and "look" prints the four exits in a loop.
- The random source is injected into shuffle_rooms as a function pointer,
  which keeps rand() in one place (adventure.c: random_below) and lets the
  unit tests check an exact permutation.
- A misspelt character in "clue" is refused and does not count as one of the
  ten clues.
- Tests: test/unit/ (one file per module plus test_adventure.c, which
  includes adventure.c to reach its static functions) and test/e2e/ (stdin
  scripts with expected transcripts under fixed CLUE_SEED values).

Video: <VIDEO URL TO BE ADDED>
