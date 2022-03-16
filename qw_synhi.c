/* qw - A minimalistic text editor by grunfink - public domain */

#include "qw.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>


/** code **/

qw_synhi *qw_synhi_find_by_name(const char *name, qw_synhi *list)
/* finds a syntax highlight definition by name */
{
    while (list && strcmp(name, list->name) != 0)
        list = qw_synhi_find_by_name(name, list->next);

    return list;
}


qw_synhi *qw_synhi_new(const char *name, qw_synhi *next)
/* creates a new syntax highlight definition (avoiding repetitions) */
{
    qw_synhi *sh;

    if ((sh = qw_synhi_find_by_name(name, next)) == NULL) {
        sh = calloc(1, sizeof(qw_synhi));

        sh->next = next;
        sh->name = strdup(name);
    }

    return sh;
}


void qw_synhi_add_extension(qw_synhi *sh, const char *ext)
/* adds a file new extension */
{
    /* add space */
    int i = sh->n_extensions++;

    sh->extensions = realloc(sh->extensions, sizeof(char *) * sh->n_extensions);

    sh->extensions[i] = strdup(ext);
}


qw_synhi *qw_synhi_find_by_extension(const char *fname, qw_synhi *list)
/* finds a syntax highlight definition by filename extension */
{
    if (list) {
        int n;
        int s = strlen(fname);

        /* iterate all extensions */
        for (n = 0; n < list->n_extensions; n++) {
            int es = strlen(list->extensions[n]);

            if (s > es && strcmp(list->extensions[n], &fname[s - es]) == 0)
                goto found;
        }

        list = qw_synhi_find_by_extension(fname, list->next);
    }

found:
    return list;
}


void qw_synhi_add_signature(qw_synhi *sh, const char *signature)
/* adds a signature */
{
    /* add space */
    int i = sh->n_signatures++;

    sh->signatures = realloc(sh->signatures, sizeof(char *) * sh->n_signatures);

    sh->signatures[i] = strdup(signature);
}


