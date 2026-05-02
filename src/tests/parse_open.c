#include <protocol/protocol.h>

#include <stdio.h>
#include <string.h>

int
main()
{
    unsigned char tbuff[4096];   /* test buffer */
    unsigned char rbuff[4096];   /* reference buffer */

    const char *strs[] = { "fail", "pass" };

    /* OPEN reference */
    msg_t *msg = (void*)rbuff;

    msg_open_t *msg_open = (void*)msg + sizeof(msg_t);

    msg_open_opt_t *msg_open_opt = (void*)msg_open + sizeof(msg_open_t);

    capinfo_t *capinfo_routetypes = (void*)msg_open_opt + sizeof(msg_open_opt_t);
    *capinfo_routetypes = (capinfo_t){
        CAPINFO_CODE_ROUTETYPE,
        supported_routetypes_size * sizeof(capinfo_routetype_t)
    };

    capinfo_routetype_t *routetypes = (void*)&capinfo_routetypes->capinfo_val;
    for (size_t i = 0; i < supported_routetypes_size; i++) {
        routetypes[i].routetype_af = supported_routetypes[i].routetype_af;
        routetypes[i].routetype_app_proto = supported_routetypes[i].routetype_app_proto;
    }

    capinfo_t *capinfo_transmode = (void*)&capinfo_routetypes->capinfo_val +
        supported_routetypes_size * sizeof(capinfo_routetype_t);
    *capinfo_transmode = (capinfo_t){
        CAPINFO_CODE_TRANSMODE,
        sizeof(capinfo_transmode_t)
    };

    capinfo_transmode_t *capinfo_transmode_val = (void*)&capinfo_transmode->capinfo_val;
    *capinfo_transmode_val = (capinfo_transmode_t)CAPINFO_TRANS_SEND_RECV;

    *msg_open_opt = (msg_open_opt_t){
        OPEN_OPT_TYPE_CAPABILITY_INFO,
        2 * sizeof(capinfo_t) + (supported_routetypes_size * sizeof(capinfo_routetype_t))
            + sizeof(capinfo_transmode_t)
    };

    *msg_open = (msg_open_t){
        1, 0, 90, 10, 0x0a000000, sizeof(msg_open_opt_t) + msg_open_opt->opt_len
    };

    *msg = (msg_t){ sizeof(msg_open_t) + msg_open->open_opts_len, MSG_TYPE_OPEN };

    /* OPEN serialization */
    int r =  new_msg_open(tbuff, 4096, 90, 10, 0x0a000000, supported_routetypes,
        supported_routetypes_size, CAPINFO_TRANS_SEND_RECV);

    msg_t *msg_ = NULL;
    msg_open_t *msg_open_ = NULL;
    msg_open_opt_t *msg_open_opt_ = NULL;
    capinfo_t *capinfo_ = NULL;
    capinfo_transmode_t *capinfo_transmode_ = NULL;
    int s = parse_msg(tbuff, 4096, &msg);
    s += parse_msg_open(tbuff + s, 4096 - s, &msg_open_);
    s += parse_msg_open_opt(tbuff + s, 4096 - s, &msg_open_opt_);
    s += parse_capinfo(tbuff + s, 4096 - s, &capinfo_);

    size_t routetypes_toread = capinfo_->capinfo_len;
    while (routetypes_toread) {
        capinfo_routetype_t *routetype = NULL;
        int r = parse_capinfo_routetype(tbuff + s, 4096 - s, &routetype);

        routetypes_toread -= r;
        s += r;
    }

    s += parse_capinfo(tbuff + s, 4096 - s, &capinfo_);
    s += parse_capinfo_transmode(tbuff + s, 4096 - s, &capinfo_transmode_);

    /* check */
    printf("size %ld == %d = %s\n", sizeof(msg_t) + msg->msg_len, r, strs[(sizeof(msg_t) + msg->msg_len) == r]);
    if (r != sizeof(msg_t) + msg->msg_len)
        return 1;

    int fail = 0;
    for (int i = 0; i < sizeof(msg_t) + msg->msg_len; i++) {
        if (i == 0) printf("msg:\n");
        if (i == sizeof(msg_t)) printf("msg_open:\n");
        if (i == sizeof(msg_t) + sizeof(msg_open_t)) printf("msg_open_opt:\n");
        if (i == sizeof(msg_t) + sizeof(msg_open_t) + sizeof(msg_open_opt_t)) printf("capinfo:\n");
        if (i == sizeof(msg_t) + sizeof(msg_open_t) + sizeof(msg_open_opt_t) + sizeof(capinfo_t)) printf("capinfo_routetype:\n");
        if (i == sizeof(msg_t) + sizeof(msg_open_t) + sizeof(msg_open_opt_t) + sizeof(capinfo_t) + sizeof(capinfo_routetype_t) + capinfo_routetypes->capinfo_len) printf("capinfo_transmode:\n");
        printf("  %.2x : %.2x == %.2x = %s\n", i, rbuff[i], tbuff[i], strs[rbuff[i] == tbuff[i]]);
        if (rbuff[i] != tbuff[i]) fail = 1;
    }

    return fail;
}

