#include <protocol/protocol.h>

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

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
        htons(CAPINFO_CODE_ROUTETYPE),
        htons(supported_routetypes_size * sizeof(capinfo_routetype_t))
    };

    capinfo_routetype_t *routetypes = (void*)&capinfo_routetypes->capinfo_val;
    for (size_t i = 0; i < supported_routetypes_size; i++) {
        routetypes[i].routetype_af = htons(supported_routetypes[i].routetype_af);
        routetypes[i].routetype_app_proto = htons(supported_routetypes[i].routetype_app_proto);
    }

    capinfo_t *capinfo_transmode = (void*)&capinfo_routetypes->capinfo_val +
        supported_routetypes_size * sizeof(capinfo_routetype_t);
    *capinfo_transmode = (capinfo_t){
        htons(CAPINFO_CODE_TRANSMODE),
        htons(sizeof(capinfo_transmode_t))
    };

    capinfo_transmode_t *capinfo_transmode_val = (void*)&capinfo_transmode->capinfo_val;
    *capinfo_transmode_val = (capinfo_transmode_t)htonl(CAPINFO_TRANS_SEND_RECV);

    *msg_open_opt = (msg_open_opt_t){
        htons(OPEN_OPT_TYPE_CAPABILITY_INFO),
        htons(2 * sizeof(capinfo_t) + (supported_routetypes_size * sizeof(capinfo_routetype_t))
            + sizeof(capinfo_transmode_t))
    };

    *msg_open = (msg_open_t){
        1, 0, htons(90), htonl(10), 0x0a000000, htons(sizeof(msg_open_opt_t) + ntohs(msg_open_opt->opt_len))
    };

    *msg = (msg_t){ htons(sizeof(msg_open_t) + ntohs(msg_open->open_opts_len)), MSG_TYPE_OPEN };

    /* OPEN serialization */
    int r =  new_msg_open(tbuff, 4096, 90, 10, 0x0a000000, supported_routetypes,
        supported_routetypes_size, CAPINFO_TRANS_SEND_RECV);

    /* check */
    printf("size %ld == %d = %s\n", sizeof(msg_t) + ntohs(msg->msg_len), r, strs[(sizeof(msg_t) + ntohs(msg->msg_len)) == r]);
    if (r != sizeof(msg_t) + ntohs(msg->msg_len))
        return 1;

    int fail = 0;
    for (int i = 0; i < sizeof(msg_t) + ntohs(msg->msg_len); i++) {
        if (i == 0) printf("msg:\n");
        if (i == sizeof(msg_t)) printf("msg_open:\n");
        if (i == sizeof(msg_t) + sizeof(msg_open_t)) printf("msg_open_opt:\n");
        if (i == sizeof(msg_t) + sizeof(msg_open_t) + sizeof(msg_open_opt_t)) printf("capinfo:\n");
        if (i == sizeof(msg_t) + sizeof(msg_open_t) + sizeof(msg_open_opt_t) + sizeof(capinfo_t)) printf("capinfo_routetype:\n");
        if (i == sizeof(msg_t) + sizeof(msg_open_t) + sizeof(msg_open_opt_t) + sizeof(capinfo_t) + sizeof(capinfo_routetype_t) + ntohs(capinfo_routetypes->capinfo_len)) printf("capinfo_transmode:\n");
        printf("  %.2x : %.2x == %.2x = %s\n", i, rbuff[i], tbuff[i], strs[rbuff[i] == tbuff[i]]);
        if (rbuff[i] != tbuff[i]) fail = 1;
    }

    return fail;
}

