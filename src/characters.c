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
