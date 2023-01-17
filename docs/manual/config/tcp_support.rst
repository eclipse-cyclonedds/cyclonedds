.. include:: ../external-links.part.rst

.. index:: ! TCP
  
.. _tcp_support:

===========
TCP support
===========

The |url::ddsi_spec| is a protocol that provides a connectionless transport with
unreliable datagrams. However, there are times where :term:`TCP` is the only
practical network transport available (for example, across a WAN). This is the reason
|var-project| can use TCP instead of :term:`UDP` if needed.

The differences in the model of operation between DDSI and TCP are quite large: DDSI is
based on the notion of peers, whereas TCP communication is based on the notion of a
session that is initiated by a "client" and accepted by a "server"; therefore, TCP requires
knowledge of the servers to connect to before the DDSI discovery protocol can exchange
that information. The configuration of this is done in the same manner as for
unicast-based UDP discovery.

TCP reliability is defined in terms of these sessions, but DDSI reliability is defined
in terms of DDSI discovery and liveliness management. It is therefore possible that a
TCP connection is (forcibly) closed while the remote endpoint is still considered alive.
Following a reconnect, the samples lost when the TCP connection was closed can be
recovered via the standard DDSI reliability. This also means that the Heartbeats and
AckNacks still need to be sent over a TCP connection, and consequently that DDSI
flow-control occurs on top of TCP flow-control.

Connection establishment potentially takes a long time, and giving up on a transmission 
to a failed, or no longer reachable host, can also take a long time. These long delays 
can be visible at the application level.