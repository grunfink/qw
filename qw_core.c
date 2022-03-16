/* qw - A minimalistic text editor by grunfink - public domain */

#include "qw.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/** code **/

qw_core *qw_core_new(void)
/* create a new core structure */
{
    qw_core *core;

    core = calloc(1, sizeof(qw_core));

    core->running = 1;  /* it's running */
    core->refresh = 1;  /* it will need a refresh ASAP */

    return core;
}


static int add_char_to_view(qw_view *view, int vsz, char chr, qw_attr attr)
/* adds a character and its attribute to a view */
{
    /* realloc if needed */
    if (view->size >= vsz) {
        vsz += 32;
        view->data = realloc(view->data, vsz);
        view->attr = realloc(view->attr, vsz);
    }

    view->data[view->size] = chr;
    view->attr[view->size] = (char) attr;
    view->size++;

    return vsz;
}


void qw_view_mark_matching(qw_view *view, int cpos)
/* marks the matching character */
{
    char *br_open  = "({[";
    char *br_close = ")}]";
    char *p1, *p2;
    int d = 0;

    if ((p1 = strchr(br_open, view->data[cpos]))) {
        /* find matching forward */
        p2 = br_close + (p1 - br_open);
        d = 1;
    }
    else
    if ((p1 = strchr(br_close, view->data[cpos]))) {
        /* find matching backwards */
        p2 = br_open + (p1 - br_close);
        d = -1;
    }

    if (d != 0) {
        int i, c = 0;

        for (i = cpos; i >= 0 && i < view->size; i += d) {
            if (view->data[i] == *p1)
                c++;
            else
            if (view->data[i] == *p2) {
                if (--c == 0)
                    break;
            }
        }

        if (c == 0)
            view->attr[i] = QW_ATTR_MATCHING;
    }
}


void qw_core_create_view(qw_core *core, int *cx, int *cy)
/* creates a view for the current document */
{
    int vsz = 0;
    qw_doc *doc   = core->docs;
    qw_view *view = &core->view;
    qw_block *b;
    int h, i, vpos, cpos = -1;
    int ms = -1, me = -1;

    /* fix vpos */
    doc->vpos = qw_view_fix_vpos(doc->b,
        doc->vpos, doc->cpos, core->width, core->height);

    /* clean previous view */
    free(view->data);
    free(view->attr);
    memset(view, '\0', sizeof(qw_view));

    /* convert the vpos to relative */
    b = qw_block_abs_to_rel(doc->b, doc->vpos, &i);

    vpos = doc->vpos;

    for (h = 0; b != NULL && h < core->height; h++) {
        char buf[4096]; /* should be enough to fit a row */
        int rsz, n;

        memset(buf, ' ', sizeof(buf));

        /* get the row size */
        rsz = qw_view_row_size(b, i, core->width);

        /* read the full row */
        qw_block_get_str(b, i, buf, rsz);

        /* transfer bytes into the view */
        for (n = 0; n < rsz; n++) {
            /* is there a selection mark? */
            if (doc->mark_s != -1 && doc->mark_e != -1) {
                /* if no mark start is set and we're inside it, mark the start */
                if (ms == -1 && vpos + n >= doc->mark_s && vpos + n < doc->mark_e)
                    ms = view->size;

                /* if we're outside the selection, mark the end */
                if (vpos + n < doc->mark_e)
                    me = view->size;
            }

            /* store the offset of the cursor inside the view */
            if (vpos + n == doc->cpos)
                cpos = view->size;

            /* FIXME: if it's a tab, change it to some thing less problematic */
            if (buf[n] == '\t')
                buf[n] = '_';

            vsz = add_char_to_view(view, vsz, buf[n], QW_ATTR_NORMAL);
        }

        /* if it wasn't a real EOL, add a soft wordwrap */
        if (rsz > 0 && buf[rsz - 1] != '\n')
            vsz = add_char_to_view(view, vsz, '\r', QW_ATTR_NORMAL);

        /* if cpos is inside this line, fill cursor position */
        if (doc->cpos >= vpos && doc->cpos <= vpos + rsz) {
            /* get y position */
            *cy = h;

            /* get x position by calculating the width of the start of the line */
            *cx = qw_utf8_str_width(buf, doc->cpos - vpos);
        }

        /* advance cursor */
        b = qw_block_move(b, i, &i, rsz);

        /* move the vpos forward */
        vpos += rsz;
    }

    /* insert an ASCIIZ to allow string searches, but don't account it */
    vsz = add_char_to_view(view, vsz, '\0', QW_ATTR_NONE);
    view->size--;

    /* change the attribute of the matching character */
    if (cpos != -1)
        qw_view_mark_matching(view, cpos);

    /* apply syntax highlight */
    qw_synhi_apply_to_view(view, doc->sh);

    /* mark the selection, if any */
    if (ms != -1) {
        /* if no mark end is set, it's beyond the end of the view */
        if (me == -1)
            me = view->size;

        /* set the mark attribute to all this range */
        for (; ms <= me; ms++)
            view->attr[ms] = QW_ATTR_MARK;
    }
}


