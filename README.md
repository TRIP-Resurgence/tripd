<!--! [TOC] -->

# tripd

Modern TRIP (RFC 3219) LS routing daemon implementation in C99

DISCLAIMER: All code here is super untested

## [Design](docs/DESIGN.md)

## [Install](docs/INSTALL.md)

## Resources

 - [RFC 1771 (1995) A Border Gateway Protocol 4 (BGP-4)](https://datatracker.ietf.org/doc/html/rfc1771)
 - [RFC 2871 (2000) A Framework for Telephony Routing over IP](https://datatracker.ietf.org/doc/html/rfc2871)
 - [RFC 3219 (2002) Telephony Routing over IP (TRIP)](https://datatracker.ietf.org/doc/html/rfc3219)
 - [RFC 5115 (2008) Telephony Routing over IP (TRIP) Attribute for Resource Priority](https://datatracker.ietf.org/doc/html/rfc5115)
 - [RFC 5140 (2008) A Telephony Gateway REgistration Protocol (TGREP)](https://datatracker.ietf.org/doc/html/rfc5140)
 - [Vovida Networks (2003) VOCAL](https://web.archive.org/web/20070918023126/http://www.vovida.org/downloads/vocal/1.5.0/vocal-1.5.0.tar.gz) C++98 TRIP implementation, almost lost media
 - [Columbia University TRIP implementation documentation](https://www.cs.columbia.edu/~hgs/research/projects/TRIP-LS/TRIP.html) code is lost media
 - [Cisco (2000) TRIP Tutorial at Packetizer.com](https://www.packetizer.com/voip/trip/)
 - [Nicklas Beijar (2001) TRIP, ENUM and Number Portability](https://www.netlab.tkk.fi/opetus/s38130/k01/Papers/Beijar-TripEnumNp.pdf)
 - [Matthew C. Schlesener (2002) Performance Evaluation of Telephony Routing over IP (TRIP)](http://ittc.ku.edu/research/thesis/documents/matt_schlesener_thesis.pdf)
 - [David Kelly (2002) Practical VoIP Using VOCAL](https://archive.org/details/practicalvoipusi00dang_0)

### TRIP vs ENUM

From Practical VoIP Using VOCAL

```
Many people seem to be confused about the differences between ENUM and TRIP.
Let’s try to clarify these differences.

ENUM is an address resolution protocol that lets you translate a phone number into
a URI. TRIP is a routing protocol that lets you obtain routing information for a par-
ticular
telephone number prefix. With TRIP, you discover where the next hop
should be in your routing of telephone numbers. With ENUM, you discover a map-
ping between a phone number and an IP entity.

ENUM answers the question, “Is there a URI (for example, an IP phone) associated
with the number that the user is dialing?” The answer to this question helps avoid
routing a call from an IP phone over the PSTN to another IP phone. If the system
knows that the far end has a URI, it can do a DNS lookup for that URI and contact it
directly over IP.

If the number doesn’t have an associated URI, TRIP allows you to find the best rate
to get to that phone number. You use ENUM first to find out if there is a URI; if
there is no match, then you use TRIP, which is a mechanism used to communicate
that a specific gateway handles a specific PSTN prefix or a specific range of PSTN
numbers.

Neither protocol has any use if you’re dialing user_name@vovida.org. These proto-
cols have to do with phone numbers, not URI dialing.
```

