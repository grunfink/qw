/* qw - A minimalistic text editor by grunfink - public domain */

#include "config.h"

#include "qw.h"

#include <stdio.h>
#include <windows.h>
#include <shlobj.h>

struct windows_drv_data {
    HWND hwnd;                      /* the window handler */
    HFONT fonts[2];                 /* the fonts (normal/italic) */
    int font_width;                 /* font width */
    int font_height;                /* font height */
    COLORREF inks[QW_ATTR_COUNT];   /* ink colors */
    COLORREF papers[QW_ATTR_COUNT]; /* paper colors */
    int italic[QW_ATTR_COUNT];      /* italic flag */
    char *font_face;                /* font face (name) */
    int font_size;                  /* font size */
};


static void windows_build_font(qw_core *core)
/* build the font and get its metrics */
{
    TEXTMETRIC tm;
    HDC hdc;
    int i;
    struct windows_drv_data *dd = core->drv_data;

    hdc = GetDC(dd->hwnd);

    if (dd->font_face != NULL) {
        i = -MulDiv(dd->font_size, GetDeviceCaps(hdc, LOGPIXELSY), 72);

        dd->fonts[0] = CreateFont(i, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, FIXED_PITCH, dd->font_face);
        dd->fonts[1] = CreateFont(i, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, FIXED_PITCH, dd->font_face);
    }
    else
        dd->fonts[0] = dd->fonts[1] = GetStockObject(SYSTEM_FIXED_FONT);

    SelectObject(hdc, dd->fonts[0]);
    GetTextMetrics(hdc, &tm);

    ReleaseDC(dd->hwnd, hdc);

    dd->font_height = tm.tmHeight;
    dd->font_width  = tm.tmAveCharWidth;
}


static void windows_resize(qw_core *core, HWND hwnd)
/* window has been resized */
{
    size_t tx, ty;
    RECT rect;
    struct windows_drv_data *dd = core->drv_data;

    /* no font? build it */
    if (dd->fonts[0] == 0)
        windows_build_font(core);

    GetClientRect(hwnd, &rect);

    tx = ((rect.right - rect.left) / dd->font_width) + 1;
    ty = ((rect.bottom - rect.top) / dd->font_height);

    core->width  = tx;
    core->height = ty;

    CreateCaret(hwnd, NULL, dd->font_width, dd->font_height);
    ShowCaret(hwnd);
}


static void windows_drop_files(qw_core *core, HDROP hDrop)
/* a file has been dropped */
{
    int n;
    char tmp[16384];

    n = DragQueryFile(hDrop, 0xffffffff, NULL, sizeof(tmp) - 1);

    while (--n >= 0) {
        DragQueryFile(hDrop, n, tmp, sizeof(tmp) - 1);

        qw_core_doc_new(core, tmp);
    }

    DragFinish(hDrop);
}


int qw_drv_conf_attr(qw_core *core, qw_attr attr, int argc, char *argv[])
/* configures an attribute */
{
    struct windows_drv_data *dd = core->drv_data;
    int id, pd;

    /* ink and paper data */
    if (strcmp(argv[2], "default") == 0)
        id = 0x000000;
    else
        sscanf(argv[2], "%x", &id);

    if (strcmp(argv[3], "default") == 0)
        pd = 0xffffff;
    else
        sscanf(argv[3], "%x", &pd);

    dd->inks[attr]   = ((id & 0x000000ff) << 16) |
                       ((id & 0x0000ff00)) |
                       ((id & 0x00ff0000) >> 16);
    dd->papers[attr] = ((pd & 0x000000ff) << 16) |
                       ((pd & 0x0000ff00)) |
                       ((pd & 0x00ff0000) >> 16);
    dd->italic[attr] = !!(argc == 5 && strcmp(argv[4], "italic") == 0);

    return 0;
}


int qw_drv_conf_font(qw_core *core, const char *font_face, const char *font_size)
/* configures the font */
{
    struct windows_drv_data *dd = core->drv_data;

    free(dd->font_face);
    dd->font_face = strdup(font_face);

    sscanf(font_size, "%d", &dd->font_size);

    windows_build_font(core);
    windows_resize(core, dd->hwnd);

    return 0;
}


