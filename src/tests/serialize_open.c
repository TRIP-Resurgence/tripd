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

