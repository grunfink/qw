/* qw - A minimalistic text editor by grunfink - public domain */

#include "config.h"

#include "qw.h"

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <sys/time.h>
#include <pwd.h>

static int sigwinch_received = 0;

struct ansi_drv_data {
    char attr[QW_ATTR_COUNT][64];   /* ANSI code for setting the attribute */
};


/** code **/

static void ansi_raw_tty(int start)
/* sets/unsets stdin in raw mode */
{
    static struct termios so;

    if (start) {
        struct termios o;

        /* save previous fd state */
        tcgetattr(0, &so);

        /* set raw */
        tcgetattr(0, &o);
        cfmakeraw(&o);
        tcsetattr(0, TCSANOW, &o);
    }
    else
        /* restore previously saved tty state */
        tcsetattr(0, TCSANOW, &so);
}


static int ansi_something_waiting(void)
/* returns yes if there is something waiting on fd */
{
    fd_set ids;
    struct timeval tv;

    /* reset */
    FD_ZERO(&ids);

    /* add stdin to set */
    FD_SET(0, &ids);

    tv.tv_sec  = 0;
    tv.tv_usec = 10000;

    return select(1, &ids, NULL, NULL, &tv) > 0;
}


char *ansi_read_string(int *size)
/* reads an ansi string, waiting in the first char */
{
    char *buf = NULL;
    int z = 0;
    int n = 0;

    while (ansi_something_waiting()) {
        char c;

        if (read(0, &c, sizeof(c)) == -1)
            break;
        else {
            if (n == z) {
                z += 32;
                buf = realloc(buf, z + 1);
            }

            /* carriage returns are always newlines */
            if (c == '\r')
                c = '\n';

            buf[n++] = c;
        }
    }

    /* null terminate for strcmp() when searching */
    if (buf != NULL)
        buf[n] = '\0';

    *size = n;

    return buf;
}


static void ansi_get_tty_size(qw_core *core)
/* asks the tty for its size */
{
    char *buffer;
    int size;
    int retries = 3;

    /* magic line: save cursor position, move to stupid position,
       ask for current position and restore cursor position */
    printf("\0337\033[r\033[999;999H\033[6n\0338");
    fflush(stdout);

    /* retry, as sometimes the answer is delayed from the signal */
    while (retries) {
        buffer = ansi_read_string(&size);

        if (size) {
            sscanf(buffer, "\033[%d;%dR", &core->height, &core->width);
            break;
        }

        usleep(100);
        retries--;
    }

    /* out of retries? assume most common size */
    if (retries == 0) {
        core->width  = 80;
        core->height = 25;
    }

    free(buffer);

    sigwinch_received = 0;
}


static void ansi_sigwinch(int s)
/* SIGWINCH signal handler */
{
    struct sigaction sa;

    sigwinch_received = 1;

    /* (re)attach signal */
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = ansi_sigwinch;
    sigaction(SIGWINCH, &sa, NULL);
}


static void ansi_gotoxy(int x, int y)
/* positions the cursor */
{
    printf("\033[%d;%dH", y + 1, x + 1);
}

#if 0
static void ansi_clrscr(void)
/* clears the screen */
{
    printf("\033[2J");
}
#endif

static void ansi_clreol(void)
/* clear to end of line */
{
    printf("\033[K");
}


static void ansi_enter_alt_screen(void)
/* enter alternate screen */
{
    printf("\033[?1049h");
}


static void ansi_leave_alt_screen(void)
/* leave alternate screen */
{
    /* set default attribute */
    printf("\033[0;39;49m\n");

    /* leave alternate screen */
    printf("\033[?1049l\n");
}


static void ansi_refresh(void)
/* force redraw */
{
    fflush(stdout);
}


