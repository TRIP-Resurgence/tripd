/*

    trip: Modern TRIP LS implementation
    Copyright (C) 2025 arf20 (Ángel Ruiz Fernandez)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    protocol.c: protocol serialization/deserialzation, thread safe

*/

/** \file
 * 
 * Protocol serialization and deserialization implementation
 */

#include "protocol.h"

#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>


/* symbols */


const char *msg_type_strs[] = {
    "nil",
    "OPEN",
    "UPDATE",
    "NOTIFICATION",
    "KEEPALIVE"
};

const char *open_opt_type_strs[] = {
    "nil",
    "OPEN_OPT_TYPE_CAPABILITY_INFO"
};


const char *capinfo_code_strs[] = {
    "nil",
    "CAPINFO_CODE_ROUTETYPE",
    "CAPINFO_CODE_TRANSMODE"
};


const char *capinfo_transmode_strs[] = {
    "nil",
    "duplex",
    "send-only",
    "receive-only"
};

const char *
flags_str(uint8_t flags)
{
    static char flags_str[256];

    static char *flags_strs[] = {
        "Well-Known",
        "Independent Transitive",
        "Dependent",
        "Partial",
        "Link-State Encapsulated"
    };

    flags_str[0] = 0;
    for (uint8_t i = 7; i >= 3; i >>= 1) {
        if (!((flags >> i) & 1))
            continue;
        if (*flags_str)
            strcat(flags_str, ", ");
        strcat(flags_str, flags_strs[7 - i]);
    }

    return flags_str;
}

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
    "ConvertedRoute",
    /** RFC5115 */
    "ResourcePriority",
    /** RFC5140 */
    "TotalCircuitCapacity",
    "AvailableCircuits",
    "CallSuccess",
    "E164Prefix",
    "PentaDecPrefix",
    "DecimalPrefix",
    "TrunkGroup",
    "Carrier"
};

const char *af_strs[] = {
    "nil",
    "decimal",
    "pentadecimal",
    "E.164",
    "trunkgroup",
    "carrier"
};


const char *notif_code_strs[] = {
    "nil",
    "message",
    "OPEN",
    "UPDATE",
    "expired",
    "state",
    "cease"
};

const char *notif_subcode_msg_strs[] = {
    "nil",
    "bad length",
    "bad type"
};

const char *notif_subcode_open_strs[] = {
    "nil",
    "unsupported version",
    "bad ITAD",
    "bad ID",
    "unsupported option",
    "bad hold",
    "unsupported capability",
    "transmission mode mismatch"
};

const char *notif_subcode_update_strs[] = {
    "nil",
    "malformed attribute",
    "unknown well-known attribute",
    "missing well-known flag",
    "bad attribute flag",
    "bad attribute length",
    "invalid attribute"
};

const char **notif_code_subcodes_strs[] = {
    NULL,
    notif_subcode_msg_strs,
    notif_subcode_open_strs,
    notif_subcode_update_strs,
    NULL,
    NULL,
    NULL
};



const char *
app_proto_str(int app_proto)
{
    static const char *app_proto_strs[] = {
        "SIP",
        "H.323-H.225.0-Q.931",
        "H.323-H.225.0-RAS",
        "H.323-H.225.0-Annex-G",
    };

    if (app_proto >= APP_PROTO_SIP && app_proto <= APP_PROTO_H323_225_0_ANNEXG)
        return app_proto_strs[app_proto - 1];
    else if (app_proto == APP_PROTO_IAX2)
        return "IAX2";
    else return "invalid";
}


const char *runtime_error_strs[] = {
    "no error",
    /* serialization and deserialization */
    "invalid buffer",
    "not enough buffer",
    "hold time must be 0 or at least 3 s",
    "ITAD must not be 0 (reserved)",
    "invalid NOTIFICATION error code",
    "invalid NOTIFICATION error subcode",
    /* deserialization specific */
    "passed an incomplete message, recv more",
    "invalid message type",
    "unsupported protocol version",
    "unsupported OPEN option param",
    "unsupported capability info code",
    "unsupported address family",
    "unsupported application protocol",
    "invalid send/recv capability",
    "unsupported attribute type",
    "attribute should have well-known",
    "attribute must be link-state encapsulated",
    "unsupported ITAD path type",
    "reserved community ITAD with bad ID"
};

