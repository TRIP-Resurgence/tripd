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

*/

/** \file
 * \brief Protocol definition header
 */

#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#include <stdint.h>
#include <stddef.h>


#define MAX_MSG_SIZE    4096


/** \brief Message types */
enum msg_type {
    MSG_TYPE_OPEN = 1,
    MSG_TYPE_UPDATE,
    MSG_TYPE_NOTIFICATION,
    MSG_TYPE_KEEPALIVE
};

/** \brief Message type strings */
extern const char *msg_type_strs[];

/** \brief Message header */
typedef struct __attribute__((packed)) {
    uint16_t    msg_len;
    uint8_t     msg_type;
    uint8_t     msg_val[];
} msg_t;


/** \brief Message OPEN optional parameter types */
enum open_opt_type {
    OPEN_OPT_TYPE_CAPABILITY_INFO = 1,
};

/** \brief Message type strings */
extern const char *open_opt_type_strs[];

/** \brief Message OPEN optional paramter */
typedef struct __attribute__((packed)) {
    uint16_t    opt_type;
    uint16_t    opt_len;
    uint8_t     opt_val[];
} msg_open_opt_t;

/** \brief current protocol version */
#define PROTOCOL_VERSION    1

/** \brief Message OPEN */
typedef struct __attribute__((packed)) {
    uint8_t         open_ver;
    uint8_t         open_reserved;
    uint16_t        open_hold;
    uint32_t        open_itad;
    uint32_t        open_id;
    uint16_t        open_opts_len;
    msg_open_opt_t  open_opts[];
} msg_open_t;


/** \brief Capability information option types */
enum capinfo_code {
    CAPINFO_CODE_ROUTETYPE = 1,
    CAPINFO_CODE_TRANSMODE
};

/** \brief Capability information option type strings */
extern const char *capinfo_code_strs[];

/** \brief Capability information option */
typedef struct __attribute__((packed)) {
    uint16_t    capinfo_code;
    uint16_t    capinfo_len;
    uint8_t     capinfo_val[];
} capinfo_t;


/** \brief Capability information supported route types */
typedef struct __attribute__((packed)) {
    uint16_t    routetype_af;
    uint16_t    routetype_app_proto;
} capinfo_routetype_t;


/** \brief Capability information transmission mode types */
enum capinfo_transmode {
    CAPINFO_TRANS_SEND_RECV = 1,
    CAPINFO_TRANS_SEND,
    CAPINFO_TRANS_RECV
};

/** \brief Capability information transmission mode types strings */
extern const char *capinfo_transmode_strs[];

/** \brief Capability information NULL transmission mode value
 *
 * Only for serializer */
#define CAPINFO_TRANS_NULL   (uint32_t)-1

/** \brief Capability information transmission mode */
typedef uint32_t capinfo_transmode_t;


/** \brief Message UPDATE
 *
 * Unpadded list of attributes,
 * dAttributes defined by RFCs are "Well-Known", and should have the WELL_KNOWN
 * bit set.
 *
 * attr_flag is a bitfield
 * bit  flag
 * 0    Well-Known Flag
 * 1    Transitive Flag
 * 2    Dependent Flag
 * 3    Partial Flag
 * 4    Link-state Encapsulated Flag
 */
#define IS_ATTR_FLAG_WELL_KNOWN(x)  ((x >> 7) & 1)
#define IS_ATTR_FLAG_TRANSITIVE(x)  ((x >> 6) & 1)
#define IS_ATTR_FLAG_DEPENDENT(x)   ((x >> 5) & 1)
#define IS_ATTR_FLAG_PARTIAL(x)     ((x >> 4) & 1)
#define IS_ATTR_FLAG_LSENCAP(x)     ((x >> 3) & 1)

#define ATTR_FLAG_WELL_KNOWN        0b10000000
#define ATTR_FLAG_TRANSITIVE        0b01000000
#define ATTR_FLAG_DEPENDENT         0b00100000
#define ATTR_FLAG_PARTIAL           0b00010000
#define ATTR_FLAG_LSENCAP           0b00001000

