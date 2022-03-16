/* qw - A minimalistic text editor by grunfink - public domain */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <locale.h>

#include "qw.h"

/* total number of tests and oks */
int tests = 0;
int oks = 0;

/* failed tests messages */
char *failed_msgs[5000];
int i_failed_msgs = 0;

int verbose = 0;
int _do_benchmarks = 0;

void _do_test(char *desc, int res, int src_line)
{
    tests++;

    if (res) {
        oks++;
//        printf("OK\n");
        if (verbose)
            printf("stress.c:%d: #%d %s: OK\n", src_line, tests, desc);
    }
    else {
        printf("stress.c:%d: #%d error: %s *** Failed ***\n", src_line, tests, desc);
    }
}


int test_summary(void)
{
    printf("\n*** Total tests passed: %d/%d\n", oks, tests);

    if (oks == tests)
        printf("*** ALL TESTS PASSED\n");
    else {
        printf("*** %d %s\n", tests - oks, "TESTS ---FAILED---");

/*        printf("\nFailed tests:\n\n");
        for (n = 0; n < i_failed_msgs; n++)
            printf("%s", failed_msgs[n]);*/
    }

    return oks == tests ? 0 : 1;
}


#define do_test(d, r) _do_test(d, r, __LINE__)


double diff_time(struct timeval *st, struct timeval *et)
{
    double t = 0.0;

    if (et == NULL)
        gettimeofday(st, NULL);
    else {
        gettimeofday(et, NULL);

        t = et->tv_sec - st->tv_sec + (et->tv_usec - st->tv_usec) / 1000000.0;
    }

    return t;
}


#define STRLEN 4096

