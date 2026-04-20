#include "logging/logging.h"
#include "protocol/protocol.h"
#include <db/trib.c>

/* compare colliding routes */

/*
 * 1. Highest degree of preference (local pref)
 * 2. Originated by local LS first
 * 3. Shortest ITAD-path
 * 4. Highest MED
 * 5. eTRIP over iTRIP (external peer route before internal peer route)
 * 6. Oldest route
 * 7. Highest LS ID
 */

/* 2^7 combinations = 128 */
#define DOMAIN_SIZE 128

void
generate_entry(entry_t *e, int comb)
{
    /* common */
    e->af = AF_E164;
    e->app_proto = APP_PROTO_SIP;
    e->prefix = "10";
    e->attrs.nexthop = "tel.arf20.com";
    e->attrs.itad_path = NULL;
    e->withdrawn = 0;

    /* comparable */
    e->attrs.local_pref = (int[]){ 100, 200 }[(comb >> 6) & 1];
    e->type = (int[]){ ENTRY_TYPE_TRIP, ENTRY_TYPE_STATIC }[(comb >> 5) & 1];
    e->attrs.itad_path_size = (int[]){ 3, 5 }[(comb >> 4) & 1];
    e->attrs.metric = (int[]){ 100, 200 }[(comb >> 3) & 1];
    e->learn_itad = (int[]){ 20, 10 }[(comb >> 2) & 1];
    e->time = (int[]){ 200, 100 }[(comb >> 1) & 1];
    e->learn_lsid = (int[]){ 10, 20 }[(comb >> 0) & 1];
}

void
print_entry(entry_t *e)
{
    printf("%d  %c    %ld    %d %d   %ld %d",
        e->attrs.local_pref, "SCT"[e->type], e->attrs.itad_path_size, e->attrs.metric, e->learn_itad, e->time, e->learn_lsid);
}

int
main()
{
    int local_itad = 10;

    logging_init(stdout, LOG_DEBUG);

    entry_t entries[DOMAIN_SIZE];
    for (int i = 0; i < DOMAIN_SIZE; i++)
        generate_entry(&entries[i], i);

    printf( "A                              | B                              | =\n"
            "pref orig path med itad age id | pref orig path med itad age id | a < b  i   < j   =   check\n");

    int res = 0;
    for (int i = 0; i < DOMAIN_SIZE; i++) {
        for (int j = 0; j < DOMAIN_SIZE; j++) {
            int r = entry_compare(&entries[i], &entries[j], local_itad);
            int check = r != (i < j);
            res |= check;

            print_entry(&entries[i]);
            printf(" | ");
            print_entry(&entries[j]);
            printf(" | %d      %-3d < %-3d = %d %s\n", r, i, j, i < j,(const char*[]){ "pass", "fail" }[check]);
        }
    }
            
    return res;
}