const capinfo_routetype_t supported_routetypes[] = {
    { AF_DECIMAL,   APP_PROTO_SIP },
    { AF_DECIMAL,   APP_PROTO_H323_225_0_Q931 },
    { AF_DECIMAL,   APP_PROTO_H323_225_0_RAS },
    { AF_DECIMAL,   APP_PROTO_H323_225_0_ANNEXG },
    { AF_DECIMAL,   APP_PROTO_IAX2 },

    { AF_E164,      APP_PROTO_SIP },
    { AF_E164,      APP_PROTO_H323_225_0_Q931 },
    { AF_E164,      APP_PROTO_H323_225_0_RAS },
    { AF_E164,      APP_PROTO_H323_225_0_ANNEXG },
    { AF_E164,      APP_PROTO_IAX2 },
};

const size_t supported_routetypes_size = sizeof(supported_routetypes) /
    sizeof(capinfo_routetype_t);


/* utils */

#define CHECK_HOLD(x) ((x == 0) || (x < 3))
#define CHECK_AF(x) ((x < AF_DECIMAL) || (x > AF_CARRIER))
#define CHECK_APP_PROTO(x) (((x < APP_PROTO_SIP) \
    || (x > APP_PROTO_H323_225_0_ANNEXG)) && \
    (x != APP_PROTO_IAX2))
#define CHECK_ITADPATH_TYPE(x) ((x < ITADPATH_TYPE_AP_SET) || \
    (x > ITADPATH_TYPE_AP_SEQUENCE))

int
check_notif_error_code_subcode(uint8_t code, uint8_t subcode)
{
    switch (code) {
    case NOTIF_CODE_ERROR_MSG:
        if (subcode < NOTIF_SUBCODE_MSG_BAD_LEN ||
            subcode > NOTIF_SUBCODE_MSG_BAD_TYPE)
        {
            return ERROR_NOTIF_ERROR_SUBCODE;
        }
    break;
    case NOTIF_CODE_ERROR_OPEN:
        if (subcode < NOTIF_SUBCODE_OPEN_UNSUP_VERSION ||
            subcode > NOTIF_SUBCODE_OPEN_CAP_MISMATCH)
        {
            return ERROR_NOTIF_ERROR_SUBCODE;
        }
    break;
    case NOTIF_CODE_ERROR_UPDATE:
        if (subcode < NOTIF_SUBCODE_UPDATE_MALFORM_ATTR ||
            subcode > NOTIF_SUBCODE_UPDATE_INVAL_ATTR)
        {
            return ERROR_NOTIF_ERROR_SUBCODE;
        }
    break;
    case NOTIF_CODE_ERROR_EXPIRED:  /* RFC does not define subcodes for these */
    case NOTIF_CODE_ERROR_STATE:
    case NOTIF_CODE_CEASE:
    break;
    default: return ERROR_NOTIF_ERROR_CODE;
    }

    return 0;
}


/* ============================ SERIALIZATION =============================== */


/* message OPEN */

runtime_error_t
new_msg_open(void *buff, size_t len,
    uint16_t hold, uint32_t itad, uint32_t id,
    const capinfo_routetype_t *capinfo_routetypes, size_t routetypes_size,
    capinfo_transmode_t capinfo_transmode)
{
    if (!buff)
        return ERROR_BUFF;

    size_t capinfo_routetypes_size = 0, capinfo_transmode_size = 0,
        opt_size = 0;

    if (capinfo_routetypes)
        capinfo_routetypes_size = sizeof(capinfo_t) +
            (sizeof(capinfo_routetype_t) * routetypes_size);
    if (capinfo_transmode != CAPINFO_TRANS_NULL)
        capinfo_transmode_size += sizeof(capinfo_t) +
            sizeof(capinfo_transmode_t);
    if (capinfo_routetypes || capinfo_transmode != CAPINFO_TRANS_NULL)
        opt_size = sizeof(msg_open_opt_t) + capinfo_routetypes_size +
            capinfo_transmode_size;
    size_t msg_size = sizeof(msg_t) + sizeof(msg_open_t) + opt_size;

    if (len < msg_size)
        return ERROR_BUFFLEN;

    if (hold != 0 && hold < 3)
        return ERROR_HOLD;

    if (itad == 0)
        return ERROR_ITAD;

    /* MSG {
     *   OPEN [{ OPT_CAPINFO { [CAPINFO_ROUTETYPE] | [CAPINFO_TRANS] } }]
     * }
     */
    msg_t *msg = buff;
    msg->msg_len = htons(msg_size - sizeof(msg_t));
    msg->msg_type = MSG_TYPE_OPEN;

    msg_open_t *msg_open = (msg_open_t*)msg->msg_val;
    msg_open->open_ver = 1;
    msg_open->open_reserved = 0;
    msg_open->open_hold = htons(hold);
    msg_open->open_itad = htonl(itad);
    msg_open->open_id = id; /* already in network order */
    msg_open->open_opts_len = htons(msg_size - sizeof(msg_t) - sizeof(msg_open_t));

    void *end = msg_open->open_opts;
    if (capinfo_routetypes || capinfo_transmode != CAPINFO_TRANS_NULL)  {
        msg_open_opt_t *opt = end;
        opt->opt_type = htons(OPEN_OPT_TYPE_CAPABILITY_INFO);
        opt->opt_len = htons(opt_size - sizeof(msg_open_opt_t));
        end = &opt->opt_val;
    }

    if (capinfo_routetypes) {
        capinfo_t *capinfo = end;
        capinfo->capinfo_code = htons(CAPINFO_CODE_ROUTETYPE);
        capinfo->capinfo_len = htons(sizeof(capinfo_routetype_t) * routetypes_size);

        capinfo_routetype_t *routetypes = (void*)&capinfo->capinfo_val;
        for (size_t i = 0; i < routetypes_size; i++) {
            routetypes[i].routetype_af = htons(capinfo_routetypes[i].routetype_af);
            routetypes[i].routetype_app_proto = htons(capinfo_routetypes[i].routetype_app_proto);
        }

        end += capinfo_routetypes_size;
    }

    if (capinfo_transmode != CAPINFO_TRANS_NULL) {
        capinfo_t *capinfo = end;
        capinfo->capinfo_code = htons(CAPINFO_CODE_TRANSMODE);
        capinfo->capinfo_len = htons(sizeof(capinfo_transmode_t));
        *(capinfo_transmode_t*)&capinfo->capinfo_val = htonl(capinfo_transmode);
        end += capinfo_transmode_size;
    }

    return msg_size;
}


/* message UPDATE
 * takes a list of attrs_size pointers to attributes that may be of type
 * msg_update_attr_t or msg_update_attr_lsencap_t already serialized
 */

runtime_error_t
new_msg_update(void *buff, size_t len,
    const msg_update_attr_t **attrs, size_t attrs_size)
{
    if (!buff)
        return ERROR_BUFF;

    if (len < sizeof(msg_t))
        return ERROR_BUFFLEN;

    msg_t *msg = buff;
    msg->msg_len = 0;
    msg->msg_type = MSG_TYPE_UPDATE;

    void *end = &msg->msg_val;
    for (size_t i = 0; i < attrs_size; i++) {
        const msg_update_attr_t *attr = attrs[i];
        size_t attrsize = IS_ATTR_FLAG_LSENCAP(attr->attr_flags) ?
            (sizeof(msg_update_attr_lsencap_t) + ntohs(attr->attr_len)) :
            (sizeof(msg_update_attr_t) + ntohs(attr->attr_len));

        if (len < end - buff + attrsize)
            return ERROR_BUFFLEN;

        memcpy(end, attr, attrsize);
        msg->msg_len += attrsize;
        end += attrsize;
    }
    
    msg->msg_len = htons(msg->msg_len);

    return end - buff;
}

runtime_error_t
new_route(void *buff, size_t len, uint16_t af, uint16_t app_proto,
    const char *addr)
{
    if (!buff)
        return ERROR_BUFF;

    size_t addr_len = strlen(addr);
    size_t route_size = sizeof(route_t) + addr_len;

    if (len < route_size)
        return ERROR_BUFFLEN;

    route_t *route = buff;
    route->route_af = htons(af);
    route->route_app_proto = htons(app_proto);
    route->route_len = htons(addr_len);
    memcpy(&route->route_addr, addr, addr_len);

    return route_size;
}

runtime_error_t
new_attr_withdrawnroutes(void *buff, size_t len,
    int lsencap, uint32_t id, uint32_t seq,
    const void *routes, size_t routes_size)
{
    if (!buff)
        return ERROR_BUFF;

    size_t attr_size = lsencap ? sizeof(msg_update_attr_lsencap_t) :
        sizeof(msg_update_attr_t);

    attr_size += routes_size;

    if (len < attr_size)
        return ERROR_BUFFLEN;

    void *end = buff;

    msg_update_attr_t *attr = end;
    attr->attr_flags = ATTR_FLAG_WELL_KNOWN;
    attr->attr_type = ATTR_TYPE_WITHDRAWNROUTES;

    if (lsencap) {
        attr->attr_flags = ATTR_FLAG_WELL_KNOWN | ATTR_FLAG_LSENCAP;
        attr->attr_len = htons(attr_size - sizeof(msg_update_attr_lsencap_t));
        msg_update_attr_lsencap_t *attr_lsencap = end;
        attr_lsencap->attr_id = id;
        attr_lsencap->attr_seq = htonl(seq);
        end += sizeof(msg_update_attr_lsencap_t);
    } else {
        attr->attr_len = htons(attr_size - sizeof(msg_update_attr_t));
        end += sizeof(msg_update_attr_t);
    }

    memcpy(end, routes, routes_size);
    end += routes_size;

    return end - buff;
}