void test_block(void)
{
    char str[STRLEN];
    qw_block *b;
    int z, i;
    char c;

    b = qw_block_new(NULL, NULL);
    do_test("qw_block_new", b != NULL);

    b = qw_block_insert_str(b, 0, "12345", 5);

    if (verbose)
        qw_block_dump(qw_block_first(b), stdout);

    b = qw_block_insert_str(b, 5, "ABCDE", 5);

    if (verbose)
        qw_block_dump(qw_block_first(b), stdout);

    z = qw_block_get_str(b, 0, str, STRLEN);
    do_test("sizeof get 1", z == 10);
    do_test("insert & get 1", strncmp(str, "12345ABCDE", 10) == 0);
    do_test("blk integrity 1", b->prev == NULL && b->next == NULL);

    b = qw_block_insert_str(b, 0, "abcde", 5);

    if (verbose)
        qw_block_dump(qw_block_first(b), stdout);

    z = qw_block_get_str(b, 0, str, STRLEN);
    do_test("sizeof get 2", z == 15);
    do_test("insert & get 2", strncmp(str, "abcde12345ABCDE", 15) == 0);
    do_test("blk integrity 2", b->prev == NULL && b->next != NULL);

    z = qw_block_get_str(b->next, 0, str, STRLEN);
    do_test("sizeof get 3", z == 10);
    do_test("insert & get 3", strncmp(str, "12345ABCDE", 10) == 0);

    b = qw_block_insert_str(b, 2, "---", 3);

    if (verbose)
        qw_block_dump(qw_block_first(b), stdout);

    z = qw_block_get_str(b, 0, str, STRLEN);
    do_test("sizeof get 3", z == 18);
    do_test("insert & get 3", strncmp(str, "ab---cde12345ABCDE", z) == 0);

    b = qw_block_insert_str(b->next, 0, "!!!", 3);

    if (verbose)
        qw_block_dump(qw_block_first(b), stdout);

    b = qw_block_first(b);
    z = qw_block_get_str(b, 0, str, STRLEN);
    do_test("sizeof get 4", z == 21);
    do_test("insert & get 4", strncmp(str, "ab---!!!cde12345ABCDE", z) == 0);

    b = qw_block_first(b);
    qw_block_delete(b, 2, 6);

    if (verbose)
        qw_block_dump(qw_block_first(b), stdout);

    z = qw_block_get_str(b, 0, str, STRLEN);
    do_test("sizeof get 5", z == 15);
    do_test("delete & get 5", strncmp(str, "abcde12345ABCDE", z) == 0);

    b = qw_block_move(b, 0, &i, 1);
    qw_block_get_str(b, i, &c, 1);
    do_test("fwd 1", c == 'b');
    b = qw_block_move(b, i, &i, 1);
    qw_block_get_str(b, i, &c, 1);
    do_test("fwd 2", c == 'c');
    b = qw_block_move(b, i, &i, 11);
    qw_block_get_str(b, i, &c, 1);
    do_test("fwd 3", c == 'D');
    do_test("fwd (now in a different block)", b != qw_block_first(b));
    do_test("fwd 5 beyond end returns NULL", qw_block_move(b, i, &i, 100) == NULL);
    b = qw_block_move(b, i, &i, 1);
    qw_block_get_str(b, i, &c, 1);
    do_test("fwd 7", c == 'E');
    b = qw_block_move(b, i, &i, 1);
    do_test("fwd 8.1 move to EOF succeeds", b != NULL);
    z = qw_block_get_str(b, i, &c, 1);
    do_test("fwd 8.2 but a get returns zero", z == 0);
    do_test("fwd 9 moving further fails as expected", qw_block_move(b, i, &i, 1) == NULL);

    b = qw_block_move(b, i, &i, -1);
    z = qw_block_get_str(b, i, &c, 1);
    do_test("bwd 1", c == 'E');
    b = qw_block_move(b, i, &i, -2);
    z = qw_block_get_str(b, i, &c, 1);
    do_test("bwd 2", c == 'C');
    b = qw_block_move(b, i, &i, -10);
    z = qw_block_get_str(b, i, &c, 1);
    do_test("bwd 3", c == L'c');
    do_test("bwd 4 beyond start returns NULL", qw_block_move(b, i, &i, -100) == NULL);

    b = qw_block_abs_to_rel(b, 15, &i);
    do_test("absolute offset 1", qw_block_rel_to_abs(b, i) == 15);
    b = qw_block_move(b, i, &i, -5);
    do_test("absolute offset 2", qw_block_rel_to_abs(b, i) == 10);
    b = qw_block_move(b, i, &i, -5);
    do_test("absolute offset 3", qw_block_rel_to_abs(b, i) == 5);
    b = qw_block_move(b, i, &i, -5);
    do_test("absolute offset 4", qw_block_rel_to_abs(b, i) == 0);

    b = qw_block_first(b);
    b = qw_block_insert_str(b, 2, "---!!!", 6);

    if (verbose)
        qw_block_dump(qw_block_first(b), stdout);

    b = qw_block_first(b);
    do_test("here 1", qw_block_here(b, 0, "ab", 2) != NULL);
    do_test("here 2", qw_block_here(b, 1, "ab", 2) == NULL);
    do_test("here 3", qw_block_here(b, 0, "ab---!!!cde", 11) != NULL);
    do_test("here 4", qw_block_here(b, 2, "---!!!cde12", 11) != NULL);

    b = qw_block_first(b);
    i = 0;
    do_test("search fwd 1", (b = qw_block_search(b, &i, "ABC", 3, 1)) != NULL);

    if (b) {
        z = qw_block_get_str(b, i, str, 3);
        do_test("search fwd 1.2", strncmp(str, "ABC", z) == 0);
    }

    b = qw_block_first(b);
    i = 0;
    do_test("search fwd 2", qw_block_search(b, &i, "XYZ", 3, 1) == NULL);

    b = qw_block_first(b);
    b = qw_block_move(b, 0, &i, 15);
    do_test("search bwd 1", (b = qw_block_search(b, &i, "345", 3, -1)) != NULL);
    if (b) {
        z = qw_block_get_str(b, i, str, 3);
        do_test("search bwd 1.2", strncmp(str, "345", z) == 0);
    }

    do_test("search bwd 2", (b = qw_block_search(b, &i, "ab", 2, -1)) != NULL);
    if (b) {
        z = qw_block_get_str(b, i, str, 2);
        do_test("search bwd 2.2", strncmp(str, "ab", z) == 0);
    }

    if (verbose)
        qw_block_dump(qw_block_first(b), stdout);

#if 0
    b = qw_blk_first(b);
    b = qw_blk_search(b, 0, &i, L"ABC", 3, 1);
    z = qw_blk_get(b, i, &c, 1);
    do_test("search is set after the last found char 1", c == L'D');
    b = qw_blk_search(b, i, &i, L"A", 1, -1);
    z = qw_blk_get(b, i, &c, 1);
    do_test("search is set after the last found char 2", c == L'B');

    b = qw_blk_first(b);

    ptr = L"this is a test";
    b = qw_blk_insert(b, 0, ptr, wcslen(ptr));

    ptr = L"abcdefghijklmnopqrstuvwxyz";

    i = qw_blk_i_and_not_e(b, 0, 1, ptr, NULL);
    z = qw_blk_get(b, 0, str, i + 1);
    str[i + 1] = L'\0';
    do_test("i_and_not_e 1", wcscmp(str, L"this") == 0);
    i += 2;
    q = qw_blk_i_and_not_e(b, i, 1, ptr, NULL);
    z = qw_blk_get(b, i, str, q - i + 1);
    str[q - i + 1] = L'\0';
    do_test("i_and_not_e 2", wcscmp(str, L"is") == 0);

    q = qw_blk_i_and_not_e(b, q, -1, ptr, NULL);
    do_test("i_and_not_e 3", i == q);

    i -= 2;
    q = qw_blk_i_and_not_e(b, i, -1, ptr, NULL);
    do_test("i_and_not_e 4", q == 0);

    i = qw_blk_i_and_not_e(b, q, 1, NULL, L" ") + 1;
    z = qw_blk_get(b, 0, str, i);
    str[i] = L'\0';
    do_test("i_and_not_e 5", wcscmp(str, L"this") == 0);
    q = qw_blk_i_and_not_e(b, i, 1, L" ", NULL) + 1;
    i = qw_blk_i_and_not_e(b, q, 1, NULL, L" ") + 1;
    z = qw_blk_get(b, q, str, i - q);
    str[i - q] = 0;
    do_test("i_and_not_e 6", wcscmp(str, L"is") == 0);

    qw_blk_destroy(qw_blk_first(b));
#endif
}

