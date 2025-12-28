<!--!
\defgroup design Architecture Design
\ingroup docs
\hidegroupgraph
[TOC]
-->

# Architecture Design

Roughly based on the UML class diagram from Vovida's VOCAL

## Components

As static libraries

 - protocol: thread safe, no allocs; serialization and deserialization of protocol messages
 - functions: session manager
 - command: command parser, owns manager
 - logging: logging functions
 - tripd: daemon, inits and launches parser for config and stdin

## Classes

 - command/parser: singleton command parser for configuration and console
 - functions/manager: singleton session manager (thread: accept loop) owns sessions
 - functions/locator: singleton peer information
 - functions/session: maintains the session state and messages (thread: connect/recv loops)

