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

    msg_notif_t *msg_notif = (void*)msg + sizeof(msg_t);

    memcpy((void*)msg_notif + sizeof(msg_notif_t), "asdf", 4);

    *msg_notif = (msg_notif_t){
        NOTIF_CODE_ERROR_OPEN, NOTIF_SUBCODE_OPEN_UNSUP_VERSION
    };

    *msg = (msg_t){ sizeof(msg_notif_t) + 4, MSG_TYPE_NOTIFICATION };

    /* OPEN serialization */
    int r = new_msg_notif(tbuff, 4096, NOTIF_CODE_ERROR_OPEN,
        NOTIF_SUBCODE_OPEN_UNSUP_VERSION, 4, "asdf");

    /* check */
    printf("size %ld == %d = %s\n", sizeof(msg_t) + msg->msg_len, r, strs[(sizeof(msg_t) + msg->msg_len) == r]);
    if (r != sizeof(msg_t) + msg->msg_len)
        return 1;

    int fail = 0;
    for (int i = 0; i < sizeof(msg_t) + msg->msg_len; i++) {
        if (i == 0) printf("msg:\n");
        if (i == sizeof(msg_t)) printf("msg_notif:\n");
        if (i == sizeof(msg_t) + sizeof(msg_notif_t)) printf("data:\n");
        printf("  %.2x : %.2x == %.2x = %s\n", i, rbuff[i], tbuff[i], strs[rbuff[i] == tbuff[i]]);
        if (rbuff[i] != tbuff[i]) fail = 1;
    }

    return fail;
}

