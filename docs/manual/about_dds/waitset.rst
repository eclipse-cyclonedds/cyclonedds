..
   Copyright(c) 2022 ZettaScale Technology and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

   SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

.. index:: Waitsets

.. _waitsets_bm:

Waitsets
========

The Waitsets tool in |var-project-short| holds execution of the program until a 
specific condition is reached, or an amount of time has expired. Waitsets and 
:ref:`listeners <listeners_bm>` have two different requirement sets:

- A Waitset allows the code to react when nothing changes.
- Listeners can only react to changes in state. Listeners allow instant callbacks to 
  specific changes. 

A StatusCondition (linked to an entity) signals to the waitset that its wait is finished.
The StatusCondition has a list of conditions (changes in status) that can trigger the 
waitset to finish its wait. When creating a StatusCondition, the enabled statuses 
should match the type of entity it is attached to. Different StatusConditions linked 
to different Entities can be attached to a Waitset through the ``attach_condition`` 
function, and detached through the ``detach_condition`` function. After creating a 
waitset with StatusConditions attached, a wait can be triggered for the Duration 
specified. Triggering the wait causes one of two things to happen:

- The wait will time out, causing an exception of the type ``dds::core::TimeoutError``.
- A status change of one of the attached conditions occurs, and the returned container 
  contains the conditions that have triggered the end of the wait.

The following code does not work because the ``subscription_matched`` is a status 
associated with a DataReader:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code-block:: C++

			dds::sub::DataReader<DataType> reader(sub, topic);
			dds::core::cond::StatusCondition rsc(reader);
			rsc.enabled_statuses(dds::core::status::StatusMask::subscription_matched());

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD


The following code does not work because the ``liveliness_lost`` is not a status associated
with a Topic:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C

			C code sample TBD

    .. group-tab:: C++

		.. code-block:: C++

			dds::topic::Topic<DataType> topic(participant,"topic");
			dds::core::cond::StatusCondition tsc(topic);
			tsc.enabled_statuses(dds::core::status::StatusMask::liveliness_lost());

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD

The following code attempts to wait a specified amount of time for readers and writers 
to see their counterparts, allowing two-way communication to occur:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code-block:: C++

			template<typename T>
			bool match_reader_and_writer(dds::sub::DataReader<T> &rd, dds::pub::DataWriter<T> &wr, const dds::core::Duration &dur) {
				try {

					dds::core::cond::StatusCondition wsc(wr), rsc(rd);
					wsc.enabled_statuses(dds::core::status::StatusMask::publication_matched());
					rsc.enabled_statuses(dds::core::status::StatusMask::subscription_matched());

					dds::core::cond::WaitSet waitset;
					waitset.attach_condition(wsc);
					waitset.attach_condition(rsc);

					auto start = std::chrono::steady_clock::now();
					auto result = waitset.wait(dur);

					bool is_pub = false;
					if (result.empty()) {
						return false;
					} if (result[0] == wsc) {
						is_pub = true;
						waitset.detach_condition(wsc);
					} else if (result[0] == rsc) {
						waitset.detach_condition(rsc);
					} else {
						return false;
					}

					auto diff = std::chrono::steady_clock::now()-start;
					dur -= dds::core::Duration(std::chrono::duration_cast<std::chrono::seconds>(diff).count(), std::chrono::duration_cast<std::chrono::nanoseconds>(diff).count()%1000000000);
					result = waitset.wait(dur);

					if (result.empty() || (is_pub && result[0] != rsc) || (!is_pub && result[0] != wsc))
						return false;
				} catch (const dds::core::TimeoutError &) {
					return false;
				}
				return true;
			}

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD

The above function returns true if the reader and writer have encountered matching 
publications and subscriptions before the timeout's duration expired, and false otherwise.
