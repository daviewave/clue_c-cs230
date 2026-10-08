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