/** operations **/

static void op_nop(qw_core *core)
/* do nothing */
{
}


static void op_left(qw_core *core)
/* moves the cursor to the left */
{
    qw_doc *doc = core->docs;
    qw_block *b;
    int i;

    /* get relative */
    b = qw_block_abs_to_rel(doc->b, doc->cpos, &i);

    /* move */
    b = qw_utf8_move(b, &i, -1);

    /* store absolute */
    if (b != NULL) {
        doc->b    = b;
        doc->cpos = qw_block_rel_to_abs(b, i);
    }
}


static void op_right(qw_core *core)
/* moves the cursor to the right */
{
    qw_doc *doc = core->docs;
    qw_block *b;
    int i;

    /* get relative */
    b = qw_block_abs_to_rel(doc->b, doc->cpos, &i);

    /* move */
    b = qw_utf8_move(b, &i, 1);

    /* store absolute */
    if (b != NULL) {
        doc->b    = b;
        doc->cpos = qw_block_rel_to_abs(b, i);
    }
}


static void op_up(qw_core *core)
/* moves the cursor up */
{
    qw_doc *doc = core->docs;
    int size, col, ac0;
    qw_block *b;
    int i;

    /* get col #0 position */
    ac0 = qw_view_get_col_0(doc->b, core->docs->cpos, core->width, &size);

    /* get column */
    col = qw_view_width_diff(doc->b, ac0, core->docs->cpos);

    b = qw_block_abs_to_rel(doc->b, ac0, &i);

    /* move back one char (to the end of previous line) */
    if ((b = qw_block_move(b, i, &i, -1)) != NULL) {
        /* get this new col #0 position */
        ac0 = qw_view_get_col_0(b, qw_block_rel_to_abs(b, i), core->width, &size);

        /* skip to col */
        core->docs->cpos = qw_view_set_col(doc->b, ac0, col, core->width);
    }
}


static void op_down(qw_core *core)
/* moves the cursor down */
{
    qw_doc *doc = core->docs;
    int size, col, ac0;
    qw_block *b;
    int i;

    /* get col #0 position */
    ac0 = qw_view_get_col_0(doc->b, core->docs->cpos, core->width, &size);

    /* get column */
    col = qw_view_width_diff(doc->b, ac0, core->docs->cpos);

    b = qw_block_abs_to_rel(doc->b, ac0, &i);

    if ((b = qw_block_move(b, i, &i, size)) != NULL) {
        /* skip to col */
        core->docs->cpos = qw_view_set_col(doc->b, qw_block_rel_to_abs(b, i), col, core->width);
    }
}


static void op_pgup(qw_core *core)
/* moves the cursor a page up */
{
    int n;

    for (n = 0; n < core->height - 1; n++)
        op_up(core);
}


static void op_pgdn(qw_core *core)
/* moves the cursor a page up */
{
    int n;

    for (n = 0; n < core->height - 1; n++)
        op_down(core);
}


static void op_bol(qw_core *core)
/* moves the cursor to the beginning of the line (row) */
{
    qw_doc *doc = core->docs;
    int size;

    /* move to column #0 */
    doc->cpos = qw_view_get_col_0(doc->b, doc->cpos, core->width, &size);
}


static void op_eol(qw_core *core)
/* moves the cursor to the end of the line (row) */
{
    qw_doc *doc = core->docs;
    int size;

    /* move to column #0 */
    doc->cpos = qw_view_get_col_0(doc->b, doc->cpos, core->width, &size);

    /* then skip to one byte less than the length */
    doc->cpos += size - 1;
}


