<!--!
\defgroup api API Reference
\ingroup docs
\hidegroupgraph
[TOC]
-->

# API Reference

tripd has a HTTP REST API for ease of querying by Asterisk and other switches

## `/route`

Return a textual list of all routes

## `/route/full`

Return the full Loc-TRIB with details

## `/route/<number>`

Return full object in JSON format

## `/route/<number>/app-proto`

Return raw application layer protocol

## `/route/<number>/nexthop-server`

Return raw next hop server

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

## `/route/<number>/sip-uri`

Return a SIP URI if SIP route

 - `sip://number@host[:port]`

## `/route/<number>/human`

Returns full details for a route in show-like human readable form

## `/route/<prefix>`

Return a textual list of routes under a prefix

