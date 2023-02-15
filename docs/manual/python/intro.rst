.. _intro:

Introduction
============

This is the documentation for |var-project|, wrapping the |var-core-project| C API for easy creation of DDS applications.

.. _installing:

Prerequisites
-------------

|var-project-short| requires Python version 3.7 or higher, with 3.11 support provisional. The wheels on PyPi contain a pre-built binary of the |var-core-project| C library and IDL compiler. These have a couple of caveats. The pre-built package:

 * has no support for DDS Security,
 * has no support for shared memory via Iceoryx,
 * comes with generic |var-core-project| binaries that are not optimized per-platform.

If you need these features or cannot use the binaries for other reasons you can install the |var-project| library from source. You will need to set the environment variable ``CYCLONEDDS_HOME`` to allow the installer to locate the |var-core-project| C library if it is on a non-standard path. At runtime we leverage several mechanisms to locate the library that are appropriate for the platform. If you get an exception about non-locatable libraries or wish to manage multiple |var-core-project| installations you can use the environment variable ``CYCLONEDDS_HOME`` to override the load location.

Installation
------------

Simply install with pip from PyPi.

.. code-block:: shell

    pip install cyclonedds


You can install from the github link directly:

.. code-block:: shell

    CYCLONEDDS_HOME="/path/to/cyclonedds" pip install git+https://github.com/eclipse-cyclonedds/cyclonedds-python


If you wish to run the testsuite or build the documentation you will need additional dependencies. These can be installed by means of Python installation optional components:

.. code-block:: shell

    git clone https://github.com/eclipse-cyclonedds/cyclonedds-python
    cd cyclonedds-python

    # Testsuite:
    pip install .[dev]
    pytest

    # Documentation
    pip install .[docs]
    cd docs
    sphinx-build ./source ./build
    python -m http.server --directory build


If you get permission errors you are using your system python. This is not recommended, we recommend using `a virtual environment`_, `poetry`_, `pipenv`_ or `pyenv`_. If you *just* want to get going, you can add ``--user`` to your pip command to install for the current user. See the `Installing Python Modules`_ documentation.

.. _first_app:

Your first Python DDS application
-----------------------------------

Let's make our entry into the world of DDS by making our presence known. We will not worry about configuration or what DDS does under the hood but just write a single message. To publish anything to DDS we need to define the type of message first. If you are worried about talking to other applications that are not necessarily running Python you would use the IDL compiler, but for now we will just manually define our message type directly in Python using the ``cyclonedds.idl`` tools:

.. code-block:: python3
    :linenos:

    from dataclasses import dataclass
    from cyclonedds.idl import IdlStruct

    @dataclass
    class Message(IdlStruct):
        text: str


    name = input("What is your name? ")
    message = Message(text=f"{name} has started their first DDS Python application!")


With ``cyclonedds.idl`` we write typed classes with the standard library module :external+python:py:mod:`~dataclasses`. For this simple application we just put in a piece of text, but this system has the same expressive power as the OMG IDL specification, allowing you to use almost any complex datastructure you can think of.

Now to send our message over DDS we need to perform a few steps:

 * Join the DDS network using a DomainParticipant
 * Define which datatype and under what name we will publish our message as a Topic
 * Make the DataWriter that publishes that Topic
 * And finally publish the message.

.. code-block:: python3
    :linenos:

    from cyclonedds.domain import DomainParticipant
    from cyclonedds.topic import Topic
    from cyclonedds.pub import DataWriter

    participant = DomainParticipant()
    topic = Topic(participant, "Announcements", Message)
    writer = DataWriter(participant, topic)

    writer.write(message)

Hurray, we have published are first message! However, it is hard to tell if that actually did anything, since we don't have anything set up that is listening. Let's make a second script that takes messages from DDS and prints them to the terminal:

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

    # If we don't receive a single announcement for five minutes we want the script to exit.
    for msg in reader.take_iter(timeout=duration(minutes=5)):
        print(msg.text)

Now with this script running in a secondary terminal you should see the message pop up when you run the first script again.

.. _a virtual environment: https://docs.python.org/3/tutorial/venv.html
.. _poetry: https://python-poetry.org/
.. _pipenv: https://pipenv.pypa.io/en/latest/
.. _pyenv: https://github.com/pyenv/pyenv
.. _Installing Python Modules: https://docs.python.org/3/installing/index.html