void test_journal(void)
{
    char str[STRLEN];
    qw_block *b;
    int z, i;
    qw_journal *j = NULL;
    char *ptr;

    /* journal */
    b = qw_block_new(NULL, NULL);

    ptr = "new string";
    j = qw_journal_new(1, b, 0, ptr, strlen(ptr), j);
    b = qw_journal_apply(b, j, 1);
    z = qw_block_get_str(qw_block_first(b), 0, str, STRLEN);
    do_test("jrnl 1 (insert 'new string')", strncmp(str, ptr, z) == 0);
    ptr = "\nmore";
    j = qw_journal_new(1, b, b->used, ptr, strlen(ptr), j);
    b = qw_journal_apply(b, j, 1);
    z = qw_block_get_str(qw_block_first(b), 0, str, STRLEN);
    do_test("jrnl 2 (insert 'more')", strncmp(str, "new string\nmore", z) == 0);
    qw_journal_mark_clean(j);

    j = qw_journal_new(0, b, 0, NULL, 3, j);
    b = qw_journal_apply(b, j, 1);
    z = qw_block_get_str(qw_block_first(b), 0, str, 7);
    do_test("jrnl 3 (delete 'new')", strncmp(str, " string", z) == 0);
    qw_journal_mark_clean(j);

    ptr = "incredible";
    j = qw_journal_new(1, b, 0, ptr, strlen(ptr), j);
    b = qw_journal_apply(b, j, 1);
    z = qw_block_get_str(qw_block_first(b), 0, str, STRLEN);
    do_test("jrnl 4 (insert again)", strncmp(str, "incredible string\nmore", z) == 0);

    /* undo */
    b = qw_journal_apply(b, j, 0);
    j = j->prev;
    z = qw_block_get_str(b, 0, str, 7);
    do_test("jrnl 5 (undo 4)", strncmp(str, " string", z) == 0);
    do_test("jrnl clean 1", j->clean == 1);

    b = qw_journal_apply(b, j, 0);
    j = j->prev;
    z = qw_block_get_str(b, 0, str, STRLEN);
    do_test("jrnl 6 (undo 3)", strncmp(str, "new string\nmore", z) == 0);
    do_test("jrnl clean 2", j->clean == 0);

    /* redo */
    j = j->next;
    b = qw_journal_apply(b, j, 1);
    z = qw_block_get_str(b, 0, str, 7);
    do_test("jrnl 7 (redo)", strncmp(str, " string", z) == 0);
    do_test("jrnl clean 3", j->clean == 1);

    j = j->next;
    b = qw_journal_apply(b, j, 1);
    z = qw_block_get_str(b, 0, str, STRLEN);
    do_test("jrnl 8 (redo)", strncmp(str, "incredible string\nmore", z) == 0);
    do_test("jrnl clean 4", j->clean == 0);
    qw_journal_mark_clean(j);

    /* undo again */
    b = qw_journal_apply(b, j, 0);
    j = j->prev;
    z = qw_block_get_str(qw_block_first(b), 0, str, 7);
    do_test("jrnl 9 (undo 4)", strncmp(str, " string", z) == 0);
    do_test("jrnl clean 5", j->clean == 0);

    b = qw_journal_apply(b, j, 0);
    j = j->prev;
    z = qw_block_get_str(qw_block_first(b), 0, str, STRLEN);
    do_test("jrnl 10 (undo 3)", strncmp(str, "new string\nmore", z) == 0);

    /* new journal entry (all undone is lost) */
    b = qw_block_move(qw_block_first(b), 0, &i, 3);
    j = qw_journal_new(0, b, 3, NULL, 7, j);
    b = qw_journal_apply(b, j, 1);
    z = qw_block_get_str(qw_block_first(b), 0, str, STRLEN);
    do_test("jrnl 11 (new branch, delete 'string')", strncmp(str, "new\nmore", z) == 0);

    b = qw_journal_apply(b, j, 0);
    j = j->prev;
    z = qw_block_get_str(qw_block_first(b), 0, str, STRLEN);
    do_test("jrnl 12 (undo)", strncmp(str, "new string\nmore", z) == 0);

    b = qw_journal_apply(b, j, 0);
    j = j->prev;
    z = qw_block_get_str(qw_block_first(b), 0, str, STRLEN);
    do_test("jrnl 13 (back to first content)", strncmp(str, "new string", z) == 0);

    b = qw_journal_apply(b, j, 0);
    z = qw_block_get_str(qw_block_first(b), 0, str, STRLEN);
    do_test("jrnl 14 (back to empty)", z == 0);

#if 0


    b = qw_blk_destroy(b);

    int cr = 0;

    if ((f = fopen("stress.txt", "rb")) != NULL) {
        i = 0;
        while ((z = qw_utf8_read(f, str, STRLEN, &cr))) {
            b = qw_blk_insert(b, i, str, z);
            b = qw_blk_move(b, i, z, &i);
        }

        fclose(f);
    }

    b = qw_blk_insert(qw_blk_first(b), 5, L"¡ÑÁ!", 4);

    if ((f = fopen("stress.out", "wb")) != NULL) {
        i = 0;
        b = qw_blk_first(b);
        while ((z = qw_blk_get(b, i, str, STRLEN))) {
            qw_utf8_write(f, str, z, cr);
            b = qw_blk_move(b, i, z, &i);
        }

        fclose(f);
    }

    b = qw_blk_destroy(b);

    /* fb */
    ptr = L"This is a rather long string to test the qw framebuffer engine,\nthat shall wrap by word. Thisisanonexistentverylongword that is not wrappable.";

    fb = qw_fb_new(23, 9);
    qw_fb_put(fb, ptr, wcslen(ptr));

//    print_fb(fb);
    sfb = qw_fb_new(60, 20);
    sfb->f = L'#';
    qw_fb_put(sfb, NULL, 0);
    qw_fb_mix(sfb, fb, 5, 5);

    if (verbose)
        print_fb(sfb);

    /* edit */
    e = qw_edit_new("stress.txt", NULL);

    if ((f = fopen(e->fn, "rb")) != NULL) {
        i = 0;
        while ((z = qw_utf8_read(f, str, STRLEN, &cr))) {
            e->b = qw_blk_insert(e->b, i, str, z);
            e->b = qw_blk_move(e->b, i, z, &i);
        }

        fclose(f);
    }

    off_t av = 0;
    qw_edit_paint(e, &av, sfb);
    qw_fb_mix(sfb, fb, 5, 5);

    if (verbose)
        print_fb(sfb);

    /* qw_blk_next_row */

    b = qw_blk_destroy(b);
    b = qw_blk_insert(b, 0, L"ABCDE 12345 ", 12);
    i = qw_blk_row_next(b, 0, 10);
    do_test("qw_blk_next_row 1", i == 6);
    i = qw_blk_row_next(b, i, 10);
    do_test("qw_blk_next_row 2 (set to EOF)", i == 6);
    b = qw_blk_first(b);
    b = qw_blk_insert(b, 0, L"ab ", 3);
    i = qw_blk_row_next(b, 0, 10);
    b = qw_blk_move(b, 0, i, &i);
    z = qw_blk_get(b, i, str, 5);
    do_test("qw_blk_next_row 3", wcsncmp(str, L"12345", z) == 0);
    b = qw_blk_first(b);
    i = qw_blk_row_next(b, 0, 5);
    b = qw_blk_move(b, 0, i, &i);
    z = qw_blk_get(b, i, str, 5);
    do_test("qw_blk_next_row 4", wcsncmp(str, L"ABCDE", z) == 0);
    b = qw_blk_first(b);
    b = qw_blk_insert(b, 0, L"C\n", 2);
    i = qw_blk_row_next(b, 0, 10);
    b = qw_blk_move(b, 0, i, &i);
    z = qw_blk_get(b, i, str, 5);
    do_test("qw_blk_next_row 5", wcsncmp(str, L"ab AB", z) == 0);
    b = qw_blk_first(b);
    b = qw_blk_insert(b, 0, L"A \n", 3);
    i = qw_blk_row_next(b, 0, 10);
    b = qw_blk_move(b, 0, i, &i);
    z = qw_blk_get(b, i, str, 2);
    do_test("qw_blk_next_row 6", wcsncmp(str, L"C\n", z) == 0);

    b = qw_blk_destroy(b);
    b = qw_blk_insert(b, 0, L"ABCDE\n12345\n", 12);
    b = qw_blk_move_bol(b, 2, &i);
    z = qw_blk_get(b, i, &c, 1);
    do_test("qw_blk_move_bol 1", i == 0 && c == L'A');
    b = qw_blk_move_bol(b, 5, &i);
    z = qw_blk_get(b, i, &c, 1);
    do_test("qw_blk_move_bol 2", i == 0 && c == L'A');
    b = qw_blk_move_bol(b, 6, &i);
    z = qw_blk_get(b, i, &c, 1);
    do_test("qw_blk_move_bol 3", i == 6 && c == L'1');
    b = qw_blk_move_bol(b, 11, &i);
    z = qw_blk_get(b, i, &c, 1);
    do_test("qw_blk_move_bol 4", i == 6 && c == L'1');
    b = qw_blk_insert(b, 0, L"\n", 1);
    b = qw_blk_move_bol(b, 0, &i);
    z = qw_blk_get(b, i, &c, 1);
    do_test("qw_blk_move_bol 5", i == 0 && c == L'\n');
#endif
}


