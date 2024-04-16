#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/*
Created by Ryan Srichai
https://github.com/Known4225
MIT licensed, this is open source software.

21.04.23:
unitype list, supports a variety of types

access items of a list:
list -> data[0]

access length of list:
list -> length

list functions:

create list:
list_t list = list_init();

access items of a list (as a void pointer):
void *item = list_item(list, [index]);

append to list:
list_append(list, (unitype) [data], 'i');

insert to list
list_insert(list, [index], (unitype) [data], 'i');

pop from list:
list_pop(list);

delete index from list:
list_delete(list, [index]);

delete range of indices from list (deletes from indexMin to indexMax - 1):
list_delete_range(list, [indexMin], [indexMax]);

find element in the list (must specify type):
list_find(list, [element], 'i');

count how many elements are in the list (must specify type):
list_count(list, [elements], 'i');

find and delete element in the list (must specify type):
list_remove(list, [element], 'i');

copy one list to another
list_copy(dest, source);

delete all the elements of a list
list_clear(list);

print the list:
list_print(list);

free the list (when done using):
list_free(list);
*/

struct list_f; // so basically im a good programmer
typedef struct list_f list_t;

typedef union { // supported types
    int i;
    unsigned int u;
    float f;
    double d;
    char c;
    char* s;
    void* p;
    list_t* r;
    long long l;
    short h;
    bool b;
} unitype;

struct list_f {
    unsigned int length;
    unsigned int realLength;
    char *type;
    unitype *data;
};

list_t* list_init() { // initialise a list
    list_t *list = malloc(sizeof(list_t));
    list -> length = 0;
    list -> realLength = 1;
    list -> type = calloc(1, sizeof(char));
    list -> data = calloc(1, sizeof(unitype));
    return list;
}

void* list_item(list_t *list, int index) { // accesses an item of the list as a void pointer
    void *ret;
    switch (list -> type[index]) {
        case 'i':
            ret = &list -> data[index].i;
        break;
        case 'u':
            ret = &list -> data[index].u;
        break;
        case 'f':
            ret = &list -> data[index].f;
        break;
        case 'd':
            ret = &list -> data[index].d;
        break;
        case 'c':
            ret = &list -> data[index].c;
        break;
        case 's':
            ret = list -> data[index].s;
        break;
        case 'p':
            ret = &list -> data[index].p;
        break;
        case 'r':
            ret = &list -> data[index].r;
        break;
        case 'l':
            ret = &list -> data[index].l;
        break;
        case 'h':
            ret = &list -> data[index].h;
        break;
        case 'b':
            ret = &list -> data[index].b;
        break;
        default:
            ret = NULL;
            printf("item - type not recognized\n");
        break;
    }
    return ret;
}

void list_free_lite(list_t *);
void list_free(list_t *);
void list_print_emb(list_t *);

void list_append(list_t *list, unitype data, char type) { // append to list, must specify type
    if (list -> realLength  <= list -> length) {
        list -> realLength *= 2;
        list -> type = realloc(list -> type, list -> realLength);
        list -> data = realloc(list -> data, list -> realLength * sizeof(unitype));
    }
    list -> type[list -> length] = type;
    if (type == 's') {
        list -> data[list -> length].s = strdup(data.s);
    } else {
        list -> data[list -> length] = data;
    }
    list -> length += 1;
}

void list_clear(list_t *list) {
    list_free_lite(list);
    list -> length = 0;
    list -> realLength = 1;
    list -> type = calloc(1, sizeof(char));
    list -> data = calloc(1, sizeof(unitype));
}

unitype list_pop(list_t *list) { // pops the last item of the list off and returns it
    if (list -> length > 0) {
        list -> length -= 1;
        unitype ret = list -> data[list -> length];
        if (list -> type[list -> length] == 'r') {
            list_free(list -> data[list -> length].r);
        }
        if (list -> type[list -> length] == 's' || list -> type[list -> length] == 'p') {
            free(list -> data[list -> length].p);
        }
        list -> type[list -> length] = (char) 0;
        list -> data[list -> length] = (unitype) 0;
        if (list -> length <= list -> realLength / 2 && list -> realLength > 1) {
            list -> realLength /= 2;
            list -> type = realloc(list -> type, list -> realLength);
            list -> data = realloc(list -> data, list -> realLength * sizeof(unitype));
        }
        return ret;
    } else {
        return (unitype) 0;
    }
}