runtime_error_t
new_attr_reachableroutes(void *buff, size_t len,
    int lsencap, uint32_t id, uint32_t seq,
    const void *routes, size_t routes_size)
{
    if (!buff)
        return ERROR_BUFF;

    size_t attr_size = lsencap ? sizeof(msg_update_attr_lsencap_t) :
        sizeof(msg_update_attr_t);

    attr_size += routes_size;

    if (len < attr_size)
        return ERROR_BUFFLEN;

    void *end = buff;
    msg_update_attr_t *attr = end;
    attr->attr_flags = ATTR_FLAG_WELL_KNOWN;
    attr->attr_type = ATTR_TYPE_REACHABLEROUTES;
    if (lsencap) {
        attr->attr_flags = ATTR_FLAG_WELL_KNOWN | ATTR_FLAG_LSENCAP;
        attr->attr_len = htons(attr_size - sizeof(msg_update_attr_lsencap_t));
        msg_update_attr_lsencap_t *attr_lsencap = end;
        attr_lsencap->attr_id = id;
        attr_lsencap->attr_seq = htonl(seq);
        end += sizeof(msg_update_attr_lsencap_t);
    } else {
        attr->attr_len = htons(attr_size - sizeof(msg_update_attr_t));
        end += sizeof(msg_update_attr_t);
    }

    memcpy(end, routes, routes_size);
    end += routes_size;

    return end - buff;
}

/* server is a null-terminated C-string */
runtime_error_t
new_attr_nexthopserver(void *buff, size_t len,
    uint32_t next_itad, const char *server)
{
    if (!buff)
        return ERROR_BUFF;
    
    size_t server_len = strlen(server);
    
    if (len < sizeof(msg_update_attr_t) + sizeof(attr_nexthopserver_t) +
        server_len)
    {
        return ERROR_BUFFLEN;
    }

    if (next_itad == 0)
        return ERROR_ITAD;

    msg_update_attr_t *attr = buff;
    attr->attr_flags = ATTR_FLAG_WELL_KNOWN;
    attr->attr_type = ATTR_TYPE_NEXTHOPSERVER;
    attr->attr_len = htons(sizeof(attr_nexthopserver_t) + server_len);

    attr_nexthopserver_t *attr_val = (attr_nexthopserver_t*)attr->attr_val;
    attr_val->nexthopserver_itad = htonl(next_itad);
    attr_val->nexthopserver_serverlen = htons(server_len);
    memcpy(attr_val->nexthopserver_server, server, server_len);

    return sizeof(msg_update_attr_t) + sizeof(attr_nexthopserver_t) +
        server_len;
}

/* does not check path */
runtime_error_t
new_attr_advertisementpath(void *buff, size_t len, uint8_t type,
    const uint32_t *segs, uint16_t segs_size)
{
    if (!buff)
        return ERROR_BUFF;
    
    if (len < sizeof(msg_update_attr_t) + sizeof(attr_advertisementpath_t) +
        (sizeof(uint32_t) * segs_size))
    {
        return ERROR_BUFFLEN;
    }

    msg_update_attr_t *attr = buff;
    attr->attr_flags = ATTR_FLAG_WELL_KNOWN;
    attr->attr_type = ATTR_TYPE_ADVERTISEMENTPATH;
    attr->attr_len = htons(sizeof(attr_advertisementpath_t) +
        (sizeof(uint32_t) * segs_size));

    attr_advertisementpath_t *advertpath = (void*)&attr->attr_val;
    advertpath->itadpath_type = type;
    advertpath->itadpath_len = sizeof(uint32_t) * segs_size;
    for (size_t i = 0; i < segs_size; i++)
        advertpath->itadpath_segs[i] = htonl(segs[i]);

    return sizeof(msg_update_attr_t) + sizeof(attr_advertisementpath_t) +
        (sizeof(uint32_t) * segs_size);
}

