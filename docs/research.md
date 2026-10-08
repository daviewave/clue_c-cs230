# Research notes

One section per core topic: sources, practices adopted, pitfalls avoided. The
game is a 3x3 grid of 9 rooms, 5 non-player characters, 6 items, singly linked
item lists per room and for the inventory, and a `clue CHARACTER` command.
Flags are those in `conventions.md` section 2; with no sanitizer or valgrind
here, `gcc -fanalyzer` plus review are the only memory checks.

## 1. Singly linked-list ownership

### Sources

- Linus Torvalds' indirect-pointer removal idiom, both versions of the code:
  https://grisha.org/blog/2013/04/02/linus-on-understanding-pointers/
- GCC analyzer trouble following pointers stored into list nodes (topic 5):
  https://gcc.gnu.org/pipermail/gcc-bugs/2026-August/970378.html

### Adopted

- One `Item` node type with a `next` pointer, allocated once at startup. Every
  node lives in exactly one list at all times: one of the 9 room lists or the
  inventory. `take` and `drop` never allocate; they relink.
- `drop_item(Item **head, const char *name)` walks a pointer to the link:
  `Item **link = head; while (*link) { if match { Item *found = *link;
  *link = found->next; found->next = NULL; return found; } link =
  &(*link)->next; }`. No `prev` variable, no head special case. Returns the
  unlinked node or `NULL`; the caller owns the result.
- `add_item(Item **head, Item *item)` pushes at the head (order of a room's
  items is unspecified by the spec). `take` is `add_item(&inventory,
  drop_item(&room->items, name))` after a `NULL` check; `drop` is the mirror.
- Teardown walks the 9 room lists and the inventory once each; the
  single-owner invariant means every node is freed exactly once. Rooms,
  characters and the avatar are freed afterwards in one `game_free`, called
  from every exit path (win, lose, quit, EOF).
- Header comments state ownership: "takes ownership of `item`" on `add_item`,
  "caller owns the returned node" on `drop_item`.

### Avoided

- Allocating a fresh node on `take` and freeing the old: doubles allocations,
  complicates the leak audit, and builds the store/load chain the analyzer
  cannot follow.
- Tracking `prev` with a head special case: two paths, two bugs.

## 2. Fisher–Yates shuffle with `rand()`

### Sources

- C99 (N1256) 7.20.2.1 and 7.20.2.2: https://port70.net/~nsz/c/c99/n1256.html
- cppreference `rand`: https://en.cppreference.com/w/c/numeric/random/rand
- cppreference `srand`: https://en.cppreference.com/w/c/numeric/random/srand
- glibc manual: https://sourceware.org/glibc/manual/latest/html_node/ISO-Random.html
- `rand(3)`: https://man7.org/linux/man-pages/man3/rand.3.html
- Fisher–Yates, Sattolo's cycle and the implementation-errors section:
  https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle

### Adopted

- Backward (Durstenfeld) form over a `size_t` index:
  `for (i = n - 1; i > 0; i--) { j = (size_t)rand() % (i + 1); swap(a[i],
  a[j]); }`. `j` ranges over `0..i` inclusive, so `a[i]` may stay put.
- Rooms: shuffle the array of 9 room pointers; slot `k` is row `k / 3`,
  column `k % 3`; then link north/south/east/west, `NULL` off the edge.
- Items, at most one per room: shuffle a 9-entry index array once, use the
  first 6 entries. Fisher–Yates can stop early, so this is a partial shuffle,
  not six rejection loops.
- Characters: 5 independent `rand() % 9` draws (sharing is allowed). Answer:
  one draw each over 9 rooms, 6 items, 5 characters.
- `srand` is called exactly once in `main`: `CLUE_SEED` via `strtoul` when
  set and valid, else `(unsigned)time(NULL)`. Same seed, same layout, same
  transcript, which is what the e2e cases rely on.
- C99 guarantees only that a seed reproduces its sequence on the same
  implementation and that `RAND_MAX >= 32767`; the algorithm is the library's
  (glibc: `RAND_MAX` 2147483647, the `random(3)` generator). The standard's
  own "portable implementation" example exists because sequences differ
  across libcs. `design.md` records that the expected transcripts are tied to
  this glibc, and unit tests check properties (permutation, bounds, distinct
  item rooms) rather than values.
- Modulo bias is accepted: with `RAND_MAX = 2^31 - 1` and `n <= 9` the
  favoured residues gain at most `9 / 2^31`, about `4e-9` relative, below
  anything observable in a game.

### Avoided