unitype list_delete(list_t *list, int index) { // deletes the item at list[index] of the list and returns it
    while (index < 0) {index += list -> length;}
    index %= list -> length;
    unitype ret = list -> data[index];
    if (list -> type[index] == 'r') {
        list_free(list -> data[index].r);
    }
    if (list -> type[index] == 's' || list -> type[index] == 'p') {
        free(list -> data[index].p);
    }
    for (int i = index; i < list -> length - 1 ; i++) {
        list -> data[i] = list -> data[i + 1];
        list -> type[i] = list -> type[i + 1];
    }
    list -> length -= 1;
    list -> type[list -> length] = (char) 0;
    list -> data[list -> length] = (unitype) 0;
    if (list -> length <= list -> realLength / 2 && list -> realLength > 1) {
        list -> realLength /= 2;
        list -> type = realloc(list -> type, list -> realLength);
        list -> data = realloc(list -> data, list -> realLength * sizeof(unitype));
    }
    return ret;
}

void list_delete_range(list_t* list, int indexMin, int indexMax) { // deletes many items from the list spanning from [indexMin] to [indexMax - 1]
    if (indexMin > indexMax) {
        int swap = indexMin;
        indexMin = indexMax;
        indexMax = swap;
    }
    char zerod = 0; // edge case: "should've used list_clear"
    int difference = (indexMax - indexMin);
    list -> realLength = list -> length - difference;
    if (list -> realLength <= 1) {
        zerod = 1;
        list -> realLength = 1;
    }
    
    char *newType = malloc(list -> realLength * sizeof(char)); // no need to calloc we're gonna fill it all up anyway
    unitype *newData = malloc(list -> realLength * sizeof(unitype));
    for (int i = 0; i < indexMin; i++) {
        newType[i] = list -> type[i];
        newData[i] = list -> data[i];
    }
    for (int i = indexMax; i < list -> length; i++) {
        newType[i - difference] = list -> type[i];
        newData[i - difference] = list -> data[i];
    }
    list -> length = list -> realLength;
    if (zerod)
        list -> length = 0;
    free(list -> type);
    free(list -> data);
    list -> type = newType;
    list -> data = newData;
}

int unitype_check_equal (unitype item1, unitype item2, char typeItem1, char typeItem2) { // checks if two unitype items are equal
    if ((typeItem1 == 's' || typeItem2 == 's') && typeItem1 != typeItem2) { // logical xor but idk how to do it
        return 0;
    }
    switch (typeItem1) {
        case 'i':
            if (item1.i == item2.i) {return 1;}
        break;
        case 'u':
            if (item1.i == item2.i) {return 1;}
        break;
        case 'f':
            if (item1.f == item2.f) {return 1;}
        break;
        case 'd':
            if (item1.d == item2.d) {return 1;}
        break;
        case 'c':
            if (item1.c == item2.c) {return 1;} // BROKEN (???) in some gcc settings on certain computer (???)
            // if (strcmp(&item1.c, &item2.c) == 0) {return 1;}
        break;
        case 's':
            if (strcmp(item1.s, item2.s) == 0) {return 1;}
        break;
        case 'p':
            if (item1.p == item2.p) {return 1;} // questionable
        break;
        case 'r':
            if (item1.r == item2.r) {return 1;} // questionable^2 (doesn't check if lists are equivalent/congruent, just compares memory location)
        break;
        case 'l':
            if (item1.l == item2.l) {return 1;}
        break;
        case 'h':
            if (item1.h == item2.h) {return 1;}
        break;
        case 'b':
            if (item1.b == item2.b) {return 1;}
        break;
        default:
            printf("unitype_check_equal: type not recognised\n");
        break;
    }
    return 0;
}
/* returns the index of the first instance of the item in the list, returns -1 if not found (python) */
int list_find(list_t *list, unitype item, char type) {
    int trig = 0;
    for (int i = 0; i < list -> length; i++) {
        trig += unitype_check_equal(list -> data[i], item, list -> type[i], type);
        if (trig == 1) {
            return i;
        }
    }
    return -1;
}

/* duplicate of list_find */
int list_index(list_t *list, unitype item, char type) {
    int trig = 0;
    for (int i = 0; i < list -> length; i++) {
        trig += unitype_check_equal(list -> data[i], item, list -> type[i], type);
        if (trig == 1) {
            return i;
        }
    }
    return -1;
}

