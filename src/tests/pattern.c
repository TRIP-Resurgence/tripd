#include <db/pib.c>

int
main()
{
    const char *tests[][3] = {
        { "_273XXXX",   "2732000",  (void*)1 },
        { "_273XXXX",   "273200",   (void*)0 },
        { "_273XXX",    "2732000",  (void*)0 },
        { "_XXXX420",   "4321420",  (void*)1 },
        { "_2.",        "2000",     (void*)1 },
        { "_273.",      "273",      (void*)1 },
    };

    int fail = 0;
    for (int i = 0; i < sizeof(tests)/(3*sizeof(void*)); i++) {
        int r = pattern_check(tests[i][0], tests[i][1]);
        printf("%-20s %-20s -> %d == %ld %s\n", tests[i][0], tests[i][1], r,
            (long)tests[i][2], (const char*[]){ "fail", "pass" }[r == (long)tests[i][2]]);
        fail |= (r !=  (long long)tests[i][2]);
    }

    return fail;
}

