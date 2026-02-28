#include <protocol/protocol.h>

#include <stdio.h>
#include <string.h>

#define CHECK_SIZE(name, type, len) \
    printf("sizeof %-25s %2d == %-2ld = %s\n", name, len, sizeof(type), strs[sizeof(type) == len]); \
    if (sizeof(type) != len) return 1;
    

int
main()
{
    unsigned char tbuff[4096];   /* test buffer */
    unsigned char rbuff[4096];   /* reference buffer */

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


    /* OPEN reference */
    msg_t *msg = (void*)rbuff;

    msg_open_t *msg_open = (void*)msg + sizeof(msg_t);

    msg_open_opt_t *msg_open_opt = (void*)msg_open + sizeof(msg_open_t);

    capinfo_t *capinfo_routetypes = (void*)msg_open_opt + sizeof(msg_open_opt_t);
    *capinfo_routetypes = (capinfo_t){
        CAPINFO_CODE_ROUTETYPE,
        supported_routetypes_size * sizeof(capinfo_routetype_t)
    };

    memcpy((void*)capinfo_routetypes + sizeof(capinfo_t), supported_routetypes,
        capinfo_routetypes->capinfo_len);

    capinfo_t *capinfo_transmode = (void*)capinfo_routetypes +
        sizeof(msg_open_opt_t) + capinfo_routetypes->capinfo_len;
    *capinfo_transmode = (capinfo_t){
        CAPINFO_CODE_TRANSMODE,
        sizeof(capinfo_transmode_t)
    };

    capinfo_transmode_t *capinfo_transmode_val = (void*)capinfo_transmode + sizeof(capinfo_t);
    *capinfo_transmode_val = (capinfo_transmode_t)CAPINFO_TRANS_SEND_RECV;

    *msg_open_opt = (msg_open_opt_t){
        OPEN_OPT_TYPE_CAPABILITY_INFO,
        2 * sizeof(capinfo_t) + capinfo_routetypes->capinfo_len +
            capinfo_transmode->capinfo_len
    };

    *msg_open = (msg_open_t){
        1, 0, 90, 10, 0x0a000000, sizeof(msg_open_opt_t) + msg_open_opt->opt_len
    };

    *msg = (msg_t){ sizeof(msg_open_t) + msg_open->open_opts_len, MSG_TYPE_OPEN };

    /* OPEN serialization */
    int r =  new_msg_open(tbuff, 4096, 90, 10, 0x0a000000, supported_routetypes,
        supported_routetypes_size, CAPINFO_TRANS_SEND_RECV);

    /* check */
    if (r != sizeof(msg_t) + msg->msg_len)
        return 1;

    int fail = 0;
    for (int i = 0; i < sizeof(msg_t) + msg->msg_len; i++) {
        printf("%.2x : %.2x == %.2x = %s\n", i, rbuff[i], tbuff[i], strs[rbuff[i] == tbuff[i]]);
        if (rbuff[i] != tbuff[i]) fail = 1;
    }

    return fail;
}

