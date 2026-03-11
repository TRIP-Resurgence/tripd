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

#### `show < running-config | peers | sessions | session <host> | route >`

Show running LS information

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

#### `prefix-list`

Enter prefix list context

#### `trip <itad>`

Enter TRIP routing context, setting the ITAD for this LS

 - itad: ITAD number as registered at the [IANA registry](https://www.iana.org/assignments/trip-parameters/trip-parameters.xhtml#trip-parameters-5)

### Prefix list context

#### `prefix <af> <prefix> <app-proto> <server>`

Defines a prefix

 - af: address family { `e164` }
 - prefix: the prefix in the address family format
 - app-proto: application protocol { `sip` | `h323-h225-0-q931` | `h323-h225-0-ras` | `h323-h225-0-anxg` | `iax2` }
 - server: hostname or address that serves the prefix with that protocol

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

Adds a known peer

 - host: hostname of the peer (`getaddrinfo()`)
 - itad: expected ITAD number of peer

