/* qw - A minimalistic text editor by grunfink - public domain */

#include "qw.h"

#include <stdlib.h>
#include <string.h>


int qw_attr_pack(qw_attr attr, char *uchr)
/* packs an attribute into an Unicode private character */
{
    uchr[0] = (char) 0xee;
    uchr[1] = (char) 0x80;
    uchr[2] = (char) (0x80 | (attr & 0x0f));

    return 3;
}


qw_attr qw_attr_unpack(const char *uchr, int size)
/* unpacks an attribute, stored as a Unicode private character */
{
    qw_attr attr = QW_ATTR_NONE;

    /* is it an attribute? (private Unicode set, U+E000 + attr) */
    if (size == 3 && uchr[0] == (char) 0xee &&
                     uchr[1] == (char) 0x80 &&
                    (uchr[2] & 0xf0) == 0x80) {
        attr = uchr[2] & 0x0f;
    }

    return attr;
}