static void windows_paint(qw_core *core, HWND hwnd)
/* main painting function */
{
    HDC hdc;
    PAINTSTRUCT ps;
    RECT rect;
    struct windows_drv_data *dd = core->drv_data;
    char buf[1024];

    /* build font, if needed */
    if (dd->fonts[0] == 0)
        windows_build_font(core);

    hdc = BeginPaint(hwnd, &ps);
    GetClientRect(hwnd, &rect);

    /* defaults */
    SelectObject(hdc, dd->fonts[0]);
    SetTextColor(hdc, dd->inks[QW_ATTR_NORMAL]);
    SetBkColor(hdc, dd->papers[QW_ATTR_NORMAL]);

    if (core->docs != NULL) {
        int cx, cy;
        int y;
        int i = 0;
        qw_view *view;

        /* create the view */
        qw_core_create_view(core, &cx, &cy);
        view = &core->view;

        SetCaretPos(cx * dd->font_width, cy * dd->font_height);

        for (y = 0; i < view->size && y < core->height; y++) {
            WCHAR buf[4096];
            int z = 0;
            int nl = 0, na = 0;
            int x = 0, w = 0;
            qw_attr attr = (qw_attr) view->attr[i];

            while (i < view->size) {
                /* different attribute? flag it */
                if (attr != (qw_attr) view->attr[i])
                    na = 1;

                /* EOL? flag it */
                if (view->data[i] == '\n' || view->data[i] == '\r')
                    nl = 1;

                /* flush */
                if (nl || na) {
                    /* draw the string */
                    SelectObject(hdc, dd->fonts[dd->italic[attr]]);
                    SetTextColor(hdc, dd->inks[attr]);
                    SetBkColor(hdc, dd->papers[attr]);
                    TextOutW(hdc, rect.left + x * dd->font_width, rect.top, buf, z);

                    /* move column */
                    x += w;

                    attr = (qw_attr) view->attr[i];

                    na = z = w = 0;
                }

                if (nl) {
                    i++;
                    break;
                }
                else {
                    /* add the new char to the string */
                    int csz;
                    uint32_t cpoint;

                    /* decode and store */
                    csz      = qw_utf8_size(&view->data[i]);
                    cpoint   = qw_utf8_decode(&view->data[i], csz);
                    buf[z++] = (WCHAR) cpoint;

                    w += qw_unicode_width(cpoint);
                    i += csz;
                }
            }

            /* fill the line with spaces */
            for (z = 0; z < core->width - x; z++)
                buf[z] = (WCHAR) ' ';

            /* print them */
            SelectObject(hdc, dd->fonts[0]);
            SetBkColor(hdc, dd->papers[QW_ATTR_NORMAL]);
            TextOutW(hdc, rect.left + x * dd->font_width,
                rect.top, buf, core->width - x);

            /* move down the top of the rect */
            rect.top += dd->font_height;
        }
    }

    /* fill the rest of the window with emptyness */
    SetBkColor(hdc, dd->papers[QW_ATTR_NORMAL]);
    ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rect, "", 0, 0);

    EndPaint(hwnd, &ps);

    /* get the status line */
    qw_core_status_line(core, buf, sizeof(buf));
    SetWindowText(hwnd, buf);

    core->refresh = 0;
}



struct vkey2key {
    int vkey;           /* the virtual key */
    qw_key key;         /* the key (without ctrl) */
    qw_key ckey;        /* the key (with ctrl) */
} vkey2keys[] = {
    { VK_UP,        QW_KEY_UP,          QW_KEY_CTRL_UP },
    { VK_DOWN,      QW_KEY_DOWN,        QW_KEY_CTRL_DOWN },
    { VK_LEFT,      QW_KEY_LEFT,        QW_KEY_CTRL_LEFT },
    { VK_RIGHT,     QW_KEY_RIGHT,       QW_KEY_CTRL_RIGHT },
    { VK_PRIOR,     QW_KEY_PGUP,        QW_KEY_CTRL_PGUP },
    { VK_NEXT,      QW_KEY_PGDN,        QW_KEY_CTRL_PGDN },
    { VK_HOME,      QW_KEY_HOME,        QW_KEY_CTRL_HOME },
    { VK_END,       QW_KEY_END,         QW_KEY_CTRL_END },
    { VK_RETURN,    QW_KEY_ENTER,       QW_KEY_CTRL_ENTER },
    { VK_BACK,      QW_KEY_BACKSPACE,   QW_KEY_NONE },
    { VK_DELETE,    QW_KEY_DEL,         QW_KEY_NONE },
    { VK_F1,        QW_KEY_F1,          QW_KEY_CTRL_F1 },
    { VK_F2,        QW_KEY_F2,          QW_KEY_CTRL_F2 },
    { VK_F3,        QW_KEY_F3,          QW_KEY_CTRL_F3 },
    { VK_F4,        QW_KEY_F4,          QW_KEY_CTRL_F4 },
    { VK_F5,        QW_KEY_F5,          QW_KEY_CTRL_F5 },
    { VK_F6,        QW_KEY_F6,          QW_KEY_CTRL_F6 },
    { VK_F7,        QW_KEY_F7,          QW_KEY_CTRL_F7 },
    { VK_F8,        QW_KEY_F8,          QW_KEY_CTRL_F8 },
    { VK_F9,        QW_KEY_F9,          QW_KEY_CTRL_F9 },
    { VK_F10,       QW_KEY_F10,         QW_KEY_CTRL_F10 },
    { VK_F11,       QW_KEY_F11,         QW_KEY_CTRL_F11 },
    { VK_F11,       QW_KEY_F12,         QW_KEY_CTRL_F12 },
};


