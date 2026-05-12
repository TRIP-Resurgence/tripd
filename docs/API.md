<!--!
\defgroup api API Reference
\ingroup docs
\hidegroupgraph
[TOC]
-->

# API Reference

tripd has a HTTP REST API for ease of querying by Asterisk and other switches

## `/query/<number>/app-proto`

Return application layer protocol

## `/query/<number>` \| `/query/<number>/nexthop-server`

Return next hop server

## `/query/<number>/asterisk`

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

## `/query/<number>/full`

Returns full details for a route in textual form

## `/list/<prefix>`

Return a textual list of routes under a prefix

## `/full-table`

Return the full Loc-TRIB

