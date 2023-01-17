.. index:: 
    single: Data communication
    single: Handshake process
    single: Security; Data communication and handshake process
    single: DDS security; Data communication and handshake process

Data communication and handshake process
****************************************

An authentication handshake between participants starts after participant discovery. If a reader and
a writer are created during that period, their match is delayed until after the handshake succeeds.

.. warning::
    During the handshake process, volatile data is lost (as if there is no reader).

After publication match, the encryption / decryption keys are exchanged between the reader and writer.
Best-effort data that is sent during this exchange is lost. Reliable data is resent.