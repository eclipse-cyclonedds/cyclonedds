Windows environment variables
=============================

To run |var-project-short| executables on Windows, the required libraries (``ddsc.dll`` and so on) must be available to the executables.
Typically, these libraries are installed in system default locations and work out of the box.
However, if they are not installed in those locations, you must change the library search path, either: 

- Execute the following command:

.. code-block:: console

    set PATH=<install-location>\bin;%PATH%

- Set the path from the "Environment variables" Windows menu.

.. note::

    An alternative to make the required libraries available to the executables are to copy the necessary libraries for the
    executables' directory. This is not recommended.
