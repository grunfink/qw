/* qw - A minimalistic text editor by grunfink - public domain */

#include "qw.h"

#include <stdlib.h>
#include <string.h>


/** code **/

static char **tokenize(const char *cmd, int *argc)
/* splits cmd into tokens */
{
    int n = 0;
    char **argv = NULL;

    *argc = 0;

    while (cmd[n] != '\0' && cmd[n] != '\n') {
        char c;
        char *token = NULL;
        int token_sz = 0;

        /* move forward while it's a blank */
        while (cmd[n] == ' ')
            n++;

        /* start storing */
        while ((c = cmd[n]) != ' ' && c != '\0' && c != '\n') {
            /* escaped char */
            if (c == '\\') {
                c = cmd[++n];

                if (c == 'n')
                    c = '\n';
            }

            token = realloc(token, token_sz + 2);
            token[token_sz++] = c;

            n++;
        }

        token[token_sz] = '\0';

        /* expand argv */
        argv = realloc(argv, sizeof(char *) * (*argc + 1));
        argv[*argc] = token;
        (*argc)++;
    }

    return argv;
}


static void destroy_tokenize(int argc, char *argv[])
/* destroy a token list */
{
    int n;

    for (n = 0; n < argc; n++)
        free(argv[n]);
}


static qw_attr find_attr(const char *attrname)
/* finds an attribute by name */
{
    qw_attr attr = QW_ATTR_NONE;

#define X(aid, aname) if (strcmp(attrname, aname) == 0) attr = aid;
#include "qw_attr.h"
#undef X

    return attr;
}


static qw_key find_key(const char *keyname)
/* finds a key by name */
{
    qw_key key = QW_KEY_NONE;

#define X(kid, kname) if (strcmp(keyname, kname) == 0) key = kid;
#include "qw_key.h"
#undef X

    return key;
}


static qw_op find_op(const char *opname)
/* finds an operation by name */
{
    qw_op op = QW_OP_NOP;

#define X(oid, oname) if (strcmp(opname, #oname) == 0) op = oid;
#include "qw_op.h"
#undef X

    return op;
}


int qw_conf_parse_cmd(qw_core *core, const char *cmd)
/* parses a command */
{
    int r = 0, n;
    int argc;
    char **argv;
    qw_synhi *sh;
    qw_attr attr;

    /* comments and empty lines are ok */
    if (cmd[0] == '\0' || cmd[0] == '#')
        goto end;

    argv = tokenize(cmd, &argc);

    if (argc == 0) {
        r = -1;
        goto end;
    }

    if (strcmp(argv[0], "tab_size") == 0) {
        if (argc != 2 || sscanf(argv[1], "%d", &core->tab_size) != 1)
            r = -1;
    }
    else
    if (strcmp(argv[0], "attr") == 0) {
        r = -1;

        if (argc >= 4) {
            attr = find_attr(argv[1]);

            if (attr != QW_ATTR_NONE)
                r = qw_drv_conf_attr(core, attr, argc, argv);
        }
    }
    else
    if (strcmp(argv[0], "sh_extension") == 0) {
        /* find or create the synhi definition */
        if ((sh = qw_synhi_find_by_name(argv[1], core->shs)) == NULL)
            sh = core->shs = qw_synhi_new(argv[1], core->shs);

        /* store all extensions */
        for (n = 2; n < argc; n++)
            qw_synhi_add_extension(sh, argv[n]);
    }
    else
    if (strcmp(argv[0], "sh_signature") == 0) {
        /* find or create the synhi definition */
        if ((sh = qw_synhi_find_by_name(argv[1], core->shs)) == NULL)
            sh = core->shs = qw_synhi_new(argv[1], core->shs);

        /* store all signatures */
        for (n = 2; n < argc; n++)
            qw_synhi_add_signature(sh, argv[n]);
    }
    else
    if (strcmp(argv[0], "sh_token") == 0) {
        if (argc >= 4) {
            /* find or create the synhi definition */
            if ((sh = qw_synhi_find_by_name(argv[1], core->shs)) == NULL)
                sh = core->shs = qw_synhi_new(argv[1], core->shs);

            attr = find_attr(argv[2]);

            if (attr != QW_ATTR_NONE) {
                /* store all tokens */
                for (n = 3; n < argc; n++)
                    qw_synhi_add_token(sh, argv[n], attr);
            }
            else
                r = -1;
        }
        else
            r = -1;
    }
    else
    if (strcmp(argv[0], "sh_section") == 0) {
        if (argc == 5 || argc == 6) {
            /* find or create the synhi definition */
            if ((sh = qw_synhi_find_by_name(argv[1], core->shs)) == NULL)
                sh = core->shs = qw_synhi_new(argv[1], core->shs);

            attr = find_attr(argv[2]);

            if (attr != QW_ATTR_NONE) {
                /* store block definition */
                qw_synhi_add_section(sh, argv[3], argv[4],
                                argc == 6 ? argv[5] : NULL, attr);
            }
        }
        else
            r = -1;
    }
    else
    if (strcmp(argv[0], "key") == 0) {
        if (argc == 3) {
            qw_key key;
            qw_op op;

            key = find_key(argv[1]);
            op  = find_op(argv[2]);

            if (key != QW_KEY_NONE && op != QW_OP_NOP)
                core->keymap[key] = op;
        }
        else
            r = -1;
    }
    else
    if (strcmp(argv[0], "font") == 0) {
        if (argc == 3)
            r = qw_drv_conf_font(core, argv[1], argv[2]);
        else
            r = -1;
    }
    else
        r = -1;

    destroy_tokenize(argc, argv);

end:
    return r;
}


void qw_conf_parse_default_cf(qw_core *core)
/* parses the default configuration file */
{
    const char *cmd = qw_default_cf;

    while (cmd != NULL) {
        qw_conf_parse_cmd(core, cmd);

        if ((cmd = strchr(cmd, '\n')) != NULL)
            cmd++;
    }
}


int qw_conf_load_file(qw_core *core, const char *fname)
/* loads and parses a configuration */
{
    FILE *f;
    int ret = 0;

    if ((f = fopen(fname, "r")) != NULL) {
        char line[4096];

        while (fgets(line, sizeof(line) - 1, f)) {
            if (qw_conf_parse_cmd(core, line) == -1)
                ret = -1;
        }
    }
    else
        ret = 1;

    return ret;
}
