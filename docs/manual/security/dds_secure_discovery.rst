.. index:: 
    single: DDS security; Secure discovery
    single: Security; Secure discovery

DDS secure discovery
********************

When DDS Security is enabled, |var-project-short| discovers remote participants, 
topics, readers and writers as normal. However, if the system contains a number of slow 
platforms, or the platform is large, discovery can take longer to due to the more 
complex handshaking that is required. The effort to perform discovery grows 
quadratically with the number of nodes.
