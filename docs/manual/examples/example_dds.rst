.. index:: 
	single: Examples; Example DDS DataType
	single: DataType; DDS Example
	single: DDS; Example DataType

.. _datatype_example:

===========
Example DDS
===========

The following is example code for writing data of the type ``DataType``. The writer waits 
for a reader to appear and then writes a single sample to the DDS service. It then 
waits for the reader to disappear and then exits:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code-block:: C++

			/* for std::this_thread */
			#include <thread>

			/* include C++ DDS API. */
			#include "dds/dds.hpp"

			/* include the c++ data type, generated from idlcxx */
			#include "DataType.hpp"

			using namespace org::eclipse::cyclonedds;

			int main() {
				/*errors in construction/etc are indicated by exceptions*/
				try {
					dds::domain::DomainParticipant participant(domain::default_id());

					dds::topic::Topic<DataType> topic(participant, "DataType Topic");

					dds::pub::Publisher publisher(participant);

					dds::pub::DataWriter<DataType> writer(publisher, topic);

					/*we wait for a reader to appear*/
					while (writer.publication_matched_status().current_count() == 0)
						std::this_thread::sleep_for(std::chrono::milliseconds(20));

					DataType msg;

					/*modify msg*/

					writer.write(msg);

					/*we wait for the reader to disappear*/
					while (writer.publication_matched_status().current_count() > 0)
						std::this_thread::sleep_for(std::chrono::milliseconds(50));
				} catch (const dds::core::Exception& e) {
					std::cerr << "An exception occurred: " << e.what() << std::endl;
					exit(1);
				}
				return 0;
			}

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD


The reader periodically checks (every 20ms) for received data. When it receives some data, it stops:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code-block:: C++

			/* for std::this_thread */
			#include <thread>

			/* include C++ DDS API. */
			#include "dds/dds.hpp"

			/* include the c++ data type, generated from idlcxx */
			#include "DataType.hpp"

			using namespace org::eclipse::cyclonedds;

			int main() {

				/*errors in construction/etc are indicated by exceptions*/
				try {
					dds::domain::DomainParticipant participant(domain::default_id());

					dds::topic::Topic<DataType> topic(participant, "DataType Topic");

					dds::sub::Subscriber subscriber(participant);

					dds::sub::DataReader<DataType> reader(subscriber, topic);

					/*we periodically check the reader for new samples*/
					bool reading = true;
					while (reading) {
						std::this_thread::sleep_for(std::chrono::milliseconds(20));
						auto samples = reader.take();
						for (const auto & p:samples) {
							const auto& info = p.info(); /*metadata*/
							if (info.valid()) {
								/*this sample contains valid data*/
								const auto& msg = p.data(); /* the actual data */
								std::cout << "Message received." << std::endl;
								reading = false; /*we are done reading*/
							}
						}
					}
				} catch (const dds::core::Exception& e) {
					std::cerr << "An exception occurred: " << e.what() << std::endl;
					exit(1);
				}
				return 0;
			}

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD
