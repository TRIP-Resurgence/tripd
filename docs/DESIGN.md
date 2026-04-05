<!--!
\defgroup design Architecture Design
\ingroup docs
\hidegroupgraph
[TOC]
-->

# Architecture Design

Roughly based on the UML class diagram from Vovida's VOCAL. Available
[here](https://github.com/TRIP-Resurgence/vovida-trip/blob/main/tripstack/docs/trip.pdf)

## Components

As static libraries

 - protocol: thread safe, no allocs; serialization and deserialization of protocol messages
 - functions: session manager
 - command: command parser, owns manager
 - db: databases
 - logging: logging functions
 - tripd: daemon, inits and launches parser for config and stdin

## Classes

 - command/parser: singleton command parser for configuration and console
 - functions/manager: singleton session manager (thread: accept loop) owns sessions
 - functions/locator: singleton peer information
 - functions/session: maintains the session state and messages (thread: connect/recv loops), owned by manager
 - db/trib: telephony routing information base, owned by manager
 - db/pib: policy information base, owned by manager