/** \brief UPDATE attribute types */
enum attr_type {
    /** RFC3219 */
    ATTR_TYPE_WITHDRAWNROUTES = 1,
    ATTR_TYPE_REACHABLEROUTES,
    ATTR_TYPE_NEXTHOPSERVER,
    ATTR_TYPE_ADVERTISEMENTPATH,
    ATTR_TYPE_ROUTEDPATH,
    ATTR_TYPE_ATOMICAGGREGATE,
    ATTR_TYPE_LOCALPREFERENCE,
    ATTR_TYPE_MULTIEXITDISC,
    ATTR_TYPE_COMMUNITIES,
    ATTR_TYPE_ITADTOPOLOGY,
    ATTR_TYPE_CONVERTEDROUTE,
    /** RFC5115 */
    ATTR_TYPE_RESOURCEPRIORITY,
    /** RFC5140 */
    ATTR_TYPE_TOTALCIRCUITCAPACITY,
    ATTR_TYPE_AVAILABLECIRCUITS,
    ATTR_TYPE_CALLSUCCESS,
    ATTR_TYPE_E164PREFIX,
    ATTR_TYPE_PENTADECPREFIX,
    ATTR_TYPE_DECIMALPREFIX,
    ATTR_TYPE_TRUNKGROUP,
    ATTR_TYPE_CARRIER
};

/** \brief UPDATE attribute */
typedef struct __attribute__((packed)) {
    uint8_t     attr_flags;
    uint8_t     attr_type;
    uint16_t    attr_len;
    uint8_t     attr_val[];
} msg_update_attr_t;

/** \brief UPDATE link-state encapsulated attribute */
typedef struct __attribute__((packed)) {
    uint8_t     attr_flags;
    uint8_t     attr_type;
    uint16_t    attr_len;
    uint32_t    attr_id;
    uint32_t    attr_seq;
    uint8_t     attr_val[];
} msg_update_attr_lsencap_t;

#define INITIAL_SEQUENCE_NUMBER 0x80000001  /* -N + 1 */
#define MAX_SEQUENCE_NUMBER     0x7fffffff  /* N - 1*/


/* attributes */


/** \brief Address families */
enum af {
    /* RFC3219 */
    AF_DECIMAL = 1,
    AF_PENTADECIMAL,
    AF_E164,
    /* RFC5140 */
    AF_TRUNKGROUP,
    AF_CARRIER
};

/** \brief Address family strings */
extern const char *af_strs[];

/** \brief Application protocols */
enum app_proto {
    /** RFC3219 */
    APP_PROTO_SIP = 1,              /**< SIP */
    APP_PROTO_H323_225_0_Q931,      /**< H.323-H.225.0-Q.931 */
    APP_PROTO_H323_225_0_RAS,       /**< H.323-H.225.0-RAS */
    APP_PROTO_H323_225_0_ANNEXG,    /**< H.323-H.225.0-Annex-G */
    /** Vendor */
    APP_PROTO_IAX2 = 32768          /**< vendor specific asterisk IAX2
                                      (RFC 5456) */
};

/** \brief Application protocol to string */
const char *app_proto_str(int app_proto);

/** \brief Route */
typedef struct __attribute__((packed)) {
    uint16_t    route_af;
    uint16_t    route_app_proto;
    uint16_t    route_len;
    char        route_addr[];
} route_t;

/** \brief Attribute WithdrawnRoutes
 *
 * flags: well-known, [link-state encapsulation]
 * list of unpadded routes
 */
typedef route_t attr_withdrawnroutes_t[];


/** \brief Attribute ReachableRoutes
 *
 * flags: well-known, [link-state encapsulation]
 * list of unpadded routes (see above)
 */
typedef route_t attr_reachableroutes_t[];


/** \brief Attribute NextHopServer
 *
 * flags: well-known
 * server: host [":" port] 
 * host: hostname, IPv4 dotted format or IPv6 enclosed in "[" "]"
 * port: decimal number 1-65535
 */

typedef struct __attribute__((packed)) {
    uint32_t    nexthopserver_itad;
    uint16_t    nexthopserver_serverlen;
    char        nexthopserver_server[];
} attr_nexthopserver_t;


/** \brief ITAD path types */
enum itadpath_type {
    ITADPATH_TYPE_AP_SET = 1,
    ITADPATH_TYPE_AP_SEQUENCE
};

/** \brief ITAD path */
typedef struct __attribute__((packed)) {
    uint8_t     itadpath_type;
    uint8_t     itadpath_len;       /**< Number of ITADs in path seg */
    uint32_t    itadpath_segs[];    /**< ITAD numbers path */
} itadpath_t;