- Drawing `j` from `0..i-1`: that is Sattolo's algorithm, which yields only
  cyclic permutations, so no room ever lands in its original slot and the
  result is not uniform over the 9! layouts.
- Drawing `j` from the whole array each iteration (`rand() % n`): `n^n`
  outcomes cannot cover `n!` permutations evenly.

## 3. Command dispatch tables

### Sources

- INN `lib/dispatch.c`: sorted `{ command, min_args, max_args, callback }`
  table, `bsearch` + `strcasecmp`, explicit `unknown` and `syntax` handlers:
  https://codegraph.jelmer.uk/inn2/2.7.4-1/lib/dispatch.c
- GCC `-Wmissing-prototypes` / `-Wstrict-prototypes`:
  https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html

### Adopted

- `typedef GameStatus (*CommandHandler)(Game *game, const char *argument);`
  and `typedef struct { const char *name; const char *usage; CommandHandler
  run; } Command;` in `adventure.c`.
- `static const Command COMMANDS[] = { {"help", "help", cmd_help}, {"list",
  "list", cmd_list}, {"look", "look", cmd_look}, {"go", "go DIRECTION",
  cmd_go}, {"take", "take ITEM", cmd_take}, {"drop", "drop ITEM", cmd_drop},
  {"inventory", "inventory", cmd_inventory}, {"clue", "clue CHARACTER",
  cmd_clue}, {"quit", "quit", cmd_quit} };` with `COMMAND_COUNT = sizeof
  COMMANDS / sizeof COMMANDS[0]`. Nine entries: a linear `strcmp` scan over
  the already-lowercased verb; `bsearch` would not pay for itself.
- Display order, not sorted order: `help` prints the table top to bottom and
  the `help` e2e transcript is diffed verbatim, so the order is the spec's.
- Handlers are `static` with full prototypes above the table. `cmd_help`
  iterates the table that contains it, so its forward declaration is
  mandatory. Every handler returns `GameStatus` (`CONTINUE`, `WON`, `LOST`,
  `QUIT`), giving the main loop one exit test and one teardown.
- Unknown verb: `Unknown command "<verb>". Type help for commands.` and
  continue. Missing argument: print the entry's `usage`.

### Avoided

- An `if / else if (strcmp(...))` chain that `help` cannot print from.

## 4. Robust line reading with `fgets`

### Sources

- cppreference `fgets`: https://en.cppreference.com/w/c/io/fgets
- POSIX `fgets`: https://man7.org/linux/man-pages/man3/fgets.3p.html
- cppreference `feof`, indicator semantics and the `while (!feof)` mistake:
  https://en.cppreference.com/w/c/io/feof
- cppreference `tolower` / `isspace`, argument domain `unsigned char` or
  `EOF`: https://en.cppreference.com/w/c/string/byte/tolower and
  https://en.cppreference.com/w/c/string/byte/isspace
- cppreference `strtok`, static state, destructive, not reentrant:
  https://en.cppreference.com/w/c/string/byte/strtok

### Adopted

- `read_line(char *buffer, size_t size, FILE *in)` wraps `fgets` and returns
  `LINE_OK`, `LINE_EOF` or `LINE_ERROR`; on `NULL` it checks `ferror` before
  `feof`. The loop condition is that enum, never `!feof(stdin)`.
- No `'\n'` in the buffer and `feof` not set means the line was too long:
  drain with `while ((c = getc(in)) != '\n' && c != EOF) {}` and reject the
  command with a "line too long" message, so the tail of a pasted line can
  never run as a second command. `enum { LINE_MAX_LENGTH = 256 }`.
- Strip the newline with `buffer[strcspn(buffer, "\r\n")] = '\0'`, which also
  tolerates CRLF and is a no-op on a newline-less final line.
- Lowercase with `*p = (char)tolower((unsigned char)*p)`: the inner cast
  keeps negative `char` out of `tolower`, the outer narrows the `int` result.
- `parse_command` skips leading `isspace((unsigned char)c)`, cuts the verb
  at the first space with one `'\0'`, skips spaces, trims trailing whitespace
  from the argument. Both parts point into the caller's buffer; no copies, no
  library state. An empty line just redisplays the prompt.
- EOF mid-game ends the loop with `QUIT`, prints a newline to close the
  prompt line, and reaches the same `game_free` as every other exit; this is
  the failure mode the orchestration plan assigns to this project.

### Avoided

- `strtok`: static cursor, overwrites delimiters, and a unit test that
  tokenises two strings in turn cannot trust it. The hand-written split is
  about 15 lines and testable alone.
