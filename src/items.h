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