/** \brief Attribute AdvertisementPath
 *
 * flags: well-known
 * mandatory if ReachableRoutes or WithdrawnRoutes present
 * list of ITAD path segments
 */
typedef itadpath_t attr_advertisementpath_t;


/** \brief Attribute RoutedPath
 *
 * flags: well-known
 * mandatory if ReachableRoutes present
 * syntax same as AdvertisementPath
 */
typedef itadpath_t attr_routedpath_t;


/** \brief Attribute AtomicAggregate
 *
 * flags: well-known
 * no value
 */
typedef void attr_atomicaggregate_t;


/** \brief Attribute LocalPreference */
typedef uint32_t attr_localpref_t;


/** \brief Attribute MultiExitDiscriminator
 *
 * flags: well-known
 */
typedef uint32_t attr_multiexitdisc_t;


/** \brief Community */
typedef struct __attribute__((packed)) {
    uint32_t    community_itad;
    uint32_t    community_id;
} community_t;

/** \brief Community value no-export */
#define COMMUNITY_NO_EXPORT ((community_t){ 0x00000000, 0xffffff01 })

/** \brief Attribute Communities
 *
 * flags: well-known, transitive
 * list of communities
 */
typedef community_t attr_communities_t[];


/** \brief Attribute ITAD Topology
 *
 * flags: well-known, link-state encapsulated
 * list of peer ITADs
 */
typedef uint32_t attr_itadtopology_t[];


/** \brief Attribute ConvertedRoute
 *
 * flags: well-known
 * no value
 */
typedef void attr_convertedroute_t;


/* future RFC5115 and RFC5140 attributes go here */


/** \brief Message KEEPALIVE 
 *
 * no value
 */
typedef void msg_keepalive_t;


/** \brief NOTIFICATION error code */
enum notif_code {
    NOTIF_CODE_ERROR_MSG = 1,
    NOTIF_CODE_ERROR_OPEN,
    NOTIF_CODE_ERROR_UPDATE,
    NOTIF_CODE_ERROR_EXPIRED,
    NOTIF_CODE_ERROR_STATE,
    NOTIF_CODE_CEASE
};

/** \brief NOTIFICATION error code strings */
extern const char *notif_code_strs[];

/** \brief NOTIFICATION error subcode for message */
enum notif_subcode_msg {
    NOTIF_SUBCODE_MSG_BAD_LEN = 1,
    NOTIF_SUBCODE_MSG_BAD_TYPE
};

/** \brief NOTIFICATION error subcode for message strings */
extern const char *notif_subcode_msg_strs[];

/** \brief NOTIFICATION error subcode for OPEN */
enum notif_subcode_open {
    NOTIF_SUBCODE_OPEN_UNSUP_VERSION = 1,
    NOTIF_SUBCODE_OPEN_BAD_ITAD,
    NOTIF_SUBCODE_OPEN_BAD_ID,
    NOTIF_SUBCODE_OPEN_UNSUP_OPT,
    NOTIF_SUBCODE_OPEN_BAD_HOLD,
    NOTIF_SUBCODE_OPEN_UNSUP_CAP,
    NOTIF_SUBCODE_OPEN_CAP_MISMATCH
};

/** \brief NOTIFICATION error subcode for OPEN strings*/
extern const char *notif_subcode_open_strs[];

/** \brief NOTIFICATION error subcode for UPDATE */
enum notif_subcode_update {
    NOTIF_SUBCODE_UPDATE_MALFORM_ATTR = 1,
    NOTIF_SUBCODE_UPDATE_UNK_WELLKNOWN_ATTR,
    NOTIF_SUBCODE_UPDATE_MISS_WELLKNOWN_ATTR,
    NOTIF_SUBCODE_UPDATE_BAD_ATTR_FLAG,
    NOTIF_SUBCODE_UPDATE_BAD_ATTR_LEN,
    NOTIF_SUBCODE_UPDATE_INVAL_ATTR
};

/** \brief NOTIFICATION error subcode for UPDATE strings */
extern const char *notif_subcode_update_strs[];

/** \brief subcode strings per code class */
extern const char **notif_code_subcodes_strs[];

/** \brief Message NOTIFICATION */
typedef struct __attribute__((packed)) {
    uint8_t     notif_error_code;
    uint8_t     notif_error_subcode;
    uint8_t     notif_data[];
} msg_notif_t;


/* serialization/deserialization functions */

