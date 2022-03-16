/* qw - A minimalistic text editor by grunfink - public domain */

#ifndef QW_H
#define QW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>

#define QW_BLOCK_SIZE 4096

typedef struct qw_block qw_block;

struct qw_block {
    qw_block *prev;             /* previous block in chain */
    qw_block *next;             /* next block in chain */
    int used;                   /* number of used bytes */
    char data[QW_BLOCK_SIZE];   /* data block */
};

qw_block *qw_block_new(qw_block *prev, qw_block *next);
qw_block *qw_block_destroy(qw_block *b);
qw_block *qw_block_insert_str(qw_block *b, int pos, const char *str, int size);
qw_block *qw_block_insert_str_and_move(qw_block *b, int *pos, const char *str, int size);
void qw_block_delete(qw_block *b, int pos, int size);
qw_block *qw_block_first(qw_block *b);
qw_block *qw_block_last(qw_block *b);
int qw_block_get_str(qw_block *b, int pos, char *buf, int size);
qw_block *qw_block_move(qw_block *b, int pos, int *npos, int inc);
int qw_block_rel_to_abs(qw_block *b, int rpos);
qw_block *qw_block_abs_to_rel(qw_block *b, int apos, int *rpos);
qw_block *qw_block_here(qw_block *b, int pos, const char *str, int size);
qw_block *qw_block_search(qw_block *b, int *pos, const char *str, int size, int inc);
qw_block *qw_block_move_bol(qw_block *b, int *pos);
qw_block *qw_block_move_eol(qw_block *b, int *pos);
void qw_block_dump(qw_block *b, FILE *f);

typedef struct qw_journal qw_journal;

struct qw_journal {
    qw_journal *prev;   /* previous entry in journal */
    qw_journal *next;   /* next entry in journal */
    int op;             /* operation: 0, delete; 1, insert */
    int apos;           /* absolute position */
    int size;           /* size of data */
    int clean;          /* clean (saved to disk) at this point */
    char data[];        /* data block */
};

qw_journal *qw_journal_first(qw_journal *j);
qw_journal *qw_journal_destroy(qw_journal *j);
qw_journal *qw_journal_new(int op, qw_block *b, int pos,
                            const char *str, int size, qw_journal *prev);
qw_block *qw_journal_apply(qw_block *b, qw_journal *j, int dir);
void qw_journal_mark_clean(qw_journal *j);

qw_block *qw_utf8_move(qw_block *b, int *pos, int inc);
int qw_unicode_width(uint32_t cpoint);
int qw_utf8_get_char(qw_block *b, int pos, char *buf);
qw_block *qw_utf8_get_char_and_move(qw_block *b, int *pos, char *buf, int *size);
int qw_utf8_size(const char *buf);
uint32_t qw_utf8_decode(const char *buf, int size);
int qw_utf8_encode(uint32_t cpoint, char *buf);
int qw_utf8_str_width(const char *str, int size);

typedef struct qw_view qw_view;

struct qw_view {
    int size;       /* content size in data and attr */
    char *data;     /* data */
    char *attr;     /* attributes */
};

int qw_view_row_size(qw_block *b, int pos, int width);
int qw_view_get_col_0(qw_block *b, int apos, int width, int *size);
int qw_view_width_diff(qw_block *b, int apos0, int apos1);
int qw_view_set_col(qw_block *b, int ac0, int col, int width);
int qw_view_fix_vpos(qw_block *b, int vpos, int cpos, int wdth, int hght);

typedef enum {
#define X(attrid, attrname) attrid,
#include "qw_attr.h"
#undef X
    QW_ATTR_COUNT
} qw_attr;

int qw_attr_pack(qw_attr attr, char *uchr);
qw_attr qw_attr_unpack(const char *uchr, int size);

typedef struct qw_token qw_token;

struct qw_token {
    const char *token;      /* token */
    qw_attr attr;           /* attribute */
};

typedef struct qw_section qw_section;

struct qw_section {
    const char *begin;      /* beginning of section */
    const char *end;        /* end of section */
    const char *escaped;    /* escaped mark (can be NULL) */
    qw_attr attr;           /* attribute */
};

typedef struct qw_synhi qw_synhi;

struct qw_synhi {
    qw_synhi *next;             /* pointer to next */
    const char *name;           /* synhi name */
    int n_extensions;           /* number of extensions */
    const char **extensions;    /* extensions */
    int n_signatures;           /* number of signatures */
    const char **signatures;    /* signatures */
    int n_tokens;               /* number of tokens */
    qw_token *tokens;           /* tokens */
    int n_sections;             /* number of sections */
    qw_section *sections;       /* sections */
    int sorted;                 /* are tokens sorted? */
};