/* does not check path */
runtime_error_t
new_attr_routedpath(void *buff, size_t len, uint8_t type,
     const uint32_t *segs, uint16_t segs_size)
{
    if (!buff)
        return ERROR_BUFF;
    
    if (len < sizeof(msg_update_attr_t) + sizeof(attr_routedpath_t) +
        (sizeof(uint32_t) * segs_size))
    {
        return ERROR_BUFFLEN;
    }

    msg_update_attr_t *attr = buff;
    attr->attr_flags = ATTR_FLAG_WELL_KNOWN;
    attr->attr_type = ATTR_TYPE_ROUTEDPATH;
    attr->attr_len = htons(sizeof(attr_routedpath_t) +
        (sizeof(uint32_t) * segs_size));

    attr_routedpath_t *routedpath = (void*)&attr->attr_val;
    routedpath->itadpath_type = type;
    routedpath->itadpath_len = sizeof(uint32_t) * segs_size;
    for (size_t i = 0; i < segs_size; i++)
        routedpath->itadpath_segs[i] = htonl(segs[i]);

    return sizeof(msg_update_attr_t) + sizeof(attr_routedpath_t) +
        (sizeof(uint32_t) * segs_size);
}

runtime_error_t
new_attr_atomicaggregate(void *buff, size_t len)
{
    if (!buff)
        return ERROR_BUFF;
    
    if (len < sizeof(msg_update_attr_t))
        return ERROR_BUFFLEN;

    msg_update_attr_t *attr = buff;
    attr->attr_flags = ATTR_FLAG_WELL_KNOWN;
    attr->attr_type = ATTR_TYPE_ATOMICAGGREGATE;
    attr->attr_len = 0;

    return sizeof(msg_update_attr_t);
}

runtime_error_t
new_attr_localpref(void *buff, size_t len, uint32_t localpref)
{
    if (!buff)
        return ERROR_BUFF;
    
    if (len < sizeof(msg_update_attr_t) + sizeof(attr_localpref_t))
        return ERROR_BUFFLEN;

    msg_update_attr_t *attr = buff;
    attr->attr_flags = ATTR_FLAG_WELL_KNOWN;
    attr->attr_type = ATTR_TYPE_LOCALPREFERENCE;
    attr->attr_len = htons(sizeof(attr_localpref_t));
    *(attr_localpref_t*)attr->attr_val = htonl(localpref);

    return sizeof(msg_update_attr_t) + sizeof(attr_localpref_t);
}

runtime_error_t
new_attr_multiexitdisc(void *buff, size_t len, uint32_t metric)
{
    if (!buff)
        return ERROR_BUFF;
    
    if (len < sizeof(msg_update_attr_t) + sizeof(attr_multiexitdisc_t))
        return ERROR_BUFFLEN;

    msg_update_attr_t *attr = buff;
    attr->attr_flags = ATTR_FLAG_WELL_KNOWN;
    attr->attr_type = ATTR_TYPE_MULTIEXITDISC;
    attr->attr_len = htons(sizeof(attr_multiexitdisc_t));
    *(attr_multiexitdisc_t*)attr->attr_val = htonl(metric);

    return sizeof(msg_update_attr_t) + sizeof(attr_multiexitdisc_t);
}

/* does not check communities */
runtime_error_t
new_attr_communities(void *buff, size_t len,
    const community_t *communities, size_t communities_size)
{
    if (!buff)
        return ERROR_BUFF;
    
    if (len < sizeof(msg_update_attr_t) +
        (sizeof(community_t) * communities_size))
    {
        return ERROR_BUFFLEN;
    }

    msg_update_attr_t *attr = buff;
    attr->attr_flags = ATTR_FLAG_TRANSITIVE;
    attr->attr_type = ATTR_TYPE_COMMUNITIES;
    attr->attr_len = htons(sizeof(community_t) * communities_size);

    community_t *comms = (void*)&attr->attr_val;
    for (size_t i = 0; i < communities_size; i++) {
        comms[i].community_itad = htonl(communities[i].community_itad);
        /* already in network order */
        comms[i].community_id = communities[i].community_id;
    }

    return sizeof(msg_update_attr_t) + (sizeof(community_t) * communities_size);
}

/* does not check itads */
runtime_error_t
new_attr_itadtopology(void *buff, size_t len,
    uint32_t id, uint32_t seq,
    const uint32_t *itads, size_t itads_size)
{
    if (!buff)
        return ERROR_BUFF;
    
    if (len < sizeof(msg_update_attr_lsencap_t) +
        (sizeof(uint32_t) * itads_size))
    {
        return ERROR_BUFFLEN;
    }

    msg_update_attr_lsencap_t *attr = buff;
    attr->attr_flags = ATTR_FLAG_WELL_KNOWN | ATTR_FLAG_LSENCAP;
    attr->attr_type = ATTR_TYPE_ITADTOPOLOGY;
    attr->attr_len = htons(sizeof(uint32_t) * itads_size);
    attr->attr_id = id;
    attr->attr_seq = htonl(seq);

    uint32_t *topo = (void*)&attr->attr_val;
    for (size_t i = 0; i < itads_size; i++)
        topo[i] = htonl(itads[i]);

    return sizeof(msg_update_attr_t) + (sizeof(uint32_t) * itads_size);
}