void test_utf8(void)
{
    char str[STRLEN];
    qw_block *b;
    char *ptr;
    int z, i;

    b = qw_block_new(NULL, NULL);

    ptr = "aña—z";
    b = qw_block_insert_str(b, 0, ptr, strlen(ptr));
    i = 0;
    b = qw_utf8_move(b, &i, 1);
    do_test("utf8 fwd 1", i == 1);
    b = qw_utf8_move(b, &i, 1);
    do_test("utf8 fwd 2", i == 3);
    b = qw_utf8_move(b, &i, 1);
    do_test("utf8 fwd 3", i == 4);
    b = qw_utf8_move(b, &i, 1);
    do_test("utf8 fwd 4", i == 7);
    do_test("utf8 fwd 5", b->data[i] == 'z');
    do_test("utf8 fwd NULL on EOF", qw_utf8_move(b, &z, 1) == NULL);
    b = qw_utf8_move(b, &i, -1);
    do_test("utf8 bwd 1", i == 4);
    b = qw_utf8_move(b, &i, -1);
    do_test("utf8 fwd 2", i == 3);
    b = qw_utf8_move(b, &i, -1);
    do_test("utf8 fwd 3", i == 1);
    b = qw_utf8_move(b, &i, -1);
    do_test("utf8 fwd 4", i == 0);
    do_test("utf8 fwd NULL on BOF", qw_utf8_move(b, &z, -1) == NULL);

    b = qw_block_new(NULL, NULL);
    ptr = "\x61\xC3\xB1\x61\xE2\x80\x94\x7A\xEE\x80\x80\x5A\xEF\xBF\xBD\x57";
    b = qw_block_insert_str(b, 0, ptr, strlen(ptr));
    i = 0;

    b = qw_utf8_get_char_and_move(b, &i, str, &z);
    do_test("utf char 1", z == 1 && str[0] == 'a');
    do_test("utf decode 1", qw_utf8_decode(str, z) == 0x61);
    b = qw_utf8_get_char_and_move(b, &i, str, &z);
    do_test("utf char 2", z == 2);
    do_test("utf decode 2", qw_utf8_decode(str, z) == 0xf1);
    b = qw_utf8_get_char_and_move(b, &i, str, &z);
    do_test("utf char 3", z == 1 && str[0] == 'a');
    b = qw_utf8_get_char_and_move(b, &i, str, &z);
    do_test("utf char 4", z == 3 && memcmp("\xe2\x80\x94", str, 3) == 0);
    do_test("utf decode 3", qw_utf8_decode(str, z) == 0x2014);  /* m-dash */
    b = qw_utf8_get_char_and_move(b, &i, str, &z);
    do_test("utf char 5", z == 1 && str[0] == 'z');
    b = qw_utf8_get_char_and_move(b, &i, str, &z);
    do_test("utf char 6", z == 3 && memcmp("\xee\x80\x80", str, 3) == 0);
    do_test("utf decode 4", qw_utf8_decode(str, z) == 0xe000);  /* private use */
    b = qw_utf8_get_char_and_move(b, &i, str, &z);
    do_test("utf char 7", z == 1 && str[0] == 'Z');
    b = qw_utf8_get_char_and_move(b, &i, str, &z);
    do_test("utf char 8", z == 3 && memcmp("\xef\xbf\xbd", str, 3) == 0);
    do_test("utf decode 5", qw_utf8_decode(str, z) == 0xfffd);  /* replacement char */
    b = qw_utf8_get_char_and_move(b, &i, str, &z);
    do_test("utf char 9", z == 1 && str[0] == 'W');
    do_test("utf char at EOF", qw_utf8_get_char_and_move(b, &i, str, &z) == NULL);
    b = qw_utf8_move(b, &i, -1);
    b = qw_utf8_get_char_and_move(b, &i, str, &z);
    do_test("utf char 10", z == 1 && str[0] == 'W');
    b = qw_utf8_move(b, &i, -1);
    b = qw_utf8_move(b, &i, -1);
    b = qw_utf8_get_char_and_move(b, &i, str, &z);
    do_test("utf char 11", z == 3 && memcmp("\xef\xbf\xbd", str, 3) == 0);

    ptr = "hello";
    do_test("utf width 1", qw_utf8_str_width(ptr, strlen(ptr)) == 5);
    ptr = "ができ"; /* katakana */
    do_test("utf width 3", qw_utf8_str_width(ptr, strlen(ptr)) == 6);

    z = qw_utf8_encode('a', str);
    do_test("utf8 encode 1 byte", z == 1 && strncmp(str, "a", z) == 0);
    z = qw_utf8_encode(0xf1, str);
    do_test("utf8 encode 2 byte", z == 2 && strncmp(str, "\xc3\xb1", z) == 0);
    z = qw_utf8_encode(0x2014, str);
    do_test("utf8 encode 3 byte", z == 3 && strncmp(str, "\xe2\x80\x94", z) == 0);
    z = qw_utf8_encode(0xe000, str);
    do_test("utf8 encode 3.2 byte", z == 3 && strncmp(str, "\xee\x80\x80", z) == 0);
}