qw_synhi *qw_synhi_find_by_name(const char *name, qw_synhi *list);
qw_synhi *qw_synhi_new(const char *name, qw_synhi *next);
void qw_synhi_add_extension(qw_synhi *sh, const char *ext);
qw_synhi *qw_synhi_find_by_extension(const char *fname, qw_synhi *list);
void qw_synhi_add_signature(qw_synhi *sh, const char *signature);
qw_synhi *qw_synhi_find_by_signature(qw_block *b, qw_synhi *list);
void qw_synhi_add_token(qw_synhi *sh, const char *token, qw_attr attr);
void qw_synhi_add_section(qw_synhi *sh, const char *begin,
        const char *end, const char *escaped, qw_attr attr);
void qw_synhi_add_signature(qw_synhi *sh, const char *signature);
void qw_synhi_optimize(qw_synhi *sh);
qw_attr qw_synhi_find_token(qw_synhi *sh, const char *token);
void qw_synhi_apply_to_view(qw_view *view, qw_synhi *sh);

typedef struct qw_doc qw_doc;

struct qw_doc {
    qw_doc *prev;       /* pointer to previous */
    qw_doc *next;       /* pointer to next */
    char *fname;        /* file name */
    int vpos;           /* first visible line absolute position */
    int cpos;           /* cursor absolute position */
    int mark_s;         /* start of selection mark */
    int mark_e;         /* end of selection mark */
    int crlf;           /* CR/LF flag */
    int new_file;       /* new file (not from disk) flag */
    qw_block *b;        /* block of chains */
    qw_journal *j;      /* journal */
    qw_synhi *sh;       /* syntax highlight definition */
};

qw_block *qw_file_load(const char *fname, int *crlf);
int qw_file_save(qw_block *b, const char *fname, int crlf);
qw_doc *qw_doc_new(qw_doc *d, const char *fname);
qw_doc *qw_doc_destroy(qw_doc *doc);
void qw_doc_dump(qw_doc *d, FILE *f);

typedef enum {
#define X(keyid, keyname) keyid,
#include "qw_key.h"
#undef X
    QW_KEY_COUNT
} qw_key;

typedef enum {
#define X(opid, opname) opid,
#include "qw_op.h"
#undef X
    QW_OP_COUNT
} qw_op;

typedef struct qw_core qw_core;

struct qw_core {
    qw_doc *docs;               /* list of documents */
    qw_synhi *shs;              /* list of syntax highlight definitions */
    char *drvname;              /* driver name */
    int width;                  /* width of viewport */
    int height;                 /* height of viewport */
    int def_crlf;               /* default CR/LF flag */
    char *clip_data;            /* clipboard */
    int clip_size;              /* clipboard size */
    char *payload;              /* payload (to be freed when used) */
    int pl_size;                /* payload size */
    char *search;               /* search string */
    int search_size;            /* size of search string */
    int tab_size;               /* size in columns of a tab */
    int running;                /* running flag */
    int refresh;                /* in need for a refresh flag */
    qw_op keymap[QW_KEY_COUNT]; /* the keymap */
    qw_view view;               /* view */
    void *drv_data;             /* opaque pointer to drv internal data */
};

qw_core *qw_core_new(void);
void qw_core_create_view(qw_core *core, int *cursor_x, int *cursor_y);
void qw_core_key(qw_core *core, qw_key key);
void qw_core_dump(qw_core *core, FILE *f);
char *qw_core_status_line(qw_core *core, char *buf, int max_size);
void qw_core_doc_new(qw_core *core, const char *fname);

extern const char qw_default_cf[];

int qw_conf_parse_cmd(qw_core *core, const char *cmd);
void qw_conf_parse_default_cf(qw_core *core);
int qw_conf_load_file(qw_core *core, const char *fname);

int qw_drv_conf_attr(qw_core *core, qw_attr attr, int argc, char *argv[]);
int qw_drv_conf_font(qw_core *core, const char *font_face, const char *font_size);
void qw_drv_alert(qw_core *core, const char *prompt);
int qw_drv_confirm(qw_core *core, const char *prompt);
char *qw_drv_readline(qw_core *core, const char *prompt);
char *qw_drv_search(qw_core *core, const char *prompt);
char *qw_drv_open_file(qw_core *core, const char *prompt);
char *qw_drv_save_file(qw_core *core, const char *prompt);
int qw_drv_startup(qw_core *core);
int qw_drv_exec(qw_core *core);
char *qw_drv_conf_file(void);
void qw_drv_usage(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* QW_H */
