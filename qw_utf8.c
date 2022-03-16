/* qw - A minimalistic text editor by grunfink - public domain */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "qw.h"


/** code **/

qw_block *qw_utf8_move(qw_block *b, int *pos, int inc)
/* moves to the previous/next utf8 character */
{
    while ((b = qw_block_move(b, *pos, pos, inc)) != NULL) {
        /* if it's not a continuation byte, done */
        if ((b->data[*pos] & 0xc0) != 0x80)
            break;
    }

    return b;
}


int qw_unicode_width(uint32_t cpoint)
/* returns the width of an Unicode codepoint (somewhat simplified) */
{
    int r = 1;

    if (cpoint == 0)                                    /* null */
        r = 0;
    else
    if (cpoint >= 0x300 && cpoint <= 0x36f)             /* diacritics */
        r = 0;
    else
    if (cpoint >= 0xe000 && cpoint <= 0xf8ff)           /* private use */
        r = 0;
    else
    if ((cpoint >= 0x1100 && cpoint <= 0x11ff)    ||    /* Hangul */
        (cpoint >= 0x2e80 && cpoint <= 0xa4cf)    ||    /* CJK */
        (cpoint >= 0xac00 && cpoint <= 0xd7a3)    ||    /* more Hangul */
        (cpoint >= 0xf900 && cpoint <= 0xfaff)    ||    /* CJK compatibility */
        (cpoint >= 0xff00 && cpoint <= 0xff60)    ||    /* fullwidth things */
        (cpoint >= 0xffdf && cpoint <= 0xffe6)    ||    /* fullwidth things */
        (cpoint >= 0x1f200 && cpoint <= 0x1ffff)  ||    /* emojis */
        (cpoint >= 0x20000 && cpoint <= 0x2fffd))       /* more CJK */
        r = 2;

    return r;
}


int qw_utf8_get_char(qw_block *b, int pos, char *buf)
/* gets an utf8 char into buf */
{
    int size = 0;

    /* get first byte */
    size = qw_block_get_str(b, pos, &buf[0], 1);

    while (size && (b = qw_block_move(b, pos, &pos, 1)) != NULL) {
        char c;

        /* store continuation bytes */
        if (((c = b->data[pos]) & 0xc0) == 0x80)
            buf[size++] = c;
        else
            break;
    }

    return size;
}


qw_block *qw_utf8_get_char_and_move(qw_block *b, int *pos, char *buf, int *size)
/* gets an utf8 char and moves forward */
{
    *size = qw_utf8_get_char(b, *pos, buf);
    return *size ? qw_block_move(b, *pos, pos, *size) : NULL;
}


int qw_utf8_size(const char *buf)
/* returns the size of the utf8 char at buf */
{
    int size = 0;

    if (buf[size] != '\0') {
        size++;

        while ((buf[size] & 0xc0) == 0x80)
            size++;
    }

    return size;
}


uint32_t qw_utf8_decode(const char *buf, int size)
/* converts an utf8 char to its codepoint */
{
    uint32_t cpoint = 0xfffd;   /* default, error */

    if ((buf[0] & 0x80) == 0 && size == 1)
        cpoint = buf[0];                        /* 1 byte char */
    else
    if ((buf[0] & 0xe0) == 0xc0 && size == 2) {
        cpoint = (buf[0] & 0x1f) << 6 |         /* 2 byte char */
                 (buf[1] & 0x3f);
    }
    else
    if ((buf[0] & 0xf0) == 0xe0 && size == 3) {
        cpoint = (buf[0] & 0x0f) << 12 |        /* 3 byte char */
                 (buf[1] & 0x3f) << 6 |
                 (buf[2] & 0x3f);
    }
    else
    if ((buf[0] & 0xf8) == 0xf0 && size == 4) {
        cpoint = (buf[0] & 0x07) << 18 |        /* 4 byte char */
                 (buf[1] & 0x3f) << 12 |
                 (buf[2] & 0x3f) << 6 |
                 (buf[3] & 0x3f);
    }

    return cpoint;
}


int qw_utf8_encode(uint32_t cpoint, char *buf)
/* converts a Unicode codepoint to utf8 */
{
    int n = 0;

    if (cpoint < 0x80)
        buf[n++] = (char) cpoint;
    else
    if (cpoint < 0x800) {
        buf[n++] = (char) (0xc0 | (cpoint >> 6));
        buf[n++] = (char) (0x80 | (cpoint & 0x3f));
    }
    else
    if (cpoint < 0x10000) {
        buf[n++] = (char) (0xe0 | (cpoint >> 12));
        buf[n++] = (char) (0x80 | ((cpoint >> 6) & 0x3f));
        buf[n++] = (char) (0x80 | (cpoint & 0x3f));
    }
    else
    if (cpoint < 0x200000) {
        buf[n++] = (char) (0xf0 | (cpoint >> 18));
        buf[n++] = (char) (0x80 | ((cpoint >> 12) & 0x3f));
        buf[n++] = (char) (0x80 | ((cpoint >> 6) & 0x3f));
        buf[n++] = (char) (0x80 | (cpoint & 0x3f));
    }

    return n;
}


int qw_utf8_str_width(const char *str, int size)
/* returns the width in columns of a string */
{
    int w = 0;

    while (size > 0 && *str) {
        int n = 1;

        /* step over continuation bytes */
        while (str[n] && (str[n] & 0xc0) == 0x80)
            n++;

        /* decode and calculate width */
        w += qw_unicode_width(qw_utf8_decode(str, n));

        str += n;
        size -= n;
    }

    return w;
}
