/* qw - A minimalistic text editor by grunfink - public domain */

#include "config.h"

#include <string.h>

#include "qw.h"

/** code **/

int main(int argc, char *argv[])
{
    int n;
    qw_core *core;

    if (argc >= 2 && strcmp(argv[1], "--help") == 0) {
        char buf[1024];

        sprintf(buf,
            "qw %s - A minimalistic text editor by grunfink - public domain\n\n"
            "Usage: qw [file(s)]\n\n"
            "Configuration file: %s\n",
            VERSION,
            qw_drv_conf_file()
        );

        qw_drv_usage(buf);

        return 1;
    }

    core = qw_core_new();

    qw_drv_startup(core);

    qw_conf_parse_default_cf(core);

    qw_conf_load_file(core, qw_drv_conf_file());

    /* load all files given as arguments */
    for (n = 1; n < argc; n++)
        qw_core_doc_new(core, argv[n]);

    /* no document? create an empty one */
    if (core->docs == NULL)
        qw_core_doc_new(core, NULL);

    /* move to next document */
    core->docs = core->docs->next;

    qw_drv_exec(core);

    return 0;
}