/* duplicate of list_find */
int list_search(list_t *list, unitype item, char type) {
    int trig = 0;
    for (int i = 0; i < list -> length; i++) {
        trig += unitype_check_equal(list -> data[i], item, list -> type[i], type);
        if (trig == 1) {
            return i;
        }
    }
    return -1;
}

/* binary search - only works with lists of ints/short/char */
int list_find_binary(list_t *list, int item) {
    int top = list -> length;
    int bottom = 0;
    int trig = top / 2;
    while (bottom < top) {
        if (list -> data[trig].i == item) {
            return trig;
        }
        if (list -> data[trig].i > item) {
            top = trig;
        } else {
            bottom = trig + 1;
        }
        trig = bottom + (top - bottom) / 2;
    }
    return -1;
}

/* same as list_find_binary */
int list_index_binary(list_t *list, int item) {
    int top = list -> length;
    int bottom = 0;
    int trig = top / 2;
    while (bottom < top) {
        if (list -> data[trig].i == item) {
            return trig;
        }
        if (list -> data[trig].i > item) {
            top = trig;
        } else {
            bottom = trig + 1;
        }
        trig = bottom + (top - bottom) / 2;
    }
    return -1;
}

/* same as list_find_binary */
int list_search_binary(list_t *list, int item) {
    int top = list -> length;
    int bottom = 0;
    int trig = top / 2;
    while (bottom < top) {
        if (list -> data[trig].i == item) {
            return trig;
        }
        if (list -> data[trig].i > item) {
            top = trig;
        } else {
            bottom = trig + 1;
        }
        trig = bottom + (top - bottom) / 2;
    }
    return -1;
}

int list_count(list_t *list, unitype item, char type) { // counts how many instances of an item is found in the list
    int count = 0;
    for (int i = 0; i < list -> length; i++) {
        count += unitype_check_equal(list -> data[i], item, list -> type[i], type);
    }
    return count;
}

/* deletes the first instance of the item from the list, returns the index the item was at, returns -1 and doesn't modify the list if not found (python but without ValueError) */
int list_remove(list_t *list, unitype item, char type) {
    int trig = 0;
    for (int i = 0; i < list -> length; i++) {
        trig += unitype_check_equal(list -> data[i], item, list -> type[i], type);
        if (trig == 1) {
            list_delete(list, i);
            return i;
        }
    }
    return -1;
}

/* prints a unitype item */
void unitype_print(unitype item, char type) {
    switch (type) {
        case 'i':
            printf("%d", item.i);
        break;
        case 'u':
            printf("%i", item.i);
        break;
        case 'f':
            printf("%f", item.f);
        break;
        case 'd':
            printf("%lf", item.d);
        break;
        case 'c':
            printf("%c", item.c);
        break;
        case 's':
            printf("%s", item.s);
        break;
        case 'p':
            printf("%p", item.p);
        break;
        case 'r':
            list_print_emb(item.r);
        break;
        case 'l':
            printf("%lld", item.l);
        break;
        case 'h':
            printf("%hi", item.h);
        break;
        case 'b':
            printf("%hi", item.b);
        break;
        default:
            printf("unitype_print: type not recognized\n");
            return;
        break;
    }
}

/* converts a unitype to a malloc'd string. Duplicates string types */
void unitype_to_string(unitype item, char type) {
    char *ret = NULL;
    int size;
    /* 
    Generating size of returned malloc'd string:
    https://stackoverflow.com/questions/3919995/determining-sprintf-buffer-size-whats-the-standard */
    switch (type) {
        case 'i':
            size = snprintf(NULL, 0, "%d", item.i);
            ret = malloc(size + 1);
            sprintf(ret, "%d", item.i);
        break;
        case 'u':
            size = snprintf(NULL, 0, "%i", item.i);
            ret = malloc(size + 1);
            sprintf(ret, "%i", item.i);
        break;
        case 'f':
            size = snprintf(NULL, 0, "%f", item.f);
            ret = malloc(size + 1);
            sprintf(ret, "%f", item.f);
        break;
        case 'd':
            size = snprintf(NULL, 0, "%lf", item.d);
            ret = malloc(size + 1);
            sprintf(ret, "%lf", item.d);
        break;
        case 'c':
            ret = malloc(2);
            sprintf(ret, "%c", item.c);
        break;
        case 's':
            ret = malloc(strlen(item.s) + 1);
            sprintf(ret, "%s", item.s);
        break;
        case 'p':
            size = snprintf(NULL, 0, "%p", item.p);
            ret = malloc(size + 1);
            sprintf(ret, "%p", item.p);
        break;
        case 'r':
            /* this is a fun project to do... for another time */
            printf("list to string feature not supported\n");
        break;
        case 'l':
            size = snprintf(NULL, 0, "%lld", item.l);
            ret = malloc(size + 1);
            sprintf(ret, "%lld", item.l);
        break;
        case 'h':
            size = snprintf(NULL, 0, "%hi", item.h);
            ret = malloc(size + 1);
            sprintf(ret, "%hi", item.h);
        break;
        case 'b':
            size = snprintf(NULL, 0, "%hi", item.b);
            ret = malloc(size + 1);
            sprintf(ret, "%hi", item.b);
        break;
        default:
            printf("unitype_to_string: type not recognized\n");
            return;
        break;
    }
}