/** \brief Serialization/deserialization runtime errors */
typedef enum runtime_errors_e {
    /* Serialization and deserialization */
    ERROR_BUFF = -1,                /**< Invalid buffer */
    ERROR_BUFFLEN = -2,             /**< Not enough buffer */
    ERROR_HOLD = -3,                /**< Hold time must be 0 or at least 3 s */
    ERROR_ITAD = -4,                /**< ITAD must not be 0 (reserved) */
    ERROR_NOTIF_ERROR_CODE = -5,    /**< Invalid NOTIFICATION error code */
    ERROR_NOTIF_ERROR_SUBCODE = -6, /**< Invalid NOTIFICATION error subcode */
    /* Deserialization specific */
    ERROR_INCOMPLETE = -7,          /**<Passed an incomplete message, recv more*/
    ERROR_MSGTYPE = -8,             /**< Invalid message type */
    ERROR_VERSION = -9,             /**< Unsupported protocol version */
    ERROR_OPT = -10,                /**< Unsupported OPEN option param */
    ERROR_CAPINFO_CODE = -11,       /**< Unsupported capability info code */
    ERROR_AF = -12,                 /**< Unsupported address family */
    ERROR_APP_PROTO = -13,          /**< Unsupported application protocol */
    ERROR_TRANS = -14,              /**< Invalid send/recv capability */
    ERROR_ATTR_TYPE = -15,          /**< Unsupported attribute type */
    ERROR_ATTR_FLAG_WELL_KNOWN = -16,/**< Attribute should have well-known */
    ERROR_ATTR_FLAG_LSENCAP = -17,  /**< Attribute must be link-state encapsul*/
    ERROR_ITADPATH_TYPE = -18,      /**< Unsupported ITAD path type */
    ERROR_COMMUNITY_ITAD = -19      /**< Reserved community ITAD with bad ID */
} runtime_error_t;

/** \brief Serialization/deserialization runtime error strings */
extern const char *runtime_error_strs[];


/* default timers */

#define TIMER_CONNECT_RETRY             120
#define TIMER_HOLD_TIME                 90
#define TIMER_KEEPALIVE                 30
#define TIMER_MAX_PURGE_TIME            10
#define TIMER_DISABLE_TIME              180
#define TIMER_MIN_ITAD_ORIG_INT         30
#define TIMER_MIN_ROUTE_ADVERT_INT      30


/* default attribute values (when not present) */

#define DEF_LOCAL_PREF                  100
#define DEF_METRIC                      100


/* objects */

/** \brief Supported routetypes constant */
extern const capinfo_routetype_t supported_routetypes[];
/** \brief Supported routetypes constant size */
extern const size_t supported_routetypes_size;


/* macros */

/** \brief TCP port as defined by RFC 3219 */
#define PROTO_TCP_PORT  6069

/** \brief Try-Catch macro for serialization/deserialization functions */
#define PROTO_TRY(o, res, a) \
    res = o; \
    if (res < 0) { \
        ERROR("protocol error: %s", runtime_error_strs[-res]); \
        a; \
    }


/* message serializers
 * sizes are in element count, not bytes
 */

/** \brief Serialize OPEN message */
runtime_error_t new_msg_open(void *buff, size_t len, uint16_t hold,
    uint32_t itad, uint32_t id, const capinfo_routetype_t *capinfo_routetypes,
    size_t routetypes_size, capinfo_transmode_t capinfo_transmode);

/** \brief Serialize UPDATE message */
runtime_error_t new_msg_update(void *buff, size_t len,
    const msg_update_attr_t **attrs, size_t attrs_size);

/** \brief Serialize KEEPALIVE message */
runtime_error_t new_msg_keepalive(void *buff, size_t len);

/** \brief Serialize NOTIFICATION message */
runtime_error_t new_msg_notif(void *buff, size_t len, uint8_t error_code,
    uint8_t error_subcode, size_t datalen, const void *data);


/* UPDATE attribute serializers */

/** \brief Serialize route */
runtime_error_t new_route(void *buff, size_t len, uint16_t af,
    uint16_t app_proto, const char *addr);

/** \brief Serialize WithdrawnRoutes attribute
 * routes_size is size of routes in bytes */
runtime_error_t new_attr_withdrawnroutes(void *buff, size_t len, int lsencap,
    uint32_t id, uint32_t seq, const void *routes, size_t routes_size);

/** \brief Serialize ReachableRoutes attribute
 * routes_size is size of routes in bytes */
