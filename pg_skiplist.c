#include "postgres.h"
#include "fmgr.h"
#include "utils/memutils.h"
#include "utils/builtins.h"
#include <limits.h>

PG_MODULE_MAGIC;

#define MAX_LEVEL 16
#define PROBABILITY 0.5

typedef struct SkipListNode {
    int32 key;
    int32 value;
    struct SkipListNode **forward;
} SkipListNode;

typedef struct SkipList {
    int32 level;
    SkipListNode *header;
} SkipList;

static SkipList *global_skiplist = NULL;

static SkipListNode* create_node(int32 key, int32 value, int32 level) {
    MemoryContext oldcontext;
    SkipListNode *n;
    int i;

    oldcontext = MemoryContextSwitchTo(TopMemoryContext);
    n = (SkipListNode *) palloc(sizeof(SkipListNode));
    n->key = key;
    n->value = value;
    n->forward = (SkipListNode **) palloc(sizeof(SkipListNode*) * (level + 1));
    for (i = 0; i <= level; i++) {
        n->forward[i] = NULL;
    }
    MemoryContextSwitchTo(oldcontext);
    return n;
}

static int random_level(void) {
    int lvl = 0;
    while (((double)rand() / RAND_MAX) < PROBABILITY && lvl < MAX_LEVEL) {
        lvl++;
    }
    return lvl;
}

PG_FUNCTION_INFO_V1(pg_skiplist_init);
Datum
pg_skiplist_init(PG_FUNCTION_ARGS)
{
    MemoryContext oldcontext;

    oldcontext = MemoryContextSwitchTo(TopMemoryContext);
    global_skiplist = (SkipList *) palloc(sizeof(SkipList));
    global_skiplist->level = 0;
    global_skiplist->header = create_node(INT_MIN, 0, MAX_LEVEL);
    MemoryContextSwitchTo(oldcontext);

    PG_RETURN_DATUM(CStringGetTextDatum("Skip List inicializada con exito"));
}

PG_FUNCTION_INFO_V1(pg_skiplist_insert);
Datum
pg_skiplist_insert(PG_FUNCTION_ARGS)
{
    int32 key;
    int32 value;
    int i;
    int rlevel;
    SkipListNode *current;
    SkipListNode *new_node;
    SkipListNode *update[MAX_LEVEL + 1];

    key = PG_GETARG_INT32(0);
    value = PG_GETARG_INT32(1);

    if (global_skiplist == NULL) {
        ereport(ERROR, (errmsg("Ejecuta skiplist_init() primero.")));
    }

    current = global_skiplist->header;

    for (i = global_skiplist->level; i >= 0; i--) {
        while (current->forward[i] != NULL && current->forward[i]->key < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    current = current->forward[0];

    if (current != NULL && current->key == key) {
        current->value = value;
        PG_RETURN_BOOL(true);
    }

    rlevel = random_level();
    if (rlevel > global_skiplist->level) {
        for (i = global_skiplist->level + 1; i <= rlevel; i++) {
            update[i] = global_skiplist->header;
        }
        global_skiplist->level = rlevel;
    }

    new_node = create_node(key, value, rlevel);
    for (i = 0; i <= rlevel; i++) {
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }

    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(pg_skiplist_search);
Datum
pg_skiplist_search(PG_FUNCTION_ARGS)
{
    int32 key;
    int i;
    SkipListNode *current;

    key = PG_GETARG_INT32(0);

    if (global_skiplist == NULL) {
        ereport(ERROR, (errmsg("La Skip List no esta inicializada.")));
    }

    current = global_skiplist->header;
    for (i = global_skiplist->level; i >= 0; i--) {
        while (current->forward[i] != NULL && current->forward[i]->key < key) {
            current = current->forward[i];
        }
    }
    current = current->forward[0];

    if (current != NULL && current->key == key) {
        PG_RETURN_INT32(current->value);
    }

    PG_RETURN_NULL();
}