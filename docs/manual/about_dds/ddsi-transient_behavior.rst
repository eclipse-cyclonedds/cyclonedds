.. _`DDSI-specific transient-local behaviour`:

***************************************
DDSI-Specific Transient-Local Behaviour
***************************************

The DCPS specification provides *transient-local*, *transient*,
and *persistent* data. The DDSI specification only provides *transient-local*, 
which is the only form of durable data available when inter-operating across vendors.

In DDSI, transient-local data is implemented using the :index:`Writer History Cache` 
(WHC) that is normally used for reliable communication. For transient-local data, 
samples are retained even when all Readers have acknowledged them. The default history 
setting of ``KEEP_LAST`` with ``history_depth = 1``, means that late-joining 
Readers can still obtain the latest sample for each existing instance.

When the DCPS Writer is deleted (or is unavailable), the DDSI Writer and its history 
are also lost. For this reason, transient data is typically preferred over 
transient-local data. 

.. note::
    |var-project| has a facility for retrieving transient data from a suitably configured 
    OpenSplice node, but does not include a native service for managing transient data.
