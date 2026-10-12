#include "postgres.h"
#include "fmgr.h"
#include "utils/memutils.h"
#include "utils/builtins.h"
#include <limits.h>
#include <stdlib.h>

PG_MODULE_MAGIC;

/* MAX_LEVEL = log_{1/p}(N) con p = 0.5: 20 niveles cubren ~10^6 elementos. */
#define MAX_LEVEL 20
#define PROBABILITY 0.5
#define SEED 42

typedef struct SkipListNode
{
    int32 key;
    int32 value;
    struct SkipListNode **forward;
} SkipListNode;

typedef struct SkipList
{
    int32 level;            /* nivel maximo actualmente en uso */
    int64 size;             /* numero de claves distintas almacenadas */
    SkipListNode *header;
} SkipList;

static SkipList *global_skiplist = NULL;
static MemoryContext skiplist_ctx = NULL;   /* contexto propio, hijo de TopMemoryContext */

PG_FUNCTION_INFO_V1(pg_skiplist_init);
PG_FUNCTION_INFO_V1(pg_skiplist_insert);
PG_FUNCTION_INFO_V1(pg_skiplist_search);
PG_FUNCTION_INFO_V1(pg_skiplist_count);
PG_FUNCTION_INFO_V1(pg_skiplist_check);

/* Se llama con CurrentMemoryContext = skiplist_ctx */
static SkipListNode *
create_node(int32 key, int32 value, int level)
{
    SkipListNode *node = (SkipListNode *) palloc(sizeof(SkipListNode));

    node->key = key;
    node->value = value;
    node->forward = (SkipListNode **) palloc0(sizeof(SkipListNode *) * (level + 1));
    return node;
}

static int
random_level(void)
{
    int lvl = 0;

    while (((double) rand() / RAND_MAX) < PROBABILITY && lvl < MAX_LEVEL)
        lvl++;
    return lvl;
}

Datum
pg_skiplist_init(PG_FUNCTION_ARGS)
{
    MemoryContext old;

    /* Reinicializar libera toda la estructura anterior (sin fugas). */
    if (skiplist_ctx != NULL)
        MemoryContextDelete(skiplist_ctx);

    skiplist_ctx = AllocSetContextCreate(TopMemoryContext,
                                         "SkipListContext",
                                         ALLOCSET_DEFAULT_SIZES);
    old = MemoryContextSwitchTo(skiplist_ctx);

    global_skiplist = (SkipList *) palloc(sizeof(SkipList));
    global_skiplist->level = 0;
    global_skiplist->size = 0;
    global_skiplist->header = create_node(INT_MIN, 0, MAX_LEVEL);

    MemoryContextSwitchTo(old);
    srand(SEED);            /* secuencia de niveles reproducible */

    PG_RETURN_TEXT_P(cstring_to_text("Skip List inicializada con exito"));
}

Datum
pg_skiplist_insert(PG_FUNCTION_ARGS)
{
    int32 key = PG_GETARG_INT32(0);
    int32 value = PG_GETARG_INT32(1);
    SkipListNode *update[MAX_LEVEL + 1];
    SkipListNode *x;
    int i, lvl;
    MemoryContext old;

    if (global_skiplist == NULL)
        ereport(ERROR, (errmsg("Ejecuta skiplist_init() primero.")));

    x = global_skiplist->header;
    for (i = global_skiplist->level; i >= 0; i--)
    {
        while (x->forward[i] != NULL && x->forward[i]->key < key)
            x = x->forward[i];
        update[i] = x;
    }
    x = x->forward[0];

    if (x != NULL && x->key == key)
    {
        x->value = value;           /* clave duplicada: reemplaza el valor */
        PG_RETURN_BOOL(true);
    }

    lvl = random_level();
    if (lvl > global_skiplist->level)
    {
        for (i = global_skiplist->level + 1; i <= lvl; i++)
            update[i] = global_skiplist->header;
        global_skiplist->level = lvl;
    }

    old = MemoryContextSwitchTo(skiplist_ctx);
    x = create_node(key, value, lvl);
    MemoryContextSwitchTo(old);

    for (i = 0; i <= lvl; i++)
    {
        x->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = x;
    }
    global_skiplist->size++;
    PG_RETURN_BOOL(true);
}

Datum
pg_skiplist_search(PG_FUNCTION_ARGS)
{
    int32 key = PG_GETARG_INT32(0);
    SkipListNode *x;
    int i;

    if (global_skiplist == NULL)
        ereport(ERROR, (errmsg("La Skip List no esta inicializada.")));

    x = global_skiplist->header;
    for (i = global_skiplist->level; i >= 0; i--)
        while (x->forward[i] != NULL && x->forward[i]->key < key)
            x = x->forward[i];
    x = x->forward[0];

    if (x != NULL && x->key == key)
        PG_RETURN_INT32(x->value);
    PG_RETURN_NULL();
}

/* Numero de claves distintas almacenadas. */
Datum
pg_skiplist_count(PG_FUNCTION_ARGS)
{
    if (global_skiplist == NULL)
        ereport(ERROR, (errmsg("La Skip List no esta inicializada.")));
    PG_RETURN_INT64(global_skiplist->size);
}

/*
 * Verifica invariantes: el nivel 0 esta estrictamente ordenado, su longitud
 * coincide con size, y cada nivel superior es subsecuencia ordenada del anterior.
 */
Datum
pg_skiplist_check(PG_FUNCTION_ARGS)
{
    SkipListNode *x;
    int64 n = 0;
    int i;

    if (global_skiplist == NULL)
        ereport(ERROR, (errmsg("La Skip List no esta inicializada.")));

    for (x = global_skiplist->header->forward[0]; x != NULL; x = x->forward[0])
    {
        if (x->forward[0] != NULL && x->key >= x->forward[0]->key)
            PG_RETURN_BOOL(false);
        n++;
    }
    if (n != global_skiplist->size)
        PG_RETURN_BOOL(false);

    for (i = 1; i <= global_skiplist->level; i++)
        for (x = global_skiplist->header->forward[i]; x != NULL; x = x->forward[i])
            if (x->forward[i] != NULL && x->key >= x->forward[i]->key)
                PG_RETURN_BOOL(false);

    PG_RETURN_BOOL(true);
}