/* converts a unitype to a malloc'd string. Duplicates string types. Locks doubles and floats to 0.truncate%lf, supports truncate values 0 - 10, put -1 for no truncation */
char *unitype_to_string_truncated_doubles(unitype item, char type, int truncate) {
    char *ret = NULL;
    int size;
    if (truncate < -1 || truncate > 10) {
        printf("unitype_to_string_truncated_doubles: invalid truncate value\n");
        return NULL;
    }
    /* 
    Generating size of returned malloc'd string:
    https://stackoverflow.com/questions/3919995/determining-sprintf-buffer-size-whats-the-standard */
    switch (type) {
        case 'i':
            size = snprintf(NULL, 0, "%d", item.i);
            ret = malloc(size + 1);
            sprintf(ret, "%d", item.i);
        break;
        case 'u':
            size = snprintf(NULL, 0, "%i", item.i);
            ret = malloc(size + 1);
            sprintf(ret, "%i", item.i);
        break;
        case 'f':
            switch (truncate) {
                case 0:
                    size = snprintf(NULL, 0, "%0.0f", item.f);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.0f", item.f);
                break;
                case 1:
                    size = snprintf(NULL, 0, "%0.1f", item.f);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.1f", item.f);
                break;
                case 2:
                    size = snprintf(NULL, 0, "%0.2f", item.f);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.2f", item.f);
                break;
                case 3:
                    size = snprintf(NULL, 0, "%0.3f", item.f);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.3f", item.f);
                break;
                case 4:
                    size = snprintf(NULL, 0, "%0.4f", item.f);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.4f", item.f);
                break;
                case 5:
                    size = snprintf(NULL, 0, "%0.5f", item.f);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.5f", item.f);
                break;
                case 6:
                    size = snprintf(NULL, 0, "%0.6f", item.f);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.6f", item.f);
                break;
                case 7:
                    size = snprintf(NULL, 0, "%0.7f", item.f);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.7f", item.f);
                break;
                case 8:
                    size = snprintf(NULL, 0, "%0.8f", item.f);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.8f", item.f);
                break;
                case 9:
                    size = snprintf(NULL, 0, "%0.9f", item.f);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.9f", item.f);
                break;
                case 10:
                    size = snprintf(NULL, 0, "%0.10f", item.f);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.10f", item.f);
                break;
                default:
                    size = snprintf(NULL, 0, "%f", item.f);
                    ret = malloc(size + 1);
                    sprintf(ret, "%f", item.f);
                break;
            }
        break;
        case 'd':
            switch (truncate) {
                case 0:
                    size = snprintf(NULL, 0, "%0.0lf", item.d);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.0lf", item.d);
                break;
                case 1:
                    size = snprintf(NULL, 0, "%0.1lf", item.d);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.1lf", item.d);
                break;
                case 2:
                    size = snprintf(NULL, 0, "%0.2lf", item.d);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.2lf", item.d);
                break;
                case 3:
                    size = snprintf(NULL, 0, "%0.3lf", item.d);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.3lf", item.d);
                break;
                case 4:
                    size = snprintf(NULL, 0, "%0.4lf", item.d);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.4lf", item.d);
                break;
                case 5:
                    size = snprintf(NULL, 0, "%0.5lf", item.d);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.5lf", item.d);
                break;
                case 6:
                    size = snprintf(NULL, 0, "%0.6lf", item.d);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.6lf", item.d);
                break;
                case 7:
                    size = snprintf(NULL, 0, "%0.7lf", item.d);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.7lf", item.d);
                break;
                case 8:
                    size = snprintf(NULL, 0, "%0.8lf", item.d);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.8lf", item.d);
                break;
                case 9:
                    size = snprintf(NULL, 0, "%0.9lf", item.d);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.9lf", item.d);
                break;
                case 10:
                    size = snprintf(NULL, 0, "%0.10lf", item.d);
                    ret = malloc(size + 1);
                    sprintf(ret, "%0.10lf", item.d);
                break;
                default:
                    size = snprintf(NULL, 0, "%lf", item.d);
                    ret = malloc(size + 1);
                    sprintf(ret, "%lf", item.d);
                break;
            }
        break;
        case 'c':
            ret = malloc(2);
            sprintf(ret, "%c", item.c);
        break;
        case 's':
            ret = malloc(strlen(item.s) + 1);
            sprintf(ret, "%s", item.s);
        break;
        case 'p':
            size = snprintf(NULL, 0, "%p", item.p);
            ret = malloc(size + 1);
            sprintf(ret, "%p", item.p);
        break;
        case 'r':
            /* this is a fun project to do... for another time */
            printf("list to string feature not supported\n");
        break;
        case 'l':
            size = snprintf(NULL, 0, "%lld", item.l);
            ret = malloc(size + 1);
            sprintf(ret, "%lld", item.l);
        break;
        case 'h':
            size = snprintf(NULL, 0, "%hi", item.h);
            ret = malloc(size + 1);
            sprintf(ret, "%hi", item.h);
        break;
        case 'b':
            size = snprintf(NULL, 0, "%hi", item.b);
            ret = malloc(size + 1);
            sprintf(ret, "%hi", item.b);
        break;
        default:
            printf("unitype_to_string_truncated_doubles: type not recognized\n");
            return NULL;
        break;
    }
    return ret;
}