void test_view(void)
{
#if 0
    qw_block *b, *view;
    int i;
    char *ptr;
    int v = 0, sz;
    int crlf;
    int cx, cy;

    b = qw_block_new(NULL, NULL);
    i = 0;

    ptr = "First sentence";
    b = qw_block_insert_str_and_move(b, &i, ptr, strlen(ptr));
    sz = qw_view_row_size(b, 0, 20);
    do_test("row_size 1 (EOF)", sz == 14);

    ptr = "\n";
    b = qw_block_insert_str_and_move(b, &i, ptr, strlen(ptr));
    sz = qw_view_row_size(b, v, 20);
    do_test("row_size 2", sz == 15);

    b = qw_block_new(NULL, NULL);
    i = 0;
    ptr = "First sentence that is long enough to not fit in\nA single row\n"
          "Withaveryveryverylongwordwithnospacesthatwillnotfitanywhere\nYes it does.";
    b = qw_block_insert_str_and_move(b, &i, ptr, strlen(ptr));

    sz = qw_view_row_size(b, v, 31);
    do_test("row_size 3", sz == 28);

    v = 0;
    sz = qw_view_row_size(b, v, 40);
    do_test("row_size 4", sz == 38);
    b = qw_block_move(b, v, &v, sz);
    do_test("row_size 4.1", b->data[v] == 'n');
    sz = qw_view_row_size(b, v, 40);
    do_test("row_size 5", sz == 11);
    b = qw_block_move(b, v, &v, sz);
    do_test("row_size 5.1", b->data[v] == 'A');
    sz = qw_view_row_size(b, v, 40);
    do_test("row_size 6", sz == 13);
    b = qw_block_move(b, v, &v, sz);
    do_test("row_size 6.1", b->data[v] == 'W');
    sz = qw_view_row_size(b, v, 40); /* long! */
    do_test("row_size 7", sz == 40);
    b = qw_block_move(b, v, &v, sz);
    do_test("row_size 7.1", b->data[v] == 't');
    sz = qw_view_row_size(b, v, 40);
    do_test("row_size 8", sz == 20);
    b = qw_block_move(b, v, &v, sz);
    do_test("row_size 8.1", b->data[v] == 'Y');

    view = qw_view_create(qw_block_first(b), 0, 0, 40, 40, &cx, &cy);

    if (verbose)
        qw_block_dump(qw_block_first(view), stdout);

    b = qw_block_new(NULL, NULL);
    i = 0;
    ptr = ":ができ上がりました。"; /* katakana */
    b = qw_block_insert_str_and_move(b, &i, ptr, strlen(ptr));

    v = 0;
    sz = qw_view_row_size(b, v, 40);
    do_test("wide_chars 1", sz == 31);

    b = qw_file_load("LICENSE", &crlf);
    view = qw_view_create(b, 0, 58, 40, 20, &cx, &cy);
    do_test("view 1", view != NULL);
    do_test("view 2", cx == 19);
    do_test("view 3", cy == 1);

    if (verbose)
        qw_block_dump(qw_block_first(b), stdout);
    if (verbose)
        qw_block_dump(qw_block_first(view), stdout);
#endif
}