static int ansi_find_color(const char *color)
/* finds a color index given its RGB */
{
    struct {
        int color_idx;      /* color index (ANSI code) with bold in bit 0x80 */
        int r, g, b;        /* components */
    } rgb2ansi[] = {
        { 0x00, 0x00, 0x00, 0x00 },
        { 0x04, 0x00, 0x00, 0x80 },
        { 0x84, 0x00, 0x00, 0xff },
        { 0x01, 0x80, 0x00, 0x00 },
        { 0x81, 0xff, 0x00, 0x00 },
        { 0x02, 0x00, 0x40, 0x00 },
        { 0x82, 0x00, 0xff, 0x00 },
        { 0x06, 0x00, 0xff, 0xff },
        { 0x06, 0x77, 0xff, 0xff },
        { 0x83, 0xff, 0xff, 0x00 },
        { 0x05, 0x80, 0x00, 0x80 },
        { 0x85, 0xff, 0x00, 0xff },
        { 0x87, 0xff, 0xff, 0xff },
        { -1,   0x00, 0x00, 0x00 }
    };
    int color_idx;

    if (strcmp(color, "default") == 0) {
        color_idx = 9;
    }
    else {
        int rgb;
        int n, r, g, b;
        int best = 0;
        int lastcol = 0x7fffffff;

        /* convert */
        sscanf(color, "%x", &rgb);

        /* split in components */
        r = (rgb & 0xff0000) >> 16;
        g = (rgb & 0x00ff00) >> 8;
        b = (rgb & 0x0000ff);

        /* find the most similar color */
        for (n = 0; rgb2ansi[n].color_idx != -1; n++) {
            int curcol = abs(r - rgb2ansi[n].r) +
                         abs(g - rgb2ansi[n].g) +
                         abs(b - rgb2ansi[n].b);

            if (curcol < lastcol) {
                lastcol = curcol;
                best = n;
            }
        }

        color_idx = rgb2ansi[best].color_idx;
    }

    return color_idx;
}


int qw_drv_conf_attr(qw_core *core, qw_attr attr, int argc, char *argv[])
/* configures an attribute */
{
    struct ansi_drv_data *dd = core->drv_data;
    int id, pd, it;

    /* ink and paper data */
    id = ansi_find_color(argv[2]);
    pd = ansi_find_color(argv[3]);

    /* italic flag */
    it = !!(argc == 5 && strcmp(argv[4], "italic") == 0);

#ifdef CONFOPT_ANSI_BOLD
    sprintf(dd->attr[attr], "\033[0;%s%s%d;%dm",
        it ? "3;" : "",
        (id & 0x80) ? "1;" : "",
        (id & 0x7f) + 30,
        (pd & 0x7f) + 40
    );
#else
    sprintf(dd->attr[attr], "\033[0;%s%d;%dm",
        it ? "3;" : "",
        (id & 0x7f) + ((id & 0x80) ? 90 : 30),
        (pd & 0x7f) + 40
    );
#endif

    return 0;
}


int qw_drv_conf_font(qw_core *core, const char *font_face, const char *font_size)
/* configures the font */
{
    /* we don't care about fonts here */
    return 0;
}


static void ansi_paint(qw_core *core)
/* dumps the current document to the screen */
{
    int cx, cy;
    int h;
    int i = 0;
    char buf[1024];
    qw_view *view;
    struct ansi_drv_data *dd = core->drv_data;

    /* create the view */
    qw_core_create_view(core, &cx, &cy);

    view = &core->view;

    for (h = 0; i < view->size && h < core->height; h++) {
        qw_attr attr = QW_ATTR_NONE;

        /* move to start of line */
        ansi_gotoxy(0, h);

        while (i < view->size) {
            /* different attribute? */
            if (attr != (qw_attr) view->attr[i]) {
                /* store for later */
                attr = (qw_attr) view->attr[i];

                printf("%s", dd->attr[attr]);
            }

            if (view->data[i] == '\n' || view->data[i] == '\r') {
                i++;
                break;
            }

            /* write the byte */
            fwrite(&view->data[i], 1, 1, stdout);

            i++;
        }

        printf("%s", dd->attr[QW_ATTR_NORMAL]);
        ansi_clreol();
    }

    /* clear the rest of lines */
    for (; h < core->height; h++) {
        ansi_gotoxy(0, h);
        ansi_clreol();
    }

    /* finally move cursor to its position */
    ansi_gotoxy(cx, cy);

    /* set title */
    printf("\033]0;%s\007", qw_core_status_line(core, buf, sizeof(buf)));

    /* force paint */
    ansi_refresh();

    core->refresh = 0;
}


