#include <protocol/protocol.h>
#include <logging/logging.h>

#include <stdio.h>
#include <string.h>

#define _COMPONENT_ "test"

const char *attr_strs[] = {
    "nil",
    "WithdrawnRoutes",
    "ReachableRoutes",
    "NextHopServer",
    "AdvertisementPath",
    "RoutedPath",
    "AtomicAggregate",
    "LocalPreference",
    "MultiExitDisc",
    "Communities",
    "ITADTopology",
    "ConvertedRoute"
};

int
main()
{
    char tbuff_array[12][4096];  /* test buffer */
    char rbuff_array[12][4096];  /* reference buffer */
    int sizes[12];

    char *tbuffs[12], *rbuffs[12];
    for (int i = 0; i < 12; i++) {
        tbuffs[i] = tbuff_array[i];
        rbuffs[i] = rbuff_array[i];
    }


    const char *strs[] = { "fail", "pass" };

    int bidx = 1;
    /* attribute reference */
    msg_update_attr_t *withdrawnroutes = (void*)rbuffs[bidx++];
    route_t *route = (void*)&withdrawnroutes->attr_val;
    route->route_af = AF_E164;
    route->route_app_proto = APP_PROTO_SIP;
    route->route_len = 5;
    strcpy((void*)&route->route_addr, "69420");
    withdrawnroutes->attr_flags = ATTR_FLAG_WELL_KNOWN;
    withdrawnroutes->attr_type = ATTR_TYPE_WITHDRAWNROUTES;
    withdrawnroutes->attr_len = sizeof(route_t) + route->route_len;

    msg_update_attr_t *reachroutes = (void*)rbuffs[bidx++];
    route = (void*)&reachroutes->attr_val;
    route->route_af = AF_E164;
    route->route_app_proto = APP_PROTO_SIP;
    route->route_len = 5;
    strcpy((void*)&route->route_addr, "42069");
    reachroutes->attr_flags = ATTR_FLAG_WELL_KNOWN;
    reachroutes->attr_type = ATTR_TYPE_REACHABLEROUTES;
    reachroutes->attr_len = sizeof(route_t) + route->route_len;

    msg_update_attr_t *nexthopa = (void*)rbuffs[bidx++];
    attr_nexthopserver_t *nexthop = (void*)&nexthopa->attr_val;
    nexthop->nexthopserver_itad = 69;
    nexthop->nexthopserver_serverlen = 9;
    strcpy((void*)&nexthop->nexthopserver_server, "deez.nuts");
    nexthopa->attr_flags = ATTR_FLAG_WELL_KNOWN;
    nexthopa->attr_type = ATTR_TYPE_NEXTHOPSERVER;
    nexthopa->attr_len = sizeof(attr_nexthopserver_t) + nexthop->nexthopserver_serverlen;

    msg_update_attr_t *advertpatha = (void*)rbuffs[bidx++];
    attr_advertisementpath_t *advertpath = (void*)&advertpatha->attr_val;
    advertpath->itadpath_type = ITADPATH_TYPE_AP_SEQUENCE;
    advertpath->itadpath_len = 2;
    advertpath->itadpath_segs[0] = 69;
    advertpath->itadpath_segs[1] = 420;
    advertpatha->attr_flags = ATTR_FLAG_WELL_KNOWN;
    advertpatha->attr_type = ATTR_TYPE_ADVERTISEMENTPATH;
    advertpatha->attr_len = sizeof(attr_advertisementpath_t) + (sizeof(uint32_t) * advertpath->itadpath_len);

    msg_update_attr_t *routedpatha = (void*)rbuffs[bidx++];
    attr_routedpath_t *routedpath = (void*)&routedpatha->attr_val;
    routedpath->itadpath_type = ITADPATH_TYPE_AP_SEQUENCE;
    routedpath->itadpath_len = 2;
    routedpath->itadpath_segs[0] = 420;
    routedpath->itadpath_segs[1] = 69;
    routedpatha->attr_flags = ATTR_FLAG_WELL_KNOWN;
    routedpatha->attr_type = ATTR_TYPE_ROUTEDPATH;
    routedpatha->attr_len = sizeof(attr_routedpath_t) + (sizeof(uint32_t) * routedpath->itadpath_len);

    /* UPDATE serialization */
    bidx = 1;
    int r = 0;
    char tmp[256];

    route = (void*)tmp;
    route->route_af = AF_E164;
    route->route_app_proto = APP_PROTO_SIP;
    route->route_len = 5;
    strcpy((void*)&route->route_addr, "69420");
    r = new_attr_withdrawnroutes(tbuffs[bidx], 4096, 0, 0, 0, route, sizeof(route_t) + route->route_len);
    if (r < 0) {
        printf("error: withdrawnroutes\n");
        return 1;
    }
    sizes[bidx++] = r;

    route = (void*)tmp;
    route->route_af = AF_E164;
    route->route_app_proto = APP_PROTO_SIP;
    route->route_len = 5;
    strcpy((void*)&route->route_addr, "42069");
    r = new_attr_reachableroutes(tbuffs[bidx], 4096, 0, 0, 0, route, sizeof(route_t) + route->route_len);
    if (r < 0) {
        printf("error: reachableroutes\n");
        return 1;
    }
    sizes[bidx++] = r;

    r = new_attr_nexthopserver(tbuffs[bidx], 4096, 69, "deez.nuts");
    if (r < 0) {
        printf("error: nexthopserver\n");
        return 1;
    }
    sizes[bidx++] = r;

    itadpath_t *path = (void*)tmp;
    path->itadpath_type = ITADPATH_TYPE_AP_SEQUENCE;
    path->itadpath_len = 2;
    path->itadpath_segs[0] = 69;
    path->itadpath_segs[1] = 420;
    r = new_attr_advertisementpath(tbuffs[bidx], 4096, path);
    if (r < 0) {
        printf("error: advertisementpath\n");
        return 1;
    }
    sizes[bidx++] = r;

    path->itadpath_type = ITADPATH_TYPE_AP_SEQUENCE;
    path->itadpath_len = 2;
    path->itadpath_segs[0] = 420;
    path->itadpath_segs[1] = 69;
    r = new_attr_routedpath(tbuffs[bidx], 4096, path);
    if (r < 0) {
        printf("error: routedpath\n");
        return 1;
    }
    sizes[bidx++] = r;

    /* check */
    int fail = 0;
    for (int i = 1; i < 12; i++) {
        int pass = memcmp(&tbuffs[i][0], &rbuffs[i][0], sizes[i]) == 0;
        printf("%s[%d]: %s\n", attr_strs[i], sizes[i], strs[pass]);
        if (!pass) {
            printf("  test ref  diff\n");
            for (int j = 0; j < sizes[i]; j++) {
                printf("  %.2x   %.2x   %s\n", (unsigned char)tbuffs[i][j], (unsigned char)rbuffs[i][j], strs[tbuffs[i][j] == rbuffs[i][j]]);
            }
        }
    }

    return fail;
}