void test_synhi(void)
{
    qw_synhi *sh;

    sh = qw_synhi_new("sh", NULL);

    qw_synhi_add_extension(sh, ".sh");
    qw_synhi_add_extension(sh, ".bash");
    do_test("synhi exts 1", sh->n_extensions == 2);

    qw_synhi_add_token(sh, "if", QW_ATTR_WORD1);
    qw_synhi_add_token(sh, "then", QW_ATTR_WORD1);
    qw_synhi_add_token(sh, "else", QW_ATTR_WORD1);
    qw_synhi_add_token(sh, "elif", QW_ATTR_WORD1);
    qw_synhi_add_token(sh, "fi", QW_ATTR_WORD2);
    do_test("synhi tokens 1", sh->n_tokens == 5);

    qw_synhi_add_section(sh, "`", "`", NULL, QW_ATTR_LITERAL);
    qw_synhi_add_section(sh, "\"", "\"", "\\\"", QW_ATTR_LITERAL);
    qw_synhi_add_section(sh, "'", "'", "\\'", QW_ATTR_LITERAL);
    qw_synhi_add_section(sh, "#", "\n", NULL, QW_ATTR_COMMENT);

    qw_synhi_optimize(sh);
    do_test("synhi optimize 1", sh->sorted == 1);
    do_test("synhi optimize 2", strcmp(sh->tokens[0].token, "elif") == 0);
    qw_synhi_add_token(sh, "while", QW_ATTR_WORD3);
    do_test("synhi optimize 3", sh->sorted == 0);

    do_test("synhi find token 1", qw_synhi_find_token(sh, "fi") == QW_ATTR_WORD2);
    do_test("synhi optimize 4", sh->sorted == 1);
    do_test("synhi find token 2", qw_synhi_find_token(sh, "then") == QW_ATTR_WORD1);
    do_test("synhi find token 3", qw_synhi_find_token(sh, "buttface") == QW_ATTR_NONE);

    do_test("synhi not repeated", sh == qw_synhi_new("sh", sh));
}


void test_file(void)
{
    qw_block *b;
    int crlf;

    b = qw_file_load("nonexistent", &crlf);
    do_test("file load 1 (nonexistent)", b == NULL);
    b = qw_file_load("config.sh", &crlf);
    do_test("file load 2", b != NULL);
    do_test("file save 1", qw_file_save(b, "stress.out", crlf) != -1);
    do_test("file save 2", qw_file_save(b, "stress.out", 1) != -1);
    b = qw_file_load("stress.out", &crlf);
    do_test("file load 3", crlf == 1);
}


int main(int argc, char *argv[])
{
    int n;

    setlocale(LC_ALL, "");

    printf("qw %s stress tests\n", VERSION);

    for (n = 1; n < argc; n++) {
        if (strcmp(argv[n], "-b") == 0)
            _do_benchmarks = 1;
        if (strcmp(argv[n], "-v") == 0)
            verbose = 1;
    }

    test_block();
    test_journal();
    test_utf8();
    test_view();
    test_synhi();
    test_file();

    return test_summary();
}
