#include "db/pib.h"
#include "db/trib.h"
#include "logging/logging.h"
#include "protocol/protocol.h"
#include <db/trib.c>

int
main()
{
    table_t src, dst;
    table_init(&src);
    table_init(&dst);

    entry_t e = {
        AF_E164, APP_PROTO_SIP, NULL, ENTRY_TYPE_TRIP, 0, 0, 0, 0, 0, NULL, 100, 100, NULL, 0, 0, 0
    };

    e.prefix = "1234";
    e.nexthop = "sip.arf20.com";
    e.itad_path = (uint32_t[3]){ 1, 2, 3 };
    e.itad_path_size = 3;
    trib_table_insert(&src, entry_clone(&e));

    e.prefix = "4321";
    e.nexthop = "sip.arf20.com";
    e.itad_path = (uint32_t[3]){ 1, 3, 2 };
    e.itad_path_size = 3;
    trib_table_insert(&src, entry_clone(&e));


    acl_t acl = { "test", malloc(2 * sizeof(acl_entry_t)), 0, 2 };
    acl_insert(&acl, 0, "_1234");

    routemap_t map = { "test", malloc(2 * sizeof(routemap_statement_t)), 0, 2 };
    routemap_statement_t *s = routemap_statement_new(&map, 10, 0);
    routemap_matcher_t *m = routemap_statement_matcher_new(s, AF_E164);
    routemap_matcher_insert(m, &acl);
    routemap_action_t a = { ROUTEMAP_SET_LOCALPREF, 200, NULL, NULL };
    routemap_statement_insert_action(s, &a);

    apply_policy(&dst, &src, 10, &map);

    if (dst.size != 1) {
        printf("wrong size: %ld != 1\n", dst.size);
        return 1;
    }

    if (strcmp(dst.table[0]->prefix, "1234") != 0) {
        printf("wrong entry: %s != 1234\n", dst.table[0]->prefix);
        return 1;
    }

    if (dst.table[0]->local_pref != 200) {
        printf("wrong set: %d != 100\n", dst.table[0]->local_pref);
        return 1;
    }

    printf("pass\n");

    return 0;
}

