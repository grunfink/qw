/* qw - A minimalistic text editor by grunfink - public domain */

#include "qw.h"

#include <stdlib.h>
#include <string.h>


/** code **/

qw_block *qw_block_new(qw_block *prev, qw_block *next)
/* allocate a new block or resize one */
{
    qw_block *b = malloc(sizeof(qw_block));

    b->prev = prev;
    b->next = next;
    b->used = 0;

    if (b->prev)
        b->prev->next = b;
    if (b->next)
        b->next->prev = b;

    return b;
}


qw_block *qw_block_destroy(qw_block *b)
/* destroy this block and all the chain after it */
{
    if (b != NULL) {
        qw_block_destroy(b->next);
        free(b);
    }

    return NULL;
}


qw_block *qw_block_insert_str(qw_block *b, int pos, const char *str, int size)
/* inserts a string into pos, updating the chain */
{
    /* insert position at the end of the block? */
    if (pos == b->used) {
        int free = QW_BLOCK_SIZE - b->used;

        /* does it fit? */
        if (size < free) {
            /* just copy and account it */
            memcpy(&b->data[b->used], str, size);
            b->used += size;
        }
        else {
            /* copy what fits */
            memcpy(&b->data[b->used], str, free);
            b->used += free;

            /* create a new block and keep inserting there */
            qw_block_insert_str(qw_block_new(b, b->next), 0, str + free, size - free);
        }
    }
    else
    /* insert position at the beginning of the block? */
    if (pos == 0) {
        /* insert into a new block before this one */
        b = qw_block_insert_str(qw_block_new(b->prev, b), 0, str, size);
    }
    else {
        /* insert position in between */

        /* create a new block and store there the second half */
        qw_block_insert_str(qw_block_new(b, b->next), 0, &b->data[pos], b->used - pos);

        /* truncate size and retry */
        b->used = pos;
        qw_block_insert_str(b, pos, str, size);
    }

    return b;
}


void qw_block_delete(qw_block *b, int pos, int size)
/* delete size chars */
{
    if (b != NULL && size > 0) {
        int rmndr = b->used - pos;

        if (rmndr > size) {
            /* collapse data */
            memcpy(&b->data[pos], &b->data[pos + size], rmndr - size);

            /* truncate used size */
            b->used = pos + rmndr - size;
        }
        else {
            /* truncate used size */
            b->used = pos;

            /* delete the rest in the next block */
            qw_block_delete(b->next, 0, size - rmndr);
        }
    }
}


qw_block *qw_block_first(qw_block *b)
/* returns the first in the block chain */
{
    return b && b->prev ? qw_block_first(b->prev) : b;
}


qw_block *qw_block_last(qw_block *b)
/* returns the last in the block chain */
{
    return b && b->next ? qw_block_last(b->next) : b;
}


int qw_block_get_str(qw_block *b, int pos, char *buf, int size)
/* gets a string from the blocks into buf, returns nr. of copied bytes */
{
    int r = 0;

    if (b != NULL && size) {
        r = b->used - pos;

        /* do not copy more than there is */
        if (r > size)
            r = size;

        memcpy(buf, &b->data[pos], r);

        /* continue copying from the next block */
        r += qw_block_get_str(b->next, 0, buf + r, size - r);
    }

    return r;
}


qw_block *qw_block_move(qw_block *b, int pos, int *npos, int inc)
/* moves from pos an inc number of bytes, returns new block and new position */
{
    if (b != NULL) {
        int r = (pos == -1 ? b->used : pos) + inc;

        /* is it inside this block? */
        if (r >= 0 && r < b->used) {
            /* return position */
            *npos = r;
        }
        else
        if (inc < 0) {
            /* move back */
            b = qw_block_move(b->prev, -1, npos, r);
        }
        else {
            /* at EOF? allow being outside the block */
            if (b->next == NULL && r == b->used)
                *npos = r;
            else
            /* move next */
            b = qw_block_move(b->next, 0, npos, r - b->used);
        }
    }

    return b;
}


int qw_block_rel_to_abs(qw_block *b, int rpos)
/* returns the absolute position from a relative one */
{
    return b && b->prev ? rpos + qw_block_rel_to_abs(b->prev, b->prev->used) : rpos;
}


qw_block *qw_block_abs_to_rel(qw_block *b, int apos, int *rpos)
/* moves from an absolute position to a relative one */
{
    return qw_block_move(qw_block_first(b), 0, rpos, apos);
}


qw_block *qw_block_here(qw_block *b, int pos, const char *str, int size)
/* tests if a string is in the specified position */
{
    qw_block *r = NULL;

    if (b != NULL) {
        int n = b->used - pos;

        if (size <= n) {
            /* all the string fits in this block, compare directly */
            if (memcmp(&b->data[pos], str, size) == 0)
                r = b;
        }
        else {
            /* compare the bytes available and if found try next block */
            if (memcmp(&b->data[pos], str, n) == 0)
                r = qw_block_here(b->next, 0, str + n, size - n);
        }
    }

    return r;
}


qw_block *qw_block_search(qw_block *b, int *pos, const char *str, int size, int inc)
/* search for a string in the chain of blocks */
{
    qw_block *r = NULL;

    while (b != NULL && (r = qw_block_here(b, *pos, str, size)) == NULL)
        b = qw_block_move(b, *pos, pos, inc);

    return r;
}


qw_block *qw_block_move_bol(qw_block *b, int *pos)
/* move to the beginning of the line */
{
    qw_block *r = b;

    /* if it's over an end of line, move backwards */
    if (r->data[*pos] == '\n')
        r = qw_block_move(r, *pos, pos, -1);

    if ((r = qw_block_search(r, pos, "\n", 1, -1)) != NULL) {
        /* found; move 1 char forward */
        r = qw_block_move(r, *pos, pos, 1);
    }
    else {
        /* not found; move to BOF */
        r = qw_block_first(b);
        *pos = 0;
    }

    return r;
}


qw_block *qw_block_move_eol(qw_block *b, int *pos)
/* move to the end of the line */
{
    qw_block *r;

    if ((r = qw_block_search(b, pos, "\n", 1, 1)) == NULL) {
        /* not found; move to EOF */
        r = qw_block_last(b);
        *pos = r->used;
    }

    return r;
}


qw_block *qw_block_insert_str_and_move(qw_block *b, int *pos, const char *str, int size)
/* insert and string and move forward */
{
    b = qw_block_insert_str(b, *pos, str, size);
    return qw_block_move(b, *pos, pos, size);
}


void qw_block_dump(qw_block *b, FILE *f)
/* dumps information on a chain of blocks */
{
    int n_blocks = 0;
    int t_size = 0;
    int t_used = 0;

    while (b) {
        int n;

        fprintf(f, "addr: %p\n", b);
        fprintf(f, "prev: %p\n", b->prev);
        fprintf(f, "next: %p\n", b->next);
        fprintf(f, "used: %d\n", b->used);
        fprintf(f, "data:\n");

        for (n = 0; n < b->used; n++) {
            if (b->data[n] == '\n')
                fprintf(f, "\\n");
            else
            if (b->data[n] == '\r')
                fprintf(f, "\\r");
            else
                fputc(b->data[n], f);
        }

        fprintf(f, "\n");

        n_blocks++;
        t_used += b->used;

        b = b->next;

        if (b)
            fprintf(f, "----------------------------\n");
    }

    t_size = n_blocks * QW_BLOCK_SIZE;

    fprintf(f, "\nblocks: %d\n", n_blocks);
    fprintf(f, "memory: %d / %d (%d%%)\n\n", t_used, t_size, t_used * 100 / t_size);
}