static int vkey_compare(const void *pa, const void *pb)
/* vkey compare function */
{
    const struct vkey2key *vka = pa;
    const struct vkey2key *vkb = pb;

    return vka->vkey - vkb->vkey;
}


static void vkey_optimize(void)
/* optimizes (sorts) the vkey2key structure */
{
    qsort(vkey2keys, sizeof(vkey2keys) / sizeof(struct vkey2key),
        sizeof(struct vkey2key), vkey_compare);
}


static void windows_vkey(qw_core *core, int m)
/* a control key has been received */
{
    qw_key key = QW_KEY_NONE;
    struct vkey2key ks = { m, QW_KEY_NONE, QW_KEY_NONE };
    struct vkey2key *kr;

    /* search for this key */
    kr = bsearch(&ks, vkey2keys,
        sizeof(vkey2keys) / sizeof(struct vkey2key),
        sizeof(struct vkey2key), vkey_compare);

    if (kr != NULL) {
        /* select according the ctrl key state */
        key = (GetKeyState(VK_CONTROL) & 0x8000) ? kr->ckey : kr->key;
    }

    if (key != QW_KEY_NONE)
        qw_core_key(core, key);
}


#define ctrl(c) ((c) & 31)

static void windows_akey(qw_core *core, int m)
/* a non-control key has been received */
{
    qw_key key = QW_KEY_NONE;

    if (m == ctrl('i')) {
        /* tab or shift-tab */
        key = (GetKeyState(VK_SHIFT) & 0x8000) ? QW_KEY_SHIFT_TAB : QW_KEY_TAB;
    }
    else
    if (m >= ctrl('a') && m <= ctrl('z')) {
        /* control keys */
        key = QW_KEY_CTRL_A + (m - ctrl('a'));
    }
    else
    if (m == '-' && (GetKeyState(VK_LMENU) & 0x8000)) {
        /* alt-minus */
        key = QW_KEY_ALT_MINUS;
    }
    else
    if (m == '\x1b') {
        /* escape */
        key = QW_KEY_ESC;
    }
    else {
        char uchr[32];

        /* it's a key */
        key = QW_KEY_CHAR;

        memset(uchr, '\0', sizeof(uchr));

        /* encode the key to utf8 and store as the payload */
        core->pl_size = qw_utf8_encode((uint32_t) m, uchr);
        core->payload = strdup(uchr);
    }

    if (key != QW_KEY_NONE)
        qw_core_key(core, key);
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    qw_core *core;

    core = (qw_core *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE:
        DragAcceptFiles(hwnd, TRUE);
        return 0;

    case WM_PAINT:
        windows_paint(core, hwnd);
        return 0;

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
        windows_vkey(core, wparam);
        return 0;

    case WM_CHAR:
        windows_akey(core, wparam);
        return 0;

    case WM_SIZE:
        if (!IsIconic(hwnd)) {
            core->refresh = 1;
            windows_resize(core, hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }

        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_DROPFILES:
        windows_drop_files(core, (HDROP) wparam);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_TIMER:
        if (core->refresh)
            InvalidateRect(hwnd, NULL, FALSE);

        return 0;

    case WM_SETFOCUS:
        if (core != NULL && core->drv_data != NULL) {
            struct windows_drv_data *dd = core->drv_data;

            if (dd->fonts[0] == 0)
                windows_build_font(core);

            CreateCaret(hwnd, NULL, dd->font_width, dd->font_height);

            ShowCaret(hwnd);
        }

        return 0;

    case WM_KILLFOCUS:
        DestroyCaret();

        return 0;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}


void qw_drv_alert(qw_core *core, const char *prompt)
/* shows an alert window */
{
    struct windows_drv_data *dd = core->drv_data;

    MessageBox(dd->hwnd, prompt, "qw", MB_ICONWARNING | MB_OK);
}


int qw_drv_confirm(qw_core *core, const char *prompt)
/* asks for confirmation (1, yes; 0, no; -1, cancel) */
{
    struct windows_drv_data *dd = core->drv_data;
    int ret = 0;

    ret = MessageBox(dd->hwnd, prompt, "qw", MB_ICONQUESTION | MB_YESNOCANCEL);

    return ret == IDYES ? 1 : ret == IDNO ? 0 : -1;
}


/* input box communication pointer */
static char *inputbox_ptr = NULL;

BOOL CALLBACK InputBoxProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
/* text input dialog */
{
    switch(msg)
    {
    case WM_INITDIALOG:

        SetWindowText(hwnd, "qw");

        /* set prompt */
        SetDlgItemText(hwnd, 1500, inputbox_ptr);

        inputbox_ptr = NULL;

        return(TRUE);

    case WM_COMMAND:

        switch(LOWORD(wparam)) {
        case IDOK:
        case IDCANCEL:

            if(LOWORD(wparam) == IDOK) {
                char buf[4096];

                /* get input box content */
                GetDlgItemText(hwnd, 1501, buf, sizeof(buf));

                inputbox_ptr = strdup(buf);
            }

            EndDialog(hwnd, 0);

            return(TRUE);
        }
    }

    return(FALSE);
}


char *qw_drv_readline(qw_core *core, const char *prompt)
/* asks for line of text */
{
    struct windows_drv_data *dd = core->drv_data;

    /* communicate the prompt */
    inputbox_ptr = (char *)prompt;

    /* call the dialog */
    DialogBoxW(GetModuleHandle(NULL), L"QWINPUTBOX", dd->hwnd, InputBoxProc);

    /* inputbox_ptr will be NULL on cancel */
    return inputbox_ptr;
}


char *qw_drv_search(qw_core *core, const char *prompt)
/* asks for a string to be searched */
{
    return qw_drv_readline(core, prompt);
}


static char *open_or_save(qw_core *core, const char *prompt, int o)
{
    struct windows_drv_data *dd = core->drv_data;
    OPENFILENAME ofn;
    char buf1[1024] = "";
    char buf2[1024];
    int r;

    memset(&ofn, '\0', sizeof(OPENFILENAME));
    ofn.lStructSize  = sizeof(OPENFILENAME);
    ofn.hwndOwner    = dd->hwnd;
    ofn.lpstrFilter  = "*.*\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile    = buf1;
    ofn.nMaxFile     = sizeof(buf1);
    ofn.lpstrTitle   = prompt;
    ofn.lpstrDefExt  = "";

    GetCurrentDirectory(sizeof(buf2), buf2);
    ofn.lpstrInitialDir = buf2;

    if (o) {
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
            OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST;

        r = GetOpenFileName(&ofn);
    }
    else {
        ofn.Flags = OFN_HIDEREADONLY;

        r = GetSaveFileName(&ofn);
    }

    return r ? strdup(buf1) : NULL;
}


char *qw_drv_open_file(qw_core *core, const char *prompt)
/* asks for a file to be opened */
{
    return open_or_save(core, prompt, 1);
}


char *qw_drv_save_file(qw_core *core, const char *prompt)
/* asks for a file to be saved */
{
    return open_or_save(core, prompt, 0);
}


int qw_drv_startup(qw_core *core)
/* initializes the driver */
{
    WNDCLASSW wc;
    HWND hwnd;
    HINSTANCE hinst;

    core->drvname  = "windows";
    core->def_crlf = 1;
    core->drv_data = calloc(1, sizeof(struct windows_drv_data));

    vkey_optimize();

    hinst = GetModuleHandle(NULL);

    wc.style            = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc      = WndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = hinst;
    wc.hIcon            = LoadIcon(hinst, "QW_ICON");
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = NULL;
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = L"qw";

    RegisterClassW(&wc);

    /* create the window */
    hwnd = CreateWindowW(L"qw", L"qw " VERSION,
                         WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         NULL, NULL, hinst, NULL);

    if (hwnd) {
        struct windows_drv_data *dd = core->drv_data;

        dd->hwnd = hwnd;

        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) core);

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        SetTimer(hwnd, 1, 100, NULL);
    }

    return 0;
}


int qw_drv_exec(qw_core *core)
/* ansi driver startup */
{
    MSG msg;

    while (core->running && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}


char *qw_drv_conf_file(void)
/* returns the path of the configuration file */
{
    static char qwcf[2048] = "";
    LPITEMIDLIST pidl;

    /* get the 'My Documents' folder */
    SHGetSpecialFolderLocation(NULL, CSIDL_LOCAL_APPDATA, &pidl);
    SHGetPathFromIDList(pidl, qwcf);
    strcat(qwcf, "\\qw.cf");

    return qwcf;
}


void qw_drv_usage(const char *str)
/* prints an usage string */
{
    MessageBox(NULL, str, "qw", MB_OK);
}