struct ansi_key {
    char *ansi_str;
    qw_key key;
} ansi_keys[] = {
    { "\033[A",             QW_KEY_UP },
    { "\033[B",             QW_KEY_DOWN },
    { "\033[C",             QW_KEY_RIGHT },
    { "\033[D",             QW_KEY_LEFT },
    { "\033[5~",            QW_KEY_PGUP },
    { "\033[6~",            QW_KEY_PGDN },
    { "\033[H",             QW_KEY_HOME },
    { "\033[F",             QW_KEY_END },
    { "\033OP",             QW_KEY_F1 },
    { "\033OQ",             QW_KEY_F2 },
    { "\033OR",             QW_KEY_F3 },
    { "\033OS",             QW_KEY_F4 },
    { "\033[15~",           QW_KEY_F5 },
    { "\033[17~",           QW_KEY_F6 },
    { "\033[18~",           QW_KEY_F7 },
    { "\033[19~",           QW_KEY_F8 },
    { "\033[20~",           QW_KEY_F9 },
    { "\033[21~",           QW_KEY_F10 },
    { "\033[23~",           QW_KEY_F11 },
    { "\033[24~",           QW_KEY_F12 },
    { "\033[1;5A",          QW_KEY_CTRL_UP },
    { "\033[1;5B",          QW_KEY_CTRL_DOWN },
    { "\033[1;5C",          QW_KEY_CTRL_RIGHT },
    { "\033[1;5D",          QW_KEY_CTRL_LEFT },
    { "\033[1;5H",          QW_KEY_CTRL_HOME },
    { "\033[1;5F",          QW_KEY_CTRL_END },
    { "\033[3~",            QW_KEY_DEL },
    { "\033[Z",             QW_KEY_SHIFT_TAB },
    { "\033[1~",            QW_KEY_HOME },
    { "\033[4~",            QW_KEY_END },
    { "\033[5;5~",          QW_KEY_CTRL_PGUP },
    { "\033[6;5~",          QW_KEY_CTRL_PGDN },
    { "\033OB",             QW_KEY_CTRL_DOWN },
    { "\033OC",             QW_KEY_CTRL_RIGHT },
    { "\033OD",             QW_KEY_CTRL_LEFT },
    { "\n",                 QW_KEY_ENTER },
    { "\t",                 QW_KEY_TAB },
    { "\b",                 QW_KEY_BACKSPACE },
    { "\177",               QW_KEY_BACKSPACE },
    { "\x01",               QW_KEY_CTRL_A },
    { "\x02",               QW_KEY_CTRL_B },
    { "\x03",               QW_KEY_CTRL_C },
    { "\x04",               QW_KEY_CTRL_D },
    { "\x05",               QW_KEY_CTRL_E },
    { "\x06",               QW_KEY_CTRL_F },
    { "\x07",               QW_KEY_CTRL_G },
    { "\x0b",               QW_KEY_CTRL_K },
    { "\x0c",               QW_KEY_CTRL_L },
    { "\x0d",               QW_KEY_CTRL_M },
    { "\x0e",               QW_KEY_CTRL_N },
    { "\x0f",               QW_KEY_CTRL_O },
    { "\x10",               QW_KEY_CTRL_P },
    { "\x11",               QW_KEY_CTRL_Q },
    { "\x12",               QW_KEY_CTRL_R },
    { "\x13",               QW_KEY_CTRL_S },
    { "\x14",               QW_KEY_CTRL_T },
    { "\x15",               QW_KEY_CTRL_U },
    { "\x16",               QW_KEY_CTRL_V },
    { "\x17",               QW_KEY_CTRL_W },
    { "\x18",               QW_KEY_CTRL_X },
    { "\x19",               QW_KEY_CTRL_Y },
    { "\x1a",               QW_KEY_CTRL_Z },
    { "\x1b",               QW_KEY_ESC },
    { "\033-",              QW_KEY_ALT_MINUS },
};


int ansi_key_compare(const void *pa, const void *pb)
/* compare function for key finding */
{
    const struct ansi_key *ka = pa;
    const struct ansi_key *kb = pb;

    return strcmp(ka->ansi_str, kb->ansi_str);
}


static void ansi_key_optimize(void)
/* sorts the ansi_keys structure for faster access */
{
    qsort(&ansi_keys, sizeof(ansi_keys) / sizeof(struct ansi_key),
        sizeof(struct ansi_key), ansi_key_compare);
}


static qw_key ansi_get_key(qw_core *core)
/* gets a key */
{
    char *str;
    int size;
    qw_key key = QW_KEY_NONE;

    /* reads from stdin */
    str = ansi_read_string(&size);

    if (str != NULL) {
        struct ansi_key kp = { str, QW_KEY_NONE };
        struct ansi_key *kr;

        /* finds this string in the table */
        kr = bsearch(&kp, ansi_keys,
            sizeof(ansi_keys) / sizeof(struct ansi_key),
            sizeof(struct ansi_key), ansi_key_compare);

        if (kr != NULL) {
            /* found key */
            key = kr->key;
        }
        else
        if (str[0] != '\x1b') {
            /* if it's not an unknown ANSI sequence,
               assume it to be a direct string */
            key = QW_KEY_CHAR;

            /* store the read string in the payload */
            core->payload = str;
            core->pl_size = size;

            /* detach to avoid freeing */
            str = NULL;
        }
    }

    free(str);

    /* if a SIGWINCH was received, get size and force refresh */
    if (sigwinch_received) {
        ansi_get_tty_size(core);
        core->refresh++;
    }

    return key;
}


static int timeval_diff(struct timeval *tn, struct timeval *tp)
/* returns the different between two timevals in milliseconds */
{
    return ((tn->tv_sec  - tp->tv_sec) * 1000) +
           ((tn->tv_usec - tp->tv_usec) / 1000);
}


#define PAINT_PERIOD 100

static void ansi_main_loop(qw_core *core)
/* ansi driver main loop */
{
    struct timeval tp;

    gettimeofday(&tp, NULL);

    while (core->running) {
        qw_key key;
        struct timeval tn;

        /* get a (possible) key */
        key = ansi_get_key(core);

        /* process it */
        if (key != QW_KEY_NONE)
            qw_core_key(core, key);

        gettimeofday(&tn, NULL);

        /* has the paint period pass over? */
        if (timeval_diff(&tn, &tp) >= PAINT_PERIOD) {
            /* if a repaint is needed, do it */
            if (core->running && core->refresh)
                ansi_paint(core);

            /* set past time as current time */
            tp = tn;
        }
    }
}


void qw_drv_alert(qw_core *core, const char *prompt)
/* shows an alert */
{
    qw_key key;

    ansi_gotoxy(0, core->height - 1);
    printf("%s [ENTER]", prompt);
    ansi_clreol();
    ansi_refresh();

    while ((key = ansi_get_key(core)) != QW_KEY_ENTER)
        usleep(100);
}


