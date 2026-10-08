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
