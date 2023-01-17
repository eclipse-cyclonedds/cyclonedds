.. index:: 
  single: DDS security; Logging and Tracing
  single: Security; Logging and Tracing
  single: Logging
  single: Tracing

Logging and tracing
*******************

The security implementation uses the standard logging and tracing mechanism in |var-project-short|.
The following is written to log and trace:

- **Configuration errors**: plugin library files, certificate files, governance, and permissions
  files that can not be found on filesystem.
- **Permission errors**: denied permission for creating writer of a topic.
- **Attribute mismatch errors**: mismatches of security attributes between participants, topics,
  readers and writers.
- **Integrity errors**: Permissions file-Permissions CA and Identity Cert-Identity CA integrity.

Local subscription, publication and topic permission errors are written as errors.
Remote participation, subscription and publication permission errors are written to log as
warning messages.