.. _network_config:

*********************
Network configuration
*********************

.. toctree::
   :maxdepth: 1
   :hidden:
   
   network_interfaces
   network_partitions
   port_numbers
   multicasting
   tcp_support
   tls_support
   ethernet_support

|var-project| provides mechanisms to configure the network infrastructure that it 
operate against. This includes:

- Configuring the properties of the underlying network: :ref:`networking_interfaces` 
  and :ref:`port_numbers`.

- Configuring the network communication protocols: :term:`UDP`, :ref:`tcp_support`, 
  or :ref:`ethernet_support`.

- Controlling where |var-project-short| uses :ref:`multicasting_bm`, and across which 
  configured network interfaces.

- Configuring the use of :ref:`tls_support` to allow for mutual authentication and the 
  encryption of data across the network.