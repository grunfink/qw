/* qw - A minimalistic text editor by grunfink - public domain */

#include "qw.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/** code **/

qw_block *qw_file_load(const char *fname, int *crlf)
/* loads a file. Returns the loaded data and if it has CR/LF line endings */
{
    FILE *f;
    qw_block *b = NULL;

    if ((f = fopen(fname, "rb")) != NULL) {
        char line[1024];
        int i = 0;

        /* normal EOL by default */
        *crlf = 0;

        b = qw_block_new(NULL, NULL);

        while ((fgets(line, sizeof(line), f)) != NULL) {
            int sz = strlen(line);

            /* does it have a CR? chop it */
            if (sz >= 2 && line[sz - 2] == '\r') {
                line[sz - 2] = '\n';
                sz--;

                *crlf = 1;
            }

            b = qw_block_insert_str_and_move(b, &i, line, sz);
        }

        fclose(f);
    }

    return b;
}


int qw_file_save(qw_block *b, const char *fname, int crlf)
/* saves a file. Returns -1 on errors */
{
    FILE *f;
    int ret = 0;

    if ((f = fopen(fname, "wb")) != NULL) {
        int i = 0;
        char c;

        /* move to the beginning */
        b = qw_block_first(b);

        while (b != NULL && qw_block_get_str(b, i, &c, 1)) {
            /* write CR if appropriate */
            if (crlf && c == '\n')
                fputc('\r', f);

            fputc(c, f);
            b = qw_block_move(b, i, &i, 1);
        }

        fclose(f);
    }
    else
        ret = -1;

    return ret;
}


qw_doc *qw_doc_new(qw_doc *d, const char *fname)
/* creates a new document */
{
    qw_doc *doc;

    doc = calloc(1, sizeof(qw_doc));

    if (fname != NULL) {
        doc->fname = strdup(fname);

        if ((doc->b = qw_file_load(fname, &doc->crlf)) == NULL)
            doc->new_file = 1;
    }

    if (doc->b == NULL)
        doc->b = qw_block_new(NULL, NULL);

    /* no selection mark */
    doc->mark_s = doc->mark_e = -1;

    /* first (dummy) journal entry */
    doc->j = qw_journal_new(0, doc->b, 0, NULL, 0, NULL);

    /* assume it as clean */
    qw_journal_mark_clean(doc->j);

    /* enqueue in a circular buffer */
    if (d == NULL)
        doc->prev = doc->next = doc;
    else {
        doc->prev = d;
        doc->next = d->next;

        doc->prev->next = doc;
        doc->next->prev = doc;
    }

    return doc;
}


qw_doc *qw_doc_destroy(qw_doc *doc)
/* destroy a document and all related data */
{
    qw_doc *next;

    /* destroy everything */
    free(doc->fname);
    qw_block_destroy(qw_block_first(doc->b));
    qw_journal_destroy(qw_journal_first(doc->j));

    /* if next if this doc, then it's the only one in the chain */
    if (doc->next == doc)
        next = NULL;
    else {
        doc->next->prev = doc->prev;
        doc->prev->next = doc->next;
        next = doc->next;
    }

    free(doc);

    return next;
}


void qw_doc_dump(qw_doc *d, FILE *f)
/* dumps information of a document */
{
    fprintf(f, "addr: %p\n", d);
    fprintf(f, "prev: %p\n", d->prev);
    fprintf(f, "next: %p\n", d->next);
    fprintf(f, "name: %s\n", d->fname);
    fprintf(f, "vpos: %d\n", d->vpos);
    fprintf(f, "cpos: %d\n", d->cpos);
    fprintf(f, "crlf: %d\n", d->crlf);
    fprintf(f, "blocks:\n\n");

    qw_block_dump(d->b, f);
}