runtime_error_t
new_attr_convertedroute(void *buff, size_t len)
{
    if (!buff)
        return ERROR_BUFF;
    
    if (len < sizeof(msg_update_attr_t))
        return ERROR_BUFFLEN;

    msg_update_attr_t *attr = buff;
    attr->attr_flags = ATTR_FLAG_WELL_KNOWN;
    attr->attr_type = ATTR_TYPE_CONVERTEDROUTE;
    attr->attr_len = 0;

    return sizeof(msg_update_attr_t);
}

/* message KEEPALIVE */

runtime_error_t
new_msg_keepalive(void *buff, size_t len)
{
    if (!buff)
        return ERROR_BUFF;

    if (len < sizeof(msg_t))
        return ERROR_BUFFLEN;

    msg_t *msg = buff;
    msg->msg_len = 0;
    msg->msg_type = MSG_TYPE_KEEPALIVE;

    return sizeof(msg_t);
}


/* message NOTIFICATION */

runtime_error_t
new_msg_notif(void *buff, size_t len,
    uint8_t error_code, uint8_t error_subcode, size_t datalen, const void *data)
{
    if (!buff)
        return ERROR_BUFF;

    size_t msg_size = sizeof(msg_t) + sizeof(msg_notif_t) + datalen;
    if (len < msg_size)
        return ERROR_BUFFLEN;

    int checkres = check_notif_error_code_subcode(error_code, error_subcode);
    if (checkres)
        return checkres;

    msg_t *msg = buff;
    msg->msg_len = htons(sizeof(msg_notif_t) + datalen);
    msg->msg_type = MSG_TYPE_NOTIFICATION;

    msg_notif_t *msg_notif = (msg_notif_t*)msg->msg_val;
    msg_notif->notif_error_code = error_code;
    msg_notif->notif_error_subcode = error_subcode;
    memcpy(msg_notif->notif_data, data, datalen);

    return msg_size;
}


/* =========================== DESERIALIZATION ============================== */

/* really just validates and returns a pointer of type
 * does not validate length, returns consumed bytes
 * buff: in place
 * len: received bytes 
 */

/* message
 */

runtime_error_t
parse_msg(void *buff, size_t len, msg_t **msg_out)
{
    if (len < sizeof(msg_t))
        return ERROR_INCOMPLETE;

    msg_t *msg = buff;
    msg->msg_len = ntohs(msg->msg_len);

    if (msg->msg_type < MSG_TYPE_OPEN || msg->msg_type > MSG_TYPE_KEEPALIVE)
        return ERROR_MSGTYPE;

    *msg_out = msg;

    return sizeof(msg_t);
}


/* message OPEN
 */

runtime_error_t
parse_msg_open(void *buff, size_t len, msg_open_t **open_out)
{
    if (len < sizeof(msg_open_t))
        return ERROR_INCOMPLETE;
    
    msg_open_t *open = buff;
    open->open_hold = ntohs(open->open_hold);
    open->open_itad = ntohl(open->open_itad);
    open->open_opts_len = ntohs(open->open_opts_len);

    if (open->open_ver != PROTOCOL_VERSION)
        return ERROR_VERSION;

    if (open->open_hold != 0 && open->open_hold < 3)
        return ERROR_HOLD;

    if (open->open_itad == 0)
        return ERROR_ITAD;

    *open_out = open;

    return sizeof(msg_open_t);
}

runtime_error_t
parse_msg_open_opt(void *buff, size_t len, msg_open_opt_t **opt_out)
{
    if (len < sizeof(msg_open_opt_t))
        return ERROR_INCOMPLETE;

    msg_open_opt_t *opt = buff;
    opt->opt_type = ntohs(opt->opt_type);
    opt->opt_len = ntohs(opt->opt_len);

    if (opt->opt_type != OPEN_OPT_TYPE_CAPABILITY_INFO)
        return ERROR_OPT;

    *opt_out = opt;

    return sizeof(msg_open_opt_t);
}

runtime_error_t
parse_capinfo(void *buff, size_t len, capinfo_t **capinfo_out)
{
    if (len < sizeof(capinfo_t))
        return ERROR_INCOMPLETE;

    capinfo_t *capinfo = buff;
    capinfo->capinfo_code = ntohs(capinfo->capinfo_code);
    capinfo->capinfo_len = ntohs(capinfo->capinfo_len);

    if (capinfo->capinfo_code < CAPINFO_CODE_ROUTETYPE ||
        capinfo->capinfo_code > CAPINFO_CODE_TRANSMODE)
    {
        return ERROR_CAPINFO_CODE;
    }

    *capinfo_out = capinfo;

    return sizeof(capinfo_t);
}

