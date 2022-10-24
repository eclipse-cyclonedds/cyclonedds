Installation remarks
====================

Environment variable updates on Windows
---------------------------------------

To run |var-project-short| executables on Windows, the required libraries (like ``ddsc.dll``) must be available to the executables.
Typically, these libraries are installed in system default locations and work out of the box.
However, the library search path must be changed if they are not installed in those locations. This can be achieved by
executing the following command or going into the "Environment variables" Windows menu.

.. code-block:: PowerShell

    set PATH=<install-location>\bin;%PATH%


.. note::

    An alternative to make the required libraries available to the executables are to copy the necessary libraries for the
    executables' directory. This is not recommended.