static void op_bof(qw_core *core)
/* moves the cursor to the beginning of the file */
{
    core->docs->cpos = 0;
}


static void op_eof(qw_core *core)
/* moves the cursor to the end of the file */
{
    qw_block *b;

    /* get last block */
    b = qw_block_last(core->docs->b);

    /* store the last position */
    core->docs->cpos = qw_block_rel_to_abs(b, b->used);
}


static void op_unmark(qw_core *core)
/* unmarks the selection block */
{
    core->docs->mark_s = core->docs->mark_e = -1;
}


static void op_del_mark(qw_core *core)
/* delete selected mark */
{
    qw_doc *doc = core->docs;
    qw_block *b;
    int i;

    if (doc->mark_s != -1 && doc->mark_e != -1) {
        /* get relative */
        b = qw_block_abs_to_rel(doc->b, doc->mark_s, &i);

        /* create a journal entry and apply it */
        doc->j = qw_journal_new(0, b, i, NULL, doc->mark_e - doc->mark_s, doc->j);
        b = qw_journal_apply(b, doc->j, 1);

        /* set the cursor to the previous start of the block */
        if (b != NULL) {
            doc->b    = b;
            doc->cpos = qw_block_rel_to_abs(b, i);
        }

        /* move the cursor to where the selection started */
        doc->cpos = doc->mark_s;

        op_unmark(core);
    }
}


static void op_char(qw_core *core)
/* inserts a string */
{
    qw_doc *doc = core->docs;
    qw_block *b;
    int i;

    /* deletes the mark, if there is one */
    op_del_mark(core);

    /* get relative */
    b = qw_block_abs_to_rel(doc->b, doc->cpos, &i);

    /* create a journal entry, apply it and move */
    doc->j = qw_journal_new(1, b, i, core->payload, core->pl_size, doc->j);
    b = qw_journal_apply(b, doc->j, 1);
    b = qw_block_move(b, i, &i, doc->j->size);

    free(core->payload);
    core->payload = NULL;

    /* store absolute */
    if (b != NULL) {
        doc->b    = b;
        doc->cpos = qw_block_rel_to_abs(b, i);
    }
}


static void op_newline(qw_core *core)
/* inserts a new line */
{
    core->payload = strdup("\n");
    core->pl_size = 1;

    op_char(core);
}


static void op_tab(qw_core *core)
/* inserts a tab */
{
    qw_doc *doc = core->docs;
    int size, col, ac0;

    /* get col #0 position */
    ac0 = qw_view_get_col_0(doc->b, core->docs->cpos, core->width, &size);

    /* get column */
    col = qw_view_width_diff(doc->b, ac0, core->docs->cpos);

    /* create a set of spaces according to the column */
    core->pl_size = core->tab_size - (col % core->tab_size);
    core->payload = malloc(core->pl_size);
    memset(core->payload, ' ', core->pl_size);

    op_char(core);
}


static void op_hard_tab(qw_core *core)
/* inserts a hard tab */
{
    core->payload = strdup("\t");
    core->pl_size = 1;

    op_char(core);
}


static void op_del(qw_core *core)
/* deletes a char over the cursor */
{
    qw_doc *doc = core->docs;

    if (doc->mark_s != -1 && doc->mark_e != -1) {
        /* if there is a mark, delete it */
        op_del_mark(core);
    }
    else {
        qw_block *b;
        int i;

        /* get relative */
        b = qw_block_abs_to_rel(doc->b, doc->cpos, &i);

        /* create a journal entry and apply it */
        doc->j = qw_journal_new(0, b, i, NULL, 1, doc->j);
        b = qw_journal_apply(b, doc->j, 1);

        /* store absolute */
        if (b != NULL) {
            doc->b    = b;
            doc->cpos = qw_block_rel_to_abs(b, i);
        }
    }
}


static void op_backspace(qw_core *core)
/* deletes a char to the left of the cursor */
{
    op_left(core);
    op_del(core);
}


static void op_undo(qw_core *core)
/* undoes a modification */
{
    qw_doc *doc = core->docs;

    if (doc->j->prev) {
        doc->b    = qw_journal_apply(doc->b, doc->j, 0);
        doc->cpos = doc->j->apos;
        doc->j    = doc->j->prev;
    }
}