runtime_error_t
parse_capinfo_routetype(void *buff, size_t len,
    capinfo_routetype_t **routetype_out)
{
    if (len < sizeof(capinfo_routetype_t))
        return ERROR_INCOMPLETE;

    capinfo_routetype_t *routetype = buff;
    routetype->routetype_af = ntohs(routetype->routetype_af);
    routetype->routetype_app_proto = ntohs(routetype->routetype_app_proto);

    if (routetype->routetype_af < AF_DECIMAL ||
        routetype->routetype_af > AF_CARRIER)
    {
        return ERROR_AF;
    }

    if (!((routetype->routetype_app_proto >= APP_PROTO_SIP &&
        routetype->routetype_app_proto <= APP_PROTO_H323_225_0_ANNEXG) ||
        routetype->routetype_app_proto == APP_PROTO_IAX2))
    {
        return ERROR_APP_PROTO;
    }

    *routetype_out = routetype;

    return sizeof(capinfo_routetype_t);
}

runtime_error_t
parse_capinfo_transmode(void *buff, size_t len,
    capinfo_transmode_t **transmode_out)
{
    if (len < sizeof(capinfo_transmode_t))
        return ERROR_INCOMPLETE;

    capinfo_transmode_t *transmode = buff;
    *transmode = htonl(*transmode);

    if (*transmode < CAPINFO_TRANS_SEND_RECV || *transmode > CAPINFO_TRANS_RECV)
        return ERROR_TRANS;

    *transmode_out = transmode;

    return sizeof(capinfo_transmode_t);
}



/* message UPDATE
 * list of attributes
 */

/* if returns 0, its a link-state encapsulated attribute,
 * call parse_msg_update_attr_lsencap
 */
runtime_error_t
parse_msg_update_attr(void *buff, size_t len, msg_update_attr_t **attr_out)
{
    if (len < sizeof(msg_update_attr_t))
        return ERROR_INCOMPLETE;
    
    msg_update_attr_t *attr = buff;
    attr->attr_len = ntohs(attr->attr_len);

    if (attr->attr_type < ATTR_TYPE_WITHDRAWNROUTES ||
        attr->attr_type > ATTR_TYPE_CARRIER)
    {
        return ERROR_ATTR_TYPE;
    }

    if ((attr->attr_type < ATTR_TYPE_WITHDRAWNROUTES ||
        attr->attr_type > ATTR_TYPE_CARRIER) &&
        !IS_ATTR_FLAG_WELL_KNOWN(attr->attr_flags))
    {
        return ERROR_ATTR_FLAG_WELL_KNOWN;
    }

    *attr_out = attr;

    if (IS_ATTR_FLAG_LSENCAP(attr->attr_flags))
        return 0;
    else
        return sizeof(msg_update_attr_t);
}

/* call parse_msg_update_attr() first to check attr */
runtime_error_t
parse_msg_update_attr_lsencap(void *buff, size_t len,
    msg_update_attr_lsencap_t **attr_out)
{
    if (len < sizeof(msg_update_attr_lsencap_t))
        return ERROR_INCOMPLETE;

    msg_update_attr_lsencap_t *attr = buff;
    attr->attr_len = ntohs(attr->attr_len);
    attr->attr_seq = ntohs(attr->attr_seq);

    if (!IS_ATTR_FLAG_LSENCAP(attr->attr_flags))
        return ERROR_ATTR_FLAG_LSENCAP;

    *attr_out = attr;

    return sizeof(msg_update_attr_lsencap_t);
}


/* attributes */

/* attribute WithdrawnRoutes
 * attribute ReachableRoutes
 */

runtime_error_t
parse_route(void *buff, size_t len, route_t **route_out)
{
    if (len < sizeof(route_t))
        return ERROR_INCOMPLETE;

    route_t *route = buff;
    route->route_af = ntohs(route->route_af);
    route->route_app_proto = ntohs(route->route_app_proto);
    route->route_len = ntohs(route->route_len);

    if (CHECK_AF(route->route_af))
        return ERROR_AF;
    if (CHECK_APP_PROTO(route->route_app_proto))
        return ERROR_APP_PROTO;

    *route_out = route;

    return sizeof(route_t);
}

/* attribute NextHopServer */