int qw_drv_confirm(qw_core *core, const char *prompt)
/* asks for confirmation (1, yes; 0, no; -1, cancel) */
{
    int ret = -2;

    ansi_gotoxy(0, core->height - 1);
    printf("%s [Y/N/Esc] ", prompt);
    ansi_clreol();
    ansi_refresh();

    while (ret == -2) {
        qw_key key = ansi_get_key(core);

        if (key == QW_KEY_ESC)
            ret = -1;
        else
        if (key == QW_KEY_CHAR) {
            if ((core->payload[0] & 0x5f) == 'Y')
                ret = 1;
            else
            if ((core->payload[0] & 0x5f) == 'N')
                ret = 0;

            free(core->payload);
            core->payload = NULL;
        }
        else
            usleep(100);
    }

    return ret;
}


char *qw_drv_readline(qw_core *core, const char *prompt)
/* asks for line of text */
{
    char buf[4096] = "";
    int px, cx, y;
    int state = 0;
    int draw = 1;

    /* print prompt and setup */
    y = core->height - 1;
    ansi_gotoxy(0, y);
    printf("%s ", prompt);
    px = strlen(prompt) + 1;
    cx = 0;

    while (state == 0) {
        qw_key key;

        if (draw) {
            /* print content */
            ansi_gotoxy(px, y);
            printf("%s", buf);
            ansi_clreol();
            ansi_gotoxy(px + cx, y);
            ansi_refresh();
            draw = 0;
        }

        key = ansi_get_key(core);

        if (key == QW_KEY_ESC)
            state = -1;
        else
        if (key == QW_KEY_ENTER)
            state = 1;
        else
        if (key == QW_KEY_BACKSPACE) {
            if (cx > 0) {
                cx--;
                buf[cx] = '\0';
                draw = 1;
            }
        }
        else
        if (key == QW_KEY_CHAR) {
            /* copy the payload into the edition buffer */
            memcpy(&buf[cx], core->payload, core->pl_size);
            cx += core->pl_size;

            /* free it */
            free(core->payload);
            core->payload = NULL;

            buf[cx] = '\0';
            draw = 1;
        }
        else
            usleep(100);
    }

    return state == 1 ? strdup(buf) : NULL;
}


char *qw_drv_search(qw_core *core, const char *prompt)
/* asks for a string to be searched */
{
    return qw_drv_readline(core, prompt);
}


char *qw_drv_open_file(qw_core *core, const char *prompt)
/* asks for a file to be opened */
{
    return qw_drv_readline(core, prompt);
}


char *qw_drv_save_file(qw_core *core, const char *prompt)
/* asks for a file to be saved */
{
    return qw_drv_readline(core, prompt);
}


int qw_drv_startup(qw_core *core)
/* initializes the driver */
{
    core->drvname  = "ansi";
    core->def_crlf = 0;
    core->drv_data = calloc(1, sizeof(struct ansi_drv_data));

    return 0;
}


int qw_drv_exec(qw_core *core)
/* ansi driver startup */
{
    signal(SIGPIPE, SIG_IGN);

    ansi_raw_tty(1);
    ansi_sigwinch(0);
    ansi_get_tty_size(core);
    ansi_enter_alt_screen();

    ansi_key_optimize();

    /* push current title to stack */
    printf("\033[22t");

    ansi_main_loop(core);

    /* pop previous title */
    printf("\033[23t");

    ansi_raw_tty(0);
    ansi_leave_alt_screen();

    return 1;
}


char *qw_drv_conf_file(void)
/* returns the path of the configuration file */
{
    struct passwd *p;
    static char qwcf[2048] = "";
    char *ptr;

    if ((ptr = getenv("XDG_CONFIG_HOME")) != NULL) {
        strcat(qwcf, ptr);
        strcat(qwcf, "/");
    }
    else
    if ((p = getpwuid(getuid())) != NULL) {
        strcat(qwcf, p->pw_dir);
        strcat(qwcf, "/.config/");
    }
    else
    if ((ptr = getenv("HOME")) != NULL) {
        strcat(qwcf, ptr);
        strcat(qwcf, "/.config/");
    }

    strcat(qwcf, "qw.cf");

    return qwcf;
}


void qw_drv_usage(const char *str)
/* prints an usage string */
{
    printf("%s\n", str);
}