static void op_redo(qw_core *core)
/* re-does a modification */
{
    qw_doc *doc = core->docs;

    if (doc->j->next) {
        doc->j    = doc->j->next;
        doc->b    = qw_journal_apply(doc->b, doc->j, 1);
        doc->cpos = doc->j->apos;
    }
}


static void op_del_row(qw_core *core)
/* deletes the row over the cursor */
{
    qw_doc *doc = core->docs;
    qw_block *b;
    int ac0, size, i;

    /* move to column #0 */
    ac0 = qw_view_get_col_0(doc->b, doc->cpos, core->width, &size);

    /* get relative */
    b = qw_block_abs_to_rel(doc->b, ac0, &i);

    /* create a journal entry and apply it */
    doc->j = qw_journal_new(0, b, i, NULL, size, doc->j);
    b = qw_journal_apply(b, doc->j, 1);

    /* store absolute */
    if (b != NULL) {
        doc->b    = b;
        doc->cpos = qw_block_rel_to_abs(b, i);
    }
}


static void op_mark(qw_core *core)
/* marks the beginning or end of the selection block */
{
    qw_doc *doc = core->docs;

    if (doc->mark_s == -1)
        doc->mark_s = doc->cpos;        /* mark start */
    else
    if (doc->mark_e == -1)
        doc->mark_e = doc->cpos;        /* mark end */
    else
    if (doc->cpos < doc->mark_s)
        doc->mark_s = doc->cpos;        /* extend mark before */
    else
    if (doc->cpos > doc->mark_e)
        doc->mark_e = doc->cpos;        /* extend mark after */

    if (doc->mark_s > doc->mark_e) {
        /* toggle both ends */
        int t = doc->mark_s;
        doc->mark_s = doc->mark_e;
        doc->mark_e = t;
    }
}


static void copy_or_cut(qw_core *core, int cut)
/* copies or cuts the selection */
{
    qw_doc *doc = core->docs;
    qw_block *b;
    int i;

    /* alloc space for the copied data */
    core->clip_size = doc->mark_e - doc->mark_s;
    core->clip_data = realloc(core->clip_data, core->clip_size);

    /* convert the mark start position to relative */
    b = qw_block_abs_to_rel(doc->b, doc->mark_s, &i);

    /* get the content */
    qw_block_get_str(b, i, core->clip_data, core->clip_size);

    if (cut)
        op_del_mark(core);

    op_unmark(core);
}


static void op_copy(qw_core *core)
/* copies the selected block to the clipboard */
{
    copy_or_cut(core, 0);
}


static void op_cut(qw_core *core)
/* cut the selected block to the clipboard */
{
    copy_or_cut(core, 1);
}


static void op_paste(qw_core *core)
/* pastes the clipboard over the cursor */
{
    qw_doc *doc = core->docs;
    qw_block *b;
    int i;

    if (core->clip_size) {
        /* get relative */
        b = qw_block_abs_to_rel(doc->b, doc->cpos, &i);

        /* create a journal entry, apply it and move */
        doc->j = qw_journal_new(1, b, i, core->clip_data, core->clip_size, doc->j);
        b = qw_journal_apply(b, doc->j, 1);
        b = qw_block_move(b, i, &i, doc->j->size);

        /* store absolute */
        if (b != NULL) {
            doc->b    = b;
            doc->cpos = qw_block_rel_to_abs(b, i);
        }
    }
}


static void op_next(qw_core *core)
/* select next open document */
{
    core->docs = core->docs->next;
}


static void op_open(qw_core *core)
/* opens a new document */
{
    char *fname;

    if ((fname = qw_drv_open_file(core, "Document to open:")) != NULL) {
        qw_core_doc_new(core, fname);
        free(fname);
    }
}


static void op_save(qw_core *core)
/* saves current document */
{
    if (core->docs->fname == NULL) {
        /* no name; ask for one */
        core->docs->fname = qw_drv_save_file(core, "Enter file name:");
    }

    if (core->docs->fname != NULL) {
        /* save to disk */
        qw_file_save(core->docs->b, core->docs->fname, core->docs->crlf);

        /* mark as clean */
        qw_journal_mark_clean(core->docs->j);

        /* no longer new */
        core->docs->new_file = 0;
    }
}


