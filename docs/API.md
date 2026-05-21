<!--!
\defgroup api API Reference
\ingroup docs
\hidegroupgraph
[TOC]
-->

# API Reference

tripd has a HTTP REST API for ease of querying by Asterisk and other switches.

See [configuration](/ref commands) for how to configure.

## `/route`

Return a textual list of all routes.

## `/route/trib-dump`

Return a full Loc-TRIB dump in ripe-ncc/bgpdump-like line-per-entry format:

```
TRIB_DUMP|<af>|<app proto>|<prefix>|<unix time>|<nextitad>|<nexthop>|<type>[|<peer itad>|<peer addr>|<advert path ' '>|<routed path ' '>|<local pref>|<metric>|<communities ':'>|]
```

## `/route/<prefix>`

Return a textual list of routes in the same formate as `/route` under a prefix,
which is considered such when there are more than one matching entries.

## `/route/<number>`

Return full object in JSON format.

## `/route/<number>/af`

Return raw application layer protocol.

## `/route/<number>/app-proto`

Return raw application layer protocol.

## `/route/<number>/nexthop-server`

Return raw next hop server.

## `/route/<number>/asterisk`

Return an Asterisk Dial string in the format Technology/Resource/Extension.

Application protocol is mapped to Technology as the following:

 - SIP -> PJSIP
 - IAX2 -> IAX2

Next hop server is copied directly into Resource. This allows specification of
SIP `host[:port]` or an IAX2 resource specification string in Asterisk format in
the form `[username[:password[:pubkey]]@]peer[:port]`. Note that it is sent cleartext
in the wire.

Number is copied directly into Extension without context or options.

Refer to [asterisk docs](https://docs.asterisk.org/Latest_API/API_Documentation/Dialplan_Applications/Dial/)

## `/route/<number>/uri`

Return a SIP or IAX2 URI if available.

 - `sip:number@host[:port]`
 - `iax2:[username[:password[:pubkey]]@]host[:port][/number]` equivalent to `iax2:AsteriskIAXResource` without context.

## `/route/<number>/human`

Returns full details for a route in show-like human readable form.

