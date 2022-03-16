/* qw - A minimalistic text editor by grunfink - public domain */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "qw.h"


/** code **/

qw_journal *qw_journal_first(qw_journal *j)
/* finds the first in the chain */
{
    return j && j->prev ? qw_journal_first(j->prev) : j;
}


qw_journal *qw_journal_destroy(qw_journal *j)
/* destroys a journal entry and all the following */
{
    if (j) {
        qw_journal_destroy(j->next);
        free(j);
    }

    return NULL;
}


qw_journal *qw_journal_new(int op, qw_block *b, int pos,
                            const char *str, int size, qw_journal *prev)
/* adds an entry to the journal */
{
    qw_journal *j;

    j = calloc(1, sizeof(qw_journal) + size);

    j->prev = prev;
    j->next = NULL;
    j->op   = op;
    j->apos = qw_block_rel_to_abs(b, pos);
    j->size = size;

    if (op == 1)
        /* insert: store the data that will be inserted */
        memcpy(j->data, str, size);
    else
        /* delete: pick the data that will be deleted */
        qw_block_get_str(b, pos, j->data, size);

    if (prev) {
        qw_journal_destroy(prev->next);
        prev->next = j;
    }

    return j;
}


qw_block *qw_journal_apply(qw_block *b, qw_journal *j, int dir)
/* applies or unapplies a journal entry */
{
    int rpos;

    /* gets the block and relative position */
    b = qw_block_abs_to_rel(b, j->apos, &rpos);

    if (j->op == dir)
        b = qw_block_insert_str(b, rpos, j->data, j->size);
    else
        qw_block_delete(b, rpos, j->size);

    return b;
}


void qw_journal_mark_clean(qw_journal *j)
/* mark this entry as clean (i.e. saved to disk) */
{
    qw_journal *t;

    j->clean = 1;

    /* mark the rest of entries as dirty */
    for (t = j->prev; t; t = t->prev)
        t->clean = 0;
    for (t = j->next; t; t = t->next)
        t->clean = 0;
}
