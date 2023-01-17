.. include:: ../external-links.part.rst

.. index:: Loan mechanism, Memory exchange; Loan mechanism

.. _loan_machanism:

Loan mechanism
==============

When using shared memory exchange, additional performance gains can be made by using 
the loan mechanism on the writer side. The loan mechanism directly allocates memory 
from the |url::iceoryx_link| shared memory pool, and provides this to the user as a 
message data type. This eliminates a copy step in the publication process.

.. highlight:: c

.. code-block:: c

  struct message_type *loaned_sample;
  dds_return_t status = dds_loan_sample (writer, (void**)&loaned_sample);

If *status* returns :c:macro:`DDS_RETCODE_OK`, then *loaned_sample* contains a pointer 
to the memory pool object, in all other cases, *loaned_sample* should not be dereferenced.

Necessary information about the data type is supplied by the writer. When requesting 
loaned samples, the writer used to request the loaned sample must be the same data 
type as the sample that you are writing in it.

When requesting loaned samples, the maximum number of outstanding loans is defined by 
**MAX_PUB_LOANS** (default set to 8). This is the maximum number of loaned samples that 
each publisher can have outstanding from the shared memory. If the maximum is reached, 
to request new loaned samples, some must be returned (handed back to the publisher) 
through :c:func:`dds_write()`.

.. note::
  When a loaned sample has been returned to the shared memory pool (by invoking 
  :c:func:`dds_write()`), dereferencing the pointer is undefined behaviour.

If |var-project-short| is configured to use shared memory, but it is not possible to 
use the loan mechanism, a :c:func:`dds_write()` still writes to the shared memory 
service. This increases the overhead by an additional copy step in publication.
That is, when a block for publishing to the shared memory is requested, the data 
of the published sample is copied into it.

