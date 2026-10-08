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
