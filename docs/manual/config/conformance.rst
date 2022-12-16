.. _`Conformance modes`:

=================
Conformance Modes
=================

|var-project| operates in one of three modes: *pedantic*, *strict*, and *lax*; the mode is
configured using the :ref:`Compatibility/StandardsConformance <//CycloneDDS/Domain/Compatibility/StandardsConformance>` setting. The default is
*lax*.

The first, *pedantic* mode, is of such limited utility that it will be removed.

The second mode, *strict*, attempts to follow the *intent* of the specification while
staying close to the letter of it. Recent developments at the OMG have resolved these
issues, and this mode is no longer of any value.

The default mode, *lax*, attempts to work around (most of) the deviations of other
implementations, and generally provides good interoperability without any further
settings. In lax mode, the |var-project| not only accepts some invalid messages,
but will even transmit them. The consequences for interoperability of not doing this are
too severe.

.. note::

  If one configures two |var-project| processes with
  different compliancy modes, the one in the stricter mode will complain about messages
  sent by the one in the less strict mode.
