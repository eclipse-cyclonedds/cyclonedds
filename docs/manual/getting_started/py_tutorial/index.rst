###############
Python Tutorial
###############

Let's enter the world of DDS by making our presence known. We will not worry about configuration or what DDS does under the hood but write a single message.
To publish anything to DDS we need to define the type of message first. Suppose you are worried about talking to other applications that are not necessarily running Python. In that case, you will use the  IDL compiler, but for now, we will manually define our message type directly in Python using the ``cyclonedds.idl`` tools:

.. code-block:: python3
    :linenos:

    from dataclasses import dataclass
    from cyclonedds.idl import IdlStruct

    @dataclass
    class Message(IdlStruct):
        text: str


    name = input("What is your name? ")
    message = Message(text=f"{name} has started his first DDS Python application!")


With ``cyclonedds.idl`` write typed classes with the standard library module `dataclasses <python:dataclasses>`. For this simple application, the data being transmitted only contains a piece of text, but this system has the same expressive power as the OMG IDL specification, allowing you to use almost any complex datastructure.

To send your message over a DDS domain, carry out the following steps:

1. Join the DDS network using a DomainParticipant
2. Define which data type and under what name you will publish your message as a Topic
3. Create the ``DataWriter`` that publishes that Topic
4. And finally, publish the message.


.. code-block:: python3
    :linenos:

    from cyclonedds.topic import Topic
    from cyclonedds.pub import DataWriter

    participant = DomainParticipant()
    topic = Topic(participant, "Announcements", Message)
    writer = DataWriter(participant, topic)

    writer.write(message)

You have now published your first message successfully! However, it is hard to tell if that did anything since we don't have anything set up to listen for incoming messages. Let's make a second script that takes messages from DDS and prints them to the terminal:

.. code-block:: python3
    :linenos:

    from dataclasses import dataclass
    from cyclonedds.domain import DomainParticipant
    from cyclonedds.topic import Topic
    from cyclonedds.sub import DataReader
    from cyclonedds.util import duration
    from cyclonedds.idl import IdlStruct

    @dataclass
    class Message(IdlStruct):
        text: str

    participant = DomainParticipant()
    topic = Topic(participant, "Announcements", Message)
    reader = DataReader(participant, topic)

    # If we don't receive a single announcement for five minutes, we want the script to exit.
    for msg in reader.take_iter(timeout=duration(minutes=5)):
        print(msg.text)

Now with this script running in a second terminal, you should see the message pop up when you rerun the first script.