static void op_close(qw_core *core)
/* closes current document */
{
    int r = 1;

    if (!core->docs->j->clean) {
        /* unsaved? ask first */
        char *fname = core->docs->fname;
        char str[4096] = "'";

        if (fname == NULL)
            strcat(str, "<unnamed>");
        else
        if (strlen(fname) < core->width / 4)
            strcat(str, fname);
        else {
            /* file name too long; print only the last part */
            char *p = &fname[strlen(fname) - core->width / 4];
            strcat(str, "...");
            strcat(str, p);
        }

        strcat(str, "' has unsaved changes. Save?");

        r = qw_drv_confirm(core, str);

        if (r == 1)         /* confirm */
            op_save(core);
        else
        if (r == -1)        /* cancel */
            core->running = 1;
    }

    if (r != -1) {
        /* destroy document */
        core->docs = qw_doc_destroy(core->docs);

        /* no document? create an empty one */
        if (core->running && core->docs == NULL)
            qw_core_doc_new(core, NULL);
    }
}


static void op_quit(qw_core *core)
/* exits the editor */
{
    core->running = 0;

    while (core->running == 0 && core->docs != NULL)
        op_close(core);
}


static void op_search_next(qw_core *core)
/* searches the same string again */
{
    if (core->search != NULL) {
        qw_doc *doc = core->docs;
        qw_block *b;
        int i;

        b = qw_block_abs_to_rel(doc->b, doc->cpos, &i);

        if ((b = qw_block_search(b, &i, core->search, core->search_size, 1)) != NULL) {
            /* skip search result */
            b = qw_block_move(b, i, &i, core->search_size);

            doc->cpos = qw_block_rel_to_abs(b, i);
        }
        else
            qw_drv_alert(core, "Not found.");
    }
}


static void op_search(qw_core *core)
/* searches a string */
{
    char *str;

    if ((str = qw_drv_search(core, "Search:")) != NULL) {
        free(core->search);
        core->search      = str;
        core->search_size = strlen(str);

        op_search_next(core);
    }
}


static void op_m_dash(qw_core *core)
/* inserts an m-dash */
{
    core->payload = strdup("\xe2\x80\x94");     /* U+2014 */
    core->pl_size = 3;

    op_char(core);
}


static void op_conf_cmd(qw_core *core)
/* executes a configuration command */
{
    char *cmd;

    if ((cmd = qw_drv_readline(core, "Command:")) != NULL) {
        if (qw_conf_parse_cmd(core, cmd) == -1)
            qw_drv_alert(core, "Invalid command");

        free(cmd);
    }
}


/* array of function handlers indexed by op */
static void (*op2func[])(qw_core *) = {
#define X(oid, oname) op_##oname,
#include "qw_op.h"
#undef X
};


void qw_core_key(qw_core *core, qw_key key)
/* processes a key */
{
    qw_op op;

    /* map which operation is to be done */
    if ((op = core->keymap[key]) != QW_OP_NOP)
        op2func[op](core);

    /* refresh needed */
    core->refresh = 1;
}


void qw_core_dump(qw_core *core, FILE *f)
/* dumps information on a core */
{
    qw_doc *d = core->docs;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    fprintf(f, "%s\n", asctime(tm));

    do {
        qw_doc_dump(d, f);

        d = d->next;
    } while (d != core->docs);
}


char *qw_core_status_line(qw_core *core, char *buf, int max_size)
/* fills the buffer with the status line */
{
    if (core->docs != NULL)
        snprintf(buf, max_size, "%s%s%s - qw",
            core->docs->j->clean      ? "" : "*",
            core->docs->fname != NULL ? core->docs->fname : "<unnamed>",
            core->docs->new_file      ? " (new file)" : "");
    else
        strcpy(buf, "qw");

    return buf;
}


void qw_core_doc_new(qw_core *core, const char *fname)
/* opens a file */
{
    core->docs = qw_doc_new(core->docs, fname);

    if (fname != NULL) {
        if ((core->docs->sh = qw_synhi_find_by_extension(fname, core->shs)) == NULL)
            core->docs->sh = qw_synhi_find_by_signature(core->docs->b, core->shs);
    }
}