runtime_error_t new_attr_reachableroutes(void *buff, size_t len, int lsencap,
    uint32_t id, uint32_t seq, const void *routes, size_t routes_size);

/** \brief Serialize NextHopServer attribute */
runtime_error_t new_attr_nexthopserver(void *buff, size_t len,
    uint32_t next_itad, const char *server);

/** \brief Serialize AdvertisementPath attribute */
runtime_error_t new_attr_advertisementpath(void *buff, size_t len, uint8_t type,
    const uint32_t *segs, uint16_t segs_size);

/** \brief Serialize RoutedPath attribute */
runtime_error_t new_attr_routedpath(void *buff, size_t len, uint8_t type,
    const uint32_t *segs, uint16_t segs_size);

/** \brief Serialize AtomicAggregate attribute */
runtime_error_t new_attr_atomicaggregate(void *buff, size_t len);

/** \brief Serialize LocalPref attribute */
runtime_error_t new_attr_localpref(void *buff, size_t len, uint32_t localpref);

/** \brief Serialize MultiExitDisc attribute */
runtime_error_t new_attr_multiexitdisc(void *buff, size_t len, uint32_t metric);

/** \brief Serialize Communities attribute */
runtime_error_t new_attr_communities(void *buff, size_t len,
    const community_t *communities, size_t communities_size);

/** \brief Serialize ITAD Topology attribute */
runtime_error_t new_attr_itadtopology(void *buff, size_t len, uint32_t id,
    uint32_t seq, const uint32_t *itads, size_t itads_size);

/** \brief Serialize ConvertedRoute attribute */
runtime_error_t new_attr_convertedroute(void *buff, size_t len);


/* deserializers */

/* messages */

/** \brief Deserialize message */
runtime_error_t parse_msg(void *buff, size_t len, msg_t **msg_out);


/* message OPEN
 */

/** \brief Deserialize message OPEN */
runtime_error_t parse_msg_open(void *buff, size_t len,
    msg_open_t **open_out);

/** \brief Deserialize message OPEN optional parameter */
runtime_error_t parse_msg_open_opt(void *buff, size_t len,
    msg_open_opt_t **opt_out);

/** \brief Deserialize option capability information */
runtime_error_t parse_capinfo(void *buff, size_t len,
    capinfo_t **capinfo_out);

/** \brief Deserialize option route type */
runtime_error_t parse_capinfo_routetype(void *buff, size_t len,
    capinfo_routetype_t **routetype_out);

/** \brief Deserialize option transmission mode */
runtime_error_t parse_capinfo_transmode(void *buff, size_t len,
    capinfo_transmode_t **transmode_out);


/* message UPDATE
 * list of attributes
 */

/** \brief Deserialize UPDATE attribute */
runtime_error_t parse_msg_update_attr(void *buff, size_t len,
    msg_update_attr_t **attr_out);

/** \brief Deserialize UPDATE link-state encapsulated attribute */
runtime_error_t parse_msg_update_attr_lsencap(void *buff, size_t len,
    msg_update_attr_lsencap_t **attr_out);


/* attributes */

/** \brief Deserialize route */
runtime_error_t parse_route(void *buff, size_t len,
    route_t **route_out);

/** \brief Deserialize ITAD path */
runtime_error_t parse_itadpath(void *buff, size_t len,
    itadpath_t **itadpath_out);

/** \brief Deserialize LocalPreference attribute */
runtime_error_t parse_attr_localpref(void *buff, size_t len,
    attr_localpref_t **localpref_out);

/** \brief Deserialize MultiExitDisc attribute */
runtime_error_t parse_attr_multiexitdisc(void *buff, size_t len,
    attr_multiexitdisc_t **multiexitdisc_out);

/** \brief Deserialize Community */
runtime_error_t parse_community(void *buff, size_t len,
    community_t **community_out);

/** \brief Deserialize ITAD */
runtime_error_t parse_itad(void *buff, size_t len,
    uint32_t **itad_out);



/* message KEEPALIVE
 * (empty, no parser)
 */


/* message NOTIFICATION
 */

/** \brief Deserialize NOTIFICATION message */
runtime_error_t parse_msg_notif(void *buff, size_t len,
    msg_notif_t **notif_out);

/** \brief String NOTIFICATION code, subcode */
const char *notif_code_subcode_str(int code, int subcode);


#endif /* _PROTOCOL_H */