qw_synhi *qw_synhi_find_by_signature(qw_block *b, qw_synhi *list)
/* searches the start of the block for a signature */
{
    char buf[1024];

    /* read the start of the file */
    qw_block_get_str(qw_block_first(b), 0, buf, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';

    while (list != NULL) {
        int n;

        for (n = 0; n < list->n_signatures; n++) {
            /* iterate all signatures until one is found */
            if (strstr(buf, list->signatures[n]) != NULL)
                goto found;
        }

        list = list->next;
    }

found:
    return list;
}


void qw_synhi_add_token(qw_synhi *sh, const char *token, qw_attr attr)
/* adds a new token */
{
    /* add space */
    int i = sh->n_tokens++;

    sh->tokens = realloc(sh->tokens, sizeof(qw_token) * sh->n_tokens);

    sh->tokens[i].token = strdup(token);
    sh->tokens[i].attr  = attr;

    /* not sorted */
    sh->sorted = 0;
}


void qw_synhi_add_section(qw_synhi *sh, const char *begin,
                            const char *end, const char *escaped, qw_attr attr)
/* adds a new section */
{
    /* add space */
    int i = sh->n_sections++;

    sh->sections = realloc(sh->sections, sizeof(qw_section) * sh->n_sections);

    sh->sections[i].begin   = strdup(begin);
    sh->sections[i].end     = strdup(end);
    sh->sections[i].escaped = escaped != NULL ? strdup(escaped) : NULL;
    sh->sections[i].attr    = attr;
}


static int token_compare(const void *p1, const void *p2)
{
    qw_token *t1 = (qw_token *)p1;
    qw_token *t2 = (qw_token *)p2;

    return strcmp(t1->token, t2->token);
}


void qw_synhi_optimize(qw_synhi *sh)
/* optimizes a syntax highlight structure for faster access */
{
    if (!sh->sorted) {
        /* sort the tokens */
        qsort(sh->tokens, sh->n_tokens, sizeof(qw_token), token_compare);
        sh->sorted = 1;
    }
}


qw_attr qw_synhi_find_token(qw_synhi *sh, const char *token)
/* finds the attribute of a token */
{
    qw_attr attr = QW_ATTR_NONE;
    qw_token s_token = { token, 0 };
    qw_token *t;

    qw_synhi_optimize(sh);

    if ((t = bsearch(&s_token, sh->tokens, sh->n_tokens,
            sizeof(qw_token), token_compare)) != NULL)
        attr = t->attr;

    return attr;
}


#define is_token(c) (isalnum(c) || (c) == '_')

static void apply_tokens(qw_view *view, qw_synhi *sh)
/* applies the synhi to tokens */
{
    char token[4096];
    int i = 0;

    while (i < view->size) {
        int s, ti = 0;
        qw_attr attr;

        /* skip to the start of a token */
        while (i < view->size && !is_token(view->data[i]))
            i++;

        /* store the token */
        s = i;
        while (i < view->size && is_token(view->data[i]))
            token[ti++] = view->data[i++];

        token[ti] = '\0';

        /* is the token a numeral? */
        if (isdigit(token[0]))
            strcpy(token, "0");

        if ((attr = qw_synhi_find_token(sh, token)) != QW_ATTR_NONE) {
            while (s < i)
                view->attr[s++] = attr;
        }
    }
}


#define is_blank(c) ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r')

static void apply_words(qw_view *view, qw_synhi *sh)
/* applies the synhi to words */
{
    char word[4096];
    int i = 0;

    while (i < view->size) {
        int s, ti = 0;
        qw_attr attr;

        /* skip blanks */
        while (i < view->size && is_blank(view->data[i]))
            i++;

        /* store the word up to next blank */
        s = i;
        while (i < view->size && !is_blank(view->data[i]))
            word[ti++] = view->data[i++];

        word[ti] = '\0';

        if ((attr = qw_synhi_find_token(sh, word)) != QW_ATTR_NONE) {
            while (s < i)
                view->attr[s++] = attr;
        }
    }
}


static void apply_sections(qw_view *view, qw_synhi *sh)
/* applies the synhi to sections */
{
    int n;

    for (n = 0; n < sh->n_sections; n++) {
        struct qw_section *sect = &sh->sections[n];
        int i = 0;
        char *begin;

        /* does the begin string start with a \n? treat specially as separated lines */
        if (sect->begin[0] == '\n') {
            while (view->data[i] != '\0') {
                /* skip blanks */
                while (view->data[i] == ' ')
                    i++;

                /* does this line start with the begin string (without the \n)? */
                if (strncmp(&view->data[i], &sect->begin[1], strlen(&sect->begin[1])) == 0) {
                    /* fill with the attr up to EOL */
                    while (view->data[i] != '\0' && view->data[i] != '\n')
                        view->attr[i++] = sect->attr;
                }
                else {
                    /* skip to next line */
                    while (view->data[i] != '\0' && view->data[i] != '\n')
                        i++;
                }

                /* if over an EOL, skip it */
                if (view->data[i] == '\n')
                    i++;
            }
        }
        else
        /* iterate section starts */
        while (view->data[i] != '\0' && (begin = strstr(&view->data[i], sect->begin))) {
            /* find the two possible end of sections */
            char *end     = strstr(begin + 1, sect->end);
            char *escaped = sect->escaped ? strstr(begin + 1, sect->escaped) : NULL;
            int e;

            /* if escaped has been found before the end, re-scan for the end */
            while (end != NULL && escaped != NULL && escaped < end) {
                end     = strstr(escaped + strlen(sect->escaped), sect->end);
                escaped = strstr(escaped + strlen(sect->escaped), sect->escaped);
            }

            /* no end of section found? do it to the end */
            if (end == NULL)
                end = view->data + view->size - 1;

            i = begin - view->data;
            e = end   - view->data + strlen(sect->end);

            /* not yet attributed? do it */
            if (view->attr[i] == QW_ATTR_NORMAL) {
                /* fill from begin to end */
                while (i < e)
                    view->attr[i++] = sect->attr;
            }
            else
                i += strlen(sect->begin);
        }
    }
}


void qw_synhi_apply_to_view(qw_view *view, qw_synhi *sh)
/* applies the synhi to the view */
{
    if (view != NULL && sh != NULL) {
        apply_tokens(view, sh);
        apply_words(view, sh);
        apply_sections(view, sh);
    }
}
