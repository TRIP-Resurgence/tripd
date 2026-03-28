<!--!
\defgroup command Command Reference
\ingroup docs
\hidegroupgraph
[TOC]
-->

# Command Reference

tripd command usage and reference

The tripd configuration is inspired by the Cisco IOS command line interface,
where the configuration syntax is the same as the interactive command syntax.

You can read a basic example configuration `tripd.conf` at the root of this project.

## Context tree

The configuration is organized in a tree of contexts

 - root base context
   - config context
     - prefix list context: where you define your prefixes to announce
     - trip context: where you define your ITAD, ID and LSs to peer with

### Common commands

Some commands are common between contexts

#### `help`

Display contextual help information

#### `end`

Exit any context and return to the root base context.

#### `exit`

Exit current context and return to the outer context.

### Root context

#### `enable`

Enter privileged mode

#### `disable`

Exit privileged mode

#### `configure`

Enter configuration context

#### `show <options>`

Show running LS information

 - `running-config`
 - `peers`
 - `session [peer address]`
 - `route [ for <prefix or number> ]`
 - `acl [name]`
 - `route-map [name]`

#### `shutdown`

Terminate all sessions and shutdown LS

### Configuration context

#### `log <file> <loglevel>`

Where to write the log. Default level is `debug`.

 - file: `stdout`, `stderr` or a filename
 - loglevel: { `error` | `warning` | `info` | `debug` | `trace` }

#### `bind-address <address>`

What interface to bind to

 - address: address or localhost (`getaddrinfo()`)

#### `route { add <af> <prefix> <app-proto> <server> | del <af> <prefix> }`

Add or remove local routes

 - af: address family { `e164` }
 - prefix: the prefix in the address family format
 - app-proto: application protocol { `sip` | `h323-h225-0-q931` | `h323-h225-0-ras` | `h323-h225-0-anxg` | `iax2` }
 - server: hostname or address that serves the prefix with that protocol

#### `acl <acl-name> { permit | deny } <expression>`

Add entry to ACL

 - acl-num: access list name
 - expression: Prefix or Asterisk style dialplan pattern match expression

##### Patterns

Starts with character '_'

[Asterisk pattern matching](https://docs.asterisk.org/Configuration/Dialplan/Pattern-Matching/)

 - 0-9: A number matchis this number.
 - X: The letter X or x represents a single digit from 0 to 9.
 - Z: The letter Z or z represents any digit from 1 to 9.
 - N: The letter N or n matches any digit from 2-9.
 - .: The '.' character matches one or more characters.

Notes:

 - "[]" charsets not supported yet
 - '.' can only be at the end of the pattern

#### `route-map <map-tag> [ permit | deny ] [seq]`

Enter a route map context to define

 - map-tag: route map identifier
 - `[ permit | deny ]`: redistribute or not
 - seq: sequence number

#### `trip <itad>`

Enter TRIP routing context, setting the ITAD for this LS

 - itad: ITAD number as registered at the [IANA registry](https://www.iana.org/assignments/trip-parameters/trip-parameters.xhtml#trip-parameters-5)

### Route Map context

#### `match <af> <acl-name> [ <acl-name> ... ]`

Configure route map to match prefixes that are permitted by an access list

 - af: Address family
 - acl-name: access control list name

#### `set <...>`

Set route attributes in map

 - `local-preference <local-pref>`
 - `metric <metric>`
 - `next-hop <af> <server>`
 - `itad-path prepend <n>`

 - n: numer of times to prepend route's ITAD-path with local ITAD

### TRIP context

#### `ls-id <id>`

Set LS ID for the LS, unique inside the ITAD

 - id: id in dotted decimal representation as in BGP

#### `timers <hold> [keep-alive] [connect-retry] [max-purge-time] [disable-time] [min-itad-orig-int] [min-route-advert-int]`

Sets LS timers

 - hold: hold time in seconds, time to declare connection dead
 - keep-alive: time between sending keepalives
 - max-purge-time:
 - disable-time:
 - min-itad-orig-int:
 - min-route-advert-int:

#### `peer <host> remote-itad <itad>`

Adds a peer

 - host: hostname of the peer (`getaddrinfo()`)
 - itad: expected ITAD number of peer

#### `peer <host> route-map <map-tag> { in | out }`

Define route map

 - host: peer hostname to apply to
 - map-tag: map identifier to apply
 - `{ in | out }`: direction

## Doubts

 - Should peer be neighbor
 - Should peers and sessions be under a trip subcmd

