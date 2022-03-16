/* qw - A minimalistic text editor by grunfink - public domain */

#include "qw.h"

#include <stdlib.h>
#include <string.h>

int qw_view_row_size(qw_block *b, int pos, int width)
/* returns the size of a view row in bytes */
{
    int w = 0;
    char uchr[32] = "";
    int cnt = 0;
    int p = pos;
    int size = -1;

    while (b && uchr[0] != '\n') {
        int csz;

        /* get one utf8 char */
        b = qw_utf8_get_char_and_move(b, &p, uchr, &csz);

        if (csz == 0) {
            /* count the EOF as 1 char size */
            cnt++;
            break;
        }

        /* calculate its width */
        w += qw_unicode_width(qw_utf8_decode(uchr, csz));

        /* if this char would overflow the width, finish */
        if (w > width)
            break;

        /* count new bytes */
        cnt += csz;

        /* remember blank positions */
        if (uchr[0] == ' ' || uchr[0] == '\n')
            size = cnt;
    }

    /* break here if at EOF or no blank were seen */
    if (b == NULL || size == -1)
        size = cnt;

    return size;
}


int qw_view_get_col_0(qw_block *b, int apos, int width, int *size)
/* returns the absolute position of column #0 */
{
    int p;
    int ac0;

    /* convert to relative */
    b = qw_block_abs_to_rel(b, apos, &p);

    /* get the first byte of the physical line */
    b = qw_block_move_bol(b, &p);

    /* convert to absolute */
    ac0 = qw_block_rel_to_abs(b, p);

    while (b != NULL) {
        /* calculate row size */
        *size = qw_view_row_size(b, p, width);

        if (*size == 0)
            break;

        /* if apos is between this column #0 and the end, done */
        if (apos >= ac0 && apos < ac0 + *size)
            break;

        /* keep moving */
        b = qw_block_move(b, p, &p, *size);
        ac0 += *size;
    }

    /* size also keeps the size of the row */

    return ac0;
}


int qw_view_width_diff(qw_block *b, int apos0, int apos1)
/* returns the difference in width between two absolute positions */
{
    int i;
    int width = 0;

    b = qw_block_abs_to_rel(b, apos0, &i);

    while (b != NULL && apos0 < apos1) {
        char uchr[32];
        int csz;

        /* get one utf8 char */
        b = qw_utf8_get_char_and_move(b, &i, uchr, &csz);

        if (csz == 0)
            break;

        /* calculate its width */
        width += qw_unicode_width(qw_utf8_decode(uchr, csz));

        /* increment position */
        apos0 += csz;
    }

    return width;
}


int qw_view_set_col(qw_block *b, int ac0, int col, int width)
/* returns the new position after moving to col */
{
    int i;
    int s = 0, w = 0;
    int size;

    b = qw_block_abs_to_rel(b, ac0, &i);

    /* will not move beyond this size */
    size = qw_view_row_size(b, i, width);

    while (b != NULL && w < col && s < size) {
        char uchr[32];
        int csz;

        /* get one utf8 char */
        csz = qw_utf8_get_char(b, i, uchr);

        if (csz == 0 || s + csz >= size)
            break;

        b = qw_block_move(b, i, &i, csz);

        /* calculate its width */
        w += qw_unicode_width(qw_utf8_decode(uchr, csz));

        /* increment the size */
        s += csz;
    }

    return qw_block_rel_to_abs(b, i);
}


int qw_view_fix_vpos(qw_block *b, int vpos, int cpos, int wdth, int hght)
/* fixes the vpos for the cursor always be visible */
{
    int i;
    int size, h = 0;
    int *ac0 = NULL;

    if (cpos < vpos) {
        /* cpos above vpos: just set it the col #0 for cpos */
        vpos = qw_view_get_col_0(b, cpos, wdth, &size);
        goto end;
    }

    b = qw_block_abs_to_rel(b, vpos, &i);

    /* allocate a circular buffer to store a double set of col #0 addresses */
    hght *= 2;
    ac0 = malloc(sizeof(int) * hght);

    /* fill the first half with the current vpos */
    for (h = 0; h < hght / 2; h++)
        ac0[h] = vpos;
    h = 0;

    for (;;) {
        /* store */
        ac0[(h + hght / 2 - 2) % hght] = vpos;

        /* get the row size */
        size = qw_view_row_size(b, i, wdth);

        /* if cpos is inside this line, set as valid */
        if (cpos >= vpos && cpos <= vpos + size)
            break;

        /* keep moving */
        b = qw_block_move(b, i, &i, size);
        vpos += size;
        h++;
    }

    vpos = ac0[h % hght];

end:
    free(ac0);

    return vpos;
}
