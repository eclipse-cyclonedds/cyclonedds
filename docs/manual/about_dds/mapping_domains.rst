.. index:: 
    single: DCPS; Mapping to DDSI domains
    single: DDSI; Mapping DCPS domains
    single: Domains: Mapping DCPS to DDSI

.. _mapping_dcps_to_ddsi:

***************************************
Mapping of DCPS Domains to DDSI Domains
***************************************

In Data-Centric Publish-Subscribe (DCPS), a domain is uniquely identified by a 
non-negative integer, the domain ID. In the UDP/IP mapping, this domain ID is mapped 
to port numbers that are used for communicating with the peer nodes. These port 
numbers are of significance for the discovery protocol. This mapping of domain IDs 
to UDP/IP port numbers ensures that accidental cross-domain communication is 
impossible with the default mapping.

In DCPS there is a one-to-many mapping of domain ID to port numbers. In DDSI, there 
is a one-to-one mapping of domain ID to a port number, which is why DDSI does not 
communicate the DCPS port number in the discovery protocol; it assumes
that each domain ID maps to a unique port number. 

.. note::
    While it is unusual to change the mapping, the specification requires this to be 
    possible, which means that two different DCPS domain IDs can be mapped to a single 
    DDSI domain.