runtime_error_t
parse_attr_nexthopserver(void *buff, size_t len,
    attr_nexthopserver_t **nexthop_out)
{
    if (len < sizeof(attr_nexthopserver_t))
        return ERROR_INCOMPLETE;

    attr_nexthopserver_t *nexthop = buff;
    nexthop->nexthopserver_itad = ntohl(nexthop->nexthopserver_itad);
    nexthop->nexthopserver_serverlen = ntohs(nexthop->nexthopserver_serverlen);

    *nexthop_out = nexthop;

    return sizeof(attr_nexthopserver_t);
}


/* attribute AdvertisementPath
 * attribute RoutedPath
 */

runtime_error_t
parse_itadpath(void *buff, size_t len, itadpath_t **itadpath_out)
{
    if (len < sizeof(itadpath_t))
        return ERROR_INCOMPLETE;

    itadpath_t *itadpath = buff;
    itadpath->itadpath_len = ntohs(itadpath->itadpath_len);

    if (CHECK_ITADPATH_TYPE(itadpath->itadpath_type))
        return ERROR_ITADPATH_TYPE;

    *itadpath_out = itadpath;

    return sizeof(itadpath_t);
}


/* attribute AtomicAggregate
 * (empty)
 */


/* attribute LocalPreference
 */

runtime_error_t
parse_attr_localpref(void *buff, size_t len, attr_localpref_t **localpref_out)
{
    if (len < sizeof(attr_localpref_t))
        return ERROR_INCOMPLETE;

    *(attr_localpref_t*)buff = ntohl(*(attr_localpref_t*)buff);
    *localpref_out = buff;

    return sizeof(attr_localpref_t);
}


/* attribute MultiExitDiscriminator
 */

runtime_error_t
parse_attr_multiexitdisc(void *buff, size_t len, attr_multiexitdisc_t **multiexitdisc_out)
{
    if (len < sizeof(attr_multiexitdisc_t))
        return ERROR_INCOMPLETE;

    *(attr_multiexitdisc_t*)buff = ntohl(*(attr_multiexitdisc_t*)buff);
    *multiexitdisc_out = buff;

    return sizeof(attr_multiexitdisc_t);
}



/* attribute Communnity
 * list of communities
 */

runtime_error_t
parse_community(void *buff, size_t len, community_t **community_out)
{
    if (len < sizeof(community_t))
        return ERROR_INCOMPLETE;
    
    community_t *community = buff;
    community->community_itad = ntohl(community->community_itad);

    if (community->community_itad == 0x00000000 &&
        community->community_id != 0xffffff01)
    {
        return ERROR_COMMUNITY_ITAD;
    }

    *community_out = community;

    return sizeof(community_t);
}


/* attribute ITAD Topology
 * list of ITADs
 */

runtime_error_t
parse_itad(void *buff, size_t len, uint32_t **itad_out)
{
    if (len < sizeof(uint32_t))
        return ERROR_INCOMPLETE;

    uint32_t *itad = buff;
    *itad = ntohl(*itad);

    if (*itad == 0)
        return ERROR_ITAD;

    *itad_out = itad;

    return sizeof(uint32_t);
}


/* attribute ConvertedRoute
 * (empty)
 */



/* message KEEPALIVE
 * (empty, no parser)
 */



/* message NOTIFICATION
 */

runtime_error_t
parse_msg_notif(void *buff, size_t len, msg_notif_t **notif_out)
{
    if (len < sizeof(msg_notif_t))
        return ERROR_INCOMPLETE;

    msg_notif_t *notif = buff;

    int checkres = check_notif_error_code_subcode(notif->notif_error_code,
        notif->notif_error_subcode);
    if (checkres)
        return checkres;

    *notif_out = notif;

    return sizeof(msg_notif_t);
}


const char *
notif_code_subcode_str(int code, int subcode)
{
    static char buff[256];

    switch (code) {
    case NOTIF_CODE_ERROR_MSG:
        snprintf(buff, 256, "%s, %s", notif_code_strs[code],
            notif_subcode_msg_strs[subcode]);
    break;
    case NOTIF_CODE_ERROR_OPEN:
        snprintf(buff, 256, "%s, %s", notif_code_strs[code],
            notif_subcode_open_strs[subcode]);
    break;
    case NOTIF_CODE_ERROR_UPDATE:
        snprintf(buff, 256, "%s, %s", notif_code_strs[code],
            notif_subcode_update_strs[subcode]);
    break;
    case NOTIF_CODE_ERROR_EXPIRED:  /* RFC does not define subcodes for these */
    case NOTIF_CODE_ERROR_STATE:
    case NOTIF_CODE_CEASE:
        snprintf(buff, 256, "%s", notif_code_strs[code]);
    break;
    }

    return buff;
}

