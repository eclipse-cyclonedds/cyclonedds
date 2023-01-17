.. include:: ../external-links.part.rst

.. index:: ! Port numbers

.. _port_numbers:

============
Port numbers
============

The |var-project| port numbers are configured as follows: 

.. note::
  The first two items are defined by the |url::ddsi_spec|. The third item is unique to 
  |var-project| as a way of serving multiple participants by a single DDSI instance.

- Two "well-known" multicast ports: ``B`` and ``B+1``.
- Two unicast ports at which only this instance is listening: ``B+PG*PI+10`` and
  ``B+PG*PI+11``
- One unicast port per domain participant it serves, chosen by the kernel from the list of 
  anonymous ports, that is, >= 32768.

where:

- *B* is :ref:`Discovery/Ports/Base <//CycloneDDS/Domain/Discovery/Ports/Base>` (``7400``) + :ref:`Discovery/Ports/DomainGain <//CycloneDDS/Domain/Discovery/Ports/DomainGain>`
  (``250``) * :ref:`Domain[@Id] <//CycloneDDS/Domain[@Id]>`
- *PG* is :ref:`Discovery/Ports/ParticipantGain <//CycloneDDS/Domain/Discovery/Ports/ParticipantGain>` (``2``)
- *PI* is :ref:`Discovery/ParticipantIndex <//CycloneDDS/Domain/Discovery/ParticipantIndex>`

The default values (taken from the DDSI specification) are in parentheses.

.. note:: 
  This shows only a sub-set of the available parameters. The other parameters in the 
  specification have no bearing on |var-project|. However, these are configurable. For
  further information, refer to the |url::dds2.1| or |url::dds2.2| specification, section 9.6.1.

*PI* relates to having multiple processes in the same domain on a single node. Its 
configured value is either *auto*, *none* or a non-negative integer. This setting matters:

+ *none* (default): It ignores the "participant index" altogether and asks the kernel to pick 
  random ports (>= 32768). This eliminates the limit on the number of standalone deployments 
  on a single machine and works well with multicast discovery, while complying with all other 
  parts of the specification for interoperability. However, it is incompatible with unicast discovery.
+ *auto*: |var-project| polls UDP port numbers on start-up, starting with ``PI = 0``, incrementing 
  it by one each time until it finds a pair of available port numbers, or it hits the limit. 
  To limit the cost of unicast discovery, the maximum PI is set in: 
  :ref:`Discovery/MaxAutoParticipantIndex <//CycloneDDS/Domain/Discovery/MaxAutoParticipantIndex>`.
+ *non-negative integer*: It is the value of PI in the above calculations. If multiple processes 
  on a single machine are required, they need unique values for PI, and therefore for standalone 
  deployments, this alternative is of little use.

To fully control port numbers, setting (= PI) to a hard-coded value is the only possibility. 
:ref:`Discovery/ParticipantIndex <//CycloneDDS/Domain/Discovery/ParticipantIndex>` 
By defining PI, the port numbers needed for unicast discovery are fixed as well. This allows 
listing peers as IP:PORT pairs, which significantly reduces traffic.

The other non-fixed ports that are used are the per-domain participant ports, the third
item in the list. These are used only because there exist some DDSI implementations
that assume each domain participant advertises a unique port number as part of the
discovery protocol, and hence that there is never any need for including an explicit
destination participant ID when intending to address a single domain participant by
using its unicast locator. |var-project| never makes this assumption, instead opting to
send a few bytes extra to ensure the contents of a message are all that is needed. With
other implementations, you will need to check.

If all DDSI implementations in the network include full addressing information in the
messages like |var-project| does, then the per-domain participant ports serve no purpose
at all. The default ``false`` setting of :ref:`Compatibility/ManySocketsMode <//CycloneDDS/Domain/Compatibility/ManySocketsMode>` disables the
creation of these ports.

This setting can have a few other side benefits, as there may be multiple
DCPS participants using the same unicast locator. This improves the chances of a single
unicast sufficing even when addressing multiple participants.
