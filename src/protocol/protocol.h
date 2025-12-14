/*

    trip: Modern TRIP LS implementation
    Copyright (C) 2023 arf20 (Ángel Ruiz Fernandez)

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

*/

#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* message header */

enum msg_type {
    MSG_TYPE_OPEN,
    MSG_TYPE_UPDATE,
    MSG_TYPE_NOTIFICATION,
    MSG_TYPE_KEEPALIVE
};

typedef struct {
    uint16_t    msg_len;
    uint8_t     msg_type;
    uint8_t     msg_val[];
} msg_t;


/* message OPEN */

enum open_opt_type {
    OPEN_OPT_TYPE_CAPABILITY_INFO = 1,
};

typedef struct {
    uint16_t    opt_type;
    uint16_t    opt_len;
    uint8_t     opt_val[];
} msg_open_opt_t;

typedef struct {
    uint8_t         open_ver;
    uint8_t         open_reserved;
    uint16_t        open_hold;
    uint32_t        open_itad;
    uint32_t        open_id;
    uint16_t        open_opts_len;
    msg_open_opt_t  open_opts[];
} msg_open_t;


enum cap_code {
    CAP_CODE_ROUTE_TYPES = 1,
    CAP_CODE_SEND_RECV
};

typedef struct {
    uint16_t    cap_code;
    uint16_t    cap_len;
    uint8_t     cap_val[];
} msg_open_opt_capinfo_t;


typedef struct {
    uint16_t    routetype_af;
    uint16_t    routetype_app_proto;
} capinfo_routetype_t;

#define CAPINFO_SENDRECV_NULL   (uint32_t)-1
typedef uint32_t capinfo_sendrecv_t;


/* message UPDATE
 * unpadded list of attributes
 * attributes defined by RFCs are Well-Known */

/*
 * attr_flag is a bitfield
 * bit  flag
 * 0    Well-Known Flag
 * 1    Transitive Flag
 * 2    Dependent Flag
 * 3    Partial Flag
 * 4    Link-state Encapsulated Flag
 */
#define IS_ATTR_FLAG_WELL_KNOWN(x)  ((x >> 0) & 1)
#define IS_ATTR_FLAG_TRANSITIVE(x)  ((x >> 1) & 1)
#define IS_ATTR_FLAG_DEPENDENT(x)   ((x >> 2) & 1)
#define IS_ATTR_FLAG_PARTIAL(x)     ((x >> 3) & 1)
#define IS_ATTR_FLAG_LSENCAP(x)     ((x >> 4) & 1)

enum attr_type {
    /* RFC3219 */
    ATTR_WITHDRAWNROUTES = 1,
    ATTR_REACHABLEROUTES,
    ATTR_NEXTHOPSERVER,
    ATTR_ADVERTISEMENTPATH,
    ATTR_ROUTEDPATH,
    ATTR_ATOMICAGGREGATE,
    ATTR_LOCALPREFERENCE,
    ATTR_MULTIEXITDISC,
    ATTR_COMMUNITIES,
    ATTR_ITADTOPOLOGY,
    ATTR_CONVERTEDROUTE,
    /* RFC5115 */
    ATTR_RESOURCEPRIORITY,
    /* RFC5140 */
    ATTR_TOTALCIRCUITCAPACITY,
    ATTR_AVAILABLECIRCUITS,
    ATTR_CALLSUCCESS,
    ATTR_E164PREFIX,
    ATTR_PENTADECPREFIX,
    ATTR_DECIMALPREFIX,
    ATTR_TRUNKGROUP,
    ATTR_CARRIER
};

typedef struct {
    uint8_t     attr_flags;
    uint8_t     attr_type;
    uint16_t    attr_len;
    uint8_t     attr_val[];
} msg_update_attr_t;

typedef struct {
    uint8_t     attr_flags;
    uint8_t     attr_type;
    uint16_t    attr_len;
    uint32_t    id;
    uint32_t    seq;
    uint8_t     attr_val[];
} msg_update_attr_lsencap_t;


/* attributes */

/* attribute WithdrawnRoutes
 * list of unpadded routes
 */

enum af {
    /* RFC3219 */
    AF_DECIMAL = 1,
    AF_PENTADECIMAL,
    AF_E164,
    /* RFC5140 */
    AF_TRUNKGROUP,
    AF_CARRIER
};

enum app_proto {
    /* RFC3219 */
    APP_PROTO_SIP = 1,              /* SIP */
    APP_PROTO_H323_225_0_Q931,      /* H.323-H.225.0-Q.931 */
    APP_PROTO_H323_225_0_RAS,       /* H.323-H.225.0-RAS */
    APP_PROTO_H323_225_0_ANNEXG,    /* H.323-H.225.0-Annex-G */
    /* vendor */
    APP_PROTO_IAX2 = 32768          /* vendor asterisk specific IAX2 */
};

typedef struct {
    uint16_t    af;
    uint16_t    app_proto;
    uint16_t    len;
    char        addr[];
} route_t;

typedef route_t attr_withdrawnroutes_t[];

/* attribute ReachableRoutes
 * list of unpadded routes (see above)
 */

typedef route_t attr_reachableroutes_t[];

/* attribute NextHopServer
 * server: host [":" port] 
 * host: hostname, IPv4 dotted format or IPv6 enclosed in "[" "]"
 * port: decimal number 1-65535
 */

typedef struct {
    uint32_t    next_itad;
    uint16_t    serverlen;
    char        server[];
} attr_nexthopserver_t;

/* attribute AdvertisementPath
 * mandatory if ReachableRoutes or WithdrawnRoutes present
 * list of ITAD path segments
 */

typedef struct {
    uint8_t     type;
    uint8_t     len;
    uint32_t    pathseg[];  /* ITAD numbers path */
} itadpathseg_t;

typedef itadpathseg_t attr_advertisementpath_t;

/* attribute RoutedPath
 * mandatory if ReachableRoutes present
 * syntax same as AdvertisementPath
 */

typedef itadpathseg_t attr_routedpath_t;

/* attribute AtomicAggregate
 * no value
 */

/* attribute LocalPreference
 */

typedef uint32_t attr_localpref_t;

/* attribute MultiExitDiscriminator
 */

typedef uint32_t attr_med_t;

/* attribute Communities
 * list of communities
 */

typedef struct {
    uint32_t    community_itad;
    uint32_t    community_id;
} community_t;

typedef community_t attr_communities_t[];

/* attribute ITAD Topology
 * has to be link-state encapsulated
 * list of peer ITADs
 */

typedef uint32_t attr_itadtopology_t[];

/* attribute ConvertedRoute
 * no value
 */


/* future RFC5115 and RFC5140 attributes go here */


/* message KEEPALIVE 
 * (empty message) header only
 */


/* message NOTIFICATION */

enum notif_code {
    NOTIF_CODE_ERROR_MSG = 1,
    NOTIF_CODE_ERROR_OPEN,
    NOTIF_CODE_ERROR_UPDATE,
    NOTIF_CODE_ERROR_EXPIRED,
    NOTIF_CODE_ERROR_STATE,
    NOTIF_CODE_CEASE
};

enum notif_subcode_msg {
    NOTIF_SUBCODE_MSG_BAD_LEN,
    NOTIF_SUBCODE_MSG_BAD_TYPE
};

enum notif_subcode_open {
    NOTIF_SUBCODE_OPEN_UNSUP_VERSION,
    NOTIF_SUBCODE_OPEN_BAD_ITAD,
    NOTIF_SUBCODE_OPEN_BAD_ID,
    NOTIF_SUBCODE_OPEN_UNSUP_OPT,
    NOTIF_SUBCODE_OPEN_BAD_HOLD,
    NOTIF_SUBCODE_OPEN_UNSUP_CAP,
    NOTIF_SUBCODE_OPEN_CAP_MISMATCH,
};

enum notif_subcode_update {
    NOTIF_SUBCODE_UPDATE_MALFORM_ATTR,
    NOTIF_SUBCODE_UPDATE_UNK_WELLKNOWN_ATTR,
    NOTIF_SUBCODE_UPDATE_MISS_WELLKNOWN_ATTR,
    NOTIF_SUBCODE_UPDATE_BAD_ATTR_FLAG,
    NOTIF_SUBCODE_UPDATE_BAD_ATTR_LEN,
    NOTIF_SUBCODE_UPDATE_INVAL_ATTR,
};

typedef struct {
    uint8_t     notif_error_code;
    uint8_t     notif_error_subcode;
    uint8_t     notif_data[];
} msg_notif_t;


/* serialization/deserialization functions */

/* runtime errors */

typedef enum runtime_errors_e {
    ERROR_BUFF = -1,            /* invalid buffer */
    ERROR_BUFFLEN = -2,         /* not enough buffer */
    ERROR_HOLD = -3,            /* hold time must be 0 or at least 3 seconds */
    ERROR_ITAD = -4,            /* ITAD must not be 0 (reserved) */
} runtime_error_t;

runtime_error_t new_msg_open(void *buff, size_t len, uint16_t hold,
    uint32_t itad, uint32_t id, const capinfo_routetype_t *capinfo_routetypes,
    size_t routetypes_size, capinfo_sendrecv_t capinfo_sendrecv);

runtime_error_t new_msg_update(void *buff, size_t len,
    const msg_update_attr_t **attrs, size_t attrs_size);

runtime_error_t new_msg_keepalive(void *buff, size_t len);

runtime_error_t new_msg_notification(void *buff, size_t len, uint8_t error_code,
    uint8_t error_subcode, size_t datalen, const void *data);


#endif /* _PROTOCOL_H */

