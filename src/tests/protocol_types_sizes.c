#include <protocol/protocol.h>

#include <stdio.h>

#define CHECK_SIZE(name, type, len) \
    printf("sizeof %-25s %2d == %-2ld = %s\n", name, len, sizeof(type), strs[sizeof(type) == len]); \
    if (sizeof(type) != len) return 1;

int
main()
{
    const char *strs[] = { "fail", "pass" };

    /* sizes */
    CHECK_SIZE("msg_t", msg_t, 3);
    CHECK_SIZE("msg_open_t", msg_open_t, 14);
    CHECK_SIZE("msg_open_opt_t", msg_open_opt_t, 4);
    CHECK_SIZE("capinfo_t", capinfo_t, 4);
    CHECK_SIZE("capinfo_routetype_t", capinfo_routetype_t, 4);
    CHECK_SIZE("capinfo_transmode_t", capinfo_transmode_t, 4);
    CHECK_SIZE("msg_update_attr_t", msg_update_attr_t, 4);
    CHECK_SIZE("msg_update_attr_lsencap_t", msg_update_attr_lsencap_t, 12);
    CHECK_SIZE("route_t", route_t, 6);
    CHECK_SIZE("attr_nexthopserver_t", attr_nexthopserver_t, 6);
    CHECK_SIZE("itadpath_t", itadpath_t, 2);
    CHECK_SIZE("community_t", community_t, 8);
    CHECK_SIZE("msg_notif_t", msg_notif_t, 2);

    return 0;
}