/* copies one list to another (duplicates strings or pointers) */
void list_copy(list_t *dest, list_t *src) {
    list_free_lite(dest);
    dest -> type = calloc(src -> realLength, sizeof(int));
    dest -> data = calloc(src -> realLength, sizeof(unitype));
    unsigned int len = src -> length;
    dest -> length = len;
    dest -> realLength = src -> realLength;
    for (int i = 0; i < len; i++) {
        dest -> type[i] = src -> type[i];
        if (src -> type[i] == 'r') {
            dest -> data[i] = (unitype) list_init();
            list_copy(dest -> data[i].r, src -> data[i].r);
        } else {
            if (src -> type[i] == 'p') {
                memcpy(dest -> data[i].p, src -> data[i].p, sizeof(unitype));
            } else {
                if (src -> type[i] == 's') {
                    dest -> data[i].s = strdup(src -> data[i].s);
                } else {
                    dest -> data[i] = src -> data[i];
                }
            }
        }
    }
}

void list_print(list_t *list) { // prints the list (like python would)
    printf("[");
    if (list -> length == 0) {
        printf("]\n");
        return;
    }
    for (int i = 0; i < list -> length; i++) {
        unitype_print(list -> data[i], list -> type[i]);
        if (i == list -> length - 1) {
            printf("]\n");
        } else {
            printf(", ");
        }
    }
}

void list_print_emb(list_t *list) { // prints the list but without closing \n
    printf("[");
    if (list -> length == 0) {
        printf("]");
        return;
    }
    for (int i = 0; i < list -> length; i++) {
        unitype_print(list -> data[i], list -> type[i]);
        if (i == list -> length - 1) {
            printf("]");
        } else {
            printf(", ");
        }
    }
}

void list_print_type(list_t *list) { // prints the types of the list
    printf("[");
    if (list -> length == 0) {
        printf("]\n");
        return;
    }
    for (int i = 0; i < list -> length; i++) {
        printf("%c", list -> type[i]);
        if (i == list -> length - 1) {
            printf("]\n");
        } else {
            printf(", ");
        }
    }
}

void list_free_lite(list_t *list) { // frees the list's data but not the list itself
    for (int i = 0; i < list -> length; i++) {
        if (list -> type[i] == 'r') {
            list_free(list -> data[i].r);
        }
        if (list -> type[i] == 's' || list -> type[i] == 'p') {
            free(list -> data[i].s);
        }
    }
    free(list -> type);
    free(list -> data);
}

void list_free(list_t *list) { // frees the data used by the list
    list_free_lite(list);
    free(list);
}