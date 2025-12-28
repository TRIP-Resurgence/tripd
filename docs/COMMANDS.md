<!--!
\defgroup command Command Reference
\ingroup docs
\hidegroupgraph
[TOC]
-->

# Command Reference

tripd command usage and reference

The tripd configuration is inspired in a Cisco-style commands,
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

#### `end`

Exit any context to the root base context
#### `exit`

Exit current context to the previous one in the tree

### Base context

#### `log <file> <loglevel>`

Where to write the log

 - file: `stdout`, `stderr` or a filename
 - loglevel: { `error` | `warning` | `info` | `debug` }

#### `bind-address <address>`

What interface to bind to

 - address: address or localhost (`getaddrinfo()`)

#### `prefix-list`

Enter prefix list context

#### `prefix <af> <prefix> <app-proto> <server>`

Defines a prefix

 - af: address family { `e164` }
 - prefix: the prefix in the address family format
 - app-proto: application protocol { `sip` | `h323-h225-0-q931` | `h323-h225-0-ras` | `h323-h225-0-anxg` | `iax2` }
 - server: hostname or address that serves the prefix with that protocol

#### `trip <itad>`

Enter TRIP routing context, setting the ITAD for this LS

 - itad: ITAD number as registered at the [IANA registry](https://www.iana.org/assignments/trip-parameters/trip-parameters.xhtml#trip-parameters-5)

#### `ls-id <id>`

Set LS ID for the LS, unique inside the ITAD

 - id: id in dotted decimal representation as in BGP

#### `timers <hold>`

Sets LS timers

 - hold: hold time in seconds (time between KEEPALIVEs)

#### `peer <host> remote-itad <itad>`

Adds a known peer

 - host: hostname of the peer (`getaddrinfo()`)
 - itad: expected ITAD number of peer