- `gets`; `scanf("%s")` without a width; plain `char` passed to `ctype`.

## 5. `gcc -fanalyzer`

### Sources

- GCC static analyzer options, the `-Wanalyzer-*` list and the "neither
  sound nor complete" statement:
  https://gcc.gnu.org/onlinedocs/gcc/Static-Analyzer-Options.html
- The `malloc (deallocator)` attribute the analyzer honours:
  https://gcc.gnu.org/onlinedocs/gcc-14.2.0/gcc/Common-Function-Attributes.html
- Red Hat introduction to the GCC 10 analyzer (state graph, path limits):
  https://developers.redhat.com/blog/2020/03/26/static-analysis-in-gcc-10
- False `-Wanalyzer-malloc-leak` when a pointer is stored into a list node
  and freed through another path: PR 126832
  https://gcc.gnu.org/pipermail/gcc-bugs/2026-August/970378.html and
  PR 94365 https://gcc.gnu.org/bugzilla/show_bug.cgi?id=94365

### Adopted

- `make check` runs `gcc -fanalyzer` with the full warning set over each
  `src/` file; output must be empty. It covers leaks, double free, use after
  free, `NULL` and possible-`NULL` dereference, free of non-heap memory.
- Every `malloc` is inside a `*_create` in the module that owns the type;
  every `free` is inside the matching `*_destroy`; `game_free` calls the
  destroys in reverse creation order. Handlers `return` a status rather than
  calling `exit()`, so the single teardown is reached on every path.
- `*_create` checks `malloc` and returns `NULL`; `game_create` unwinds what
  it already built on any `NULL`, so a failed allocation leaks nothing.
- List teardown reads `next` before `free`: `while (node) { Item *next =
  node->next; free(node); node = next; }`.
- Ownership transfer is in the header comment of every function that stores
  or returns a heap pointer, so a reviewer pairs each `malloc` with its
  `free` from the headers alone.
- Nodes move by relinking (topic 1), so the only store-then-free-elsewhere
  chain is a plain singly linked walk, which the analyzer follows; the
  embedded `list_head` / `container_of` shape behind PR 126832 and PR 94365
  is not used.

### Avoided

- Freeing in a different file from a named destroy: the analyzer works per
  translation unit, so a `free` hidden elsewhere reads as a leak.
- `exit()` from a handler or from `read_line`, a genuine leak of every room,
  item and character.

## 6. `-Wconversion` and `-Wpedantic` in C99

### Sources

- GCC warning options: `-Wconversion` enables `-Wsign-conversion` and
  `-Wfloat-conversion` in C; `-Wpedantic`, `-Wshadow`, `-Wvla`,
  `-Wstrict-prototypes`, `-Wmissing-prototypes`:
  https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
- `tolower` returns `int` (topic 4 sources); `rand()` returns `int` in
  `0..RAND_MAX` (topic 2 sources).

### Adopted

- `rand()` is `int`; every modulus against a `size_t` count is
  `(size_t)rand() % count`. The cast is safe because `rand()` is never
  negative, and it is the only way `-Wsign-conversion` stays silent.
- Array indices and loop counters are `size_t`. `ROOM_COUNT = 9`,
  `GRID_SIDE = 3`, `CHARACTER_COUNT = 5`, `ITEM_COUNT = 6`, `MAX_CLUES = 10`
  are `enum` constants and are cast to `size_t` where they meet one. The
  shuffle loop runs `i = n - 1; i > 0; i--` so an unsigned counter never
  wraps below zero.
- `tolower` results are cast to `char` on assignment; `getc` results stay
  `int` until compared with `EOF`.
- `CLUE_SEED` via `strtoul` is range-checked against `UINT_MAX` and cast to
  `unsigned`; `time(NULL)` is cast to `unsigned` for `srand`.
- All table strings (room names, descriptions, command names, usage) are
  `const char *`, so literals never decay to a mutable `char *`.
- Every non-`main` function is `static` or prototyped in its header; every
  prototype names its parameter types (`int f(void)`, never `int f()`).

### Avoided

- `int i; for (i = 0; i < strlen(s); i++)`: signed/unsigned comparison and
  a `size_t` to `int` narrowing.
- `char c = getc(in)` (loses `EOF`) and `char c = tolower(x)` with no cast.
- `-Wpedantic` extensions: empty initialiser `{}`, zero-length arrays,
  anonymous unions, trailing `;` after a function body, `__attribute__` in
  submitted sources (Gradescope builds with plain `gcc -std=c99 -Wall`).
