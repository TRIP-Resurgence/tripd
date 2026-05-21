<!--!
\defgroup enum ENUM query interface
\ingroup docs
\hidegroupgraph
[TOC]
-->

# ENUM query interface

For compatibility reasons, an ENUM query interface is available for switches and PBXs that
only support it for dynamic routing. It emulates an ENUM zone where all the numbers under a route return
a DDDS RegEx that the application should be able to handle, for both SIP and IAX2 application protocols.

See [configuration](/ref commands) for how to configure.

## Querying

To query a route via ENUM, the reverse E.164 convention is used, with the specified ENUM zone appended. For example

```
0119273 -> 3.7.2.9.1.1.0.e164.arpa.
```

to which the interface returns a NAPTR record such as

```
3.7.2.9.1.1.0.e164.arpa. 1      IN      NAPTR   100 10 "u" "E2U+sip" "!^.*$!sip:0119273@tel.arf20.com!" .
```

