.. index:: IDL

.. _idl_bm:

===
IDL
===

.. toctree::
   :maxdepth: 1
   :hidden:

   idl_compilers

IDL is a language to defined data types and interfaces that are platform and language 
agnostic. There are tools that have been developed to then consume that IDL and emit 
code and infrastructure that allows you target a specific platform or language.

All IDL type support is contained within the subpackage `cyclonedds.idl`, which 
enables you to use it even in contexts where you do not need the full 
|var-project| ecosystem.

For example, the following IDL code:

.. code-block:: shell

   module HelloWorldData
   {
   struct Msg
   {
      @key
      long userID;
      string message;
   };
   };

``idlc`` (the :ref:`idl_compiler`) converts this to:

.. tabs::

   .. group-tab:: Core DDS (C)
			
      .. code-block:: C

            typedef struct HelloWorldData_Msg
            {
               int32_t userID;
               char * message;
            } HelloWorldData_Msg;

   .. group-tab:: C++

      .. code-block:: C++

         namespace HelloWorldData
         {
            class Msg OSPL_DDS_FINAL
            {
            public:
               int32_t userID_;
               std::string message_;
            /** SNIP THE REST OF THE METHODS **/
            }
         }

   .. group-tab:: Python

      .. code-block:: python

         @dataclass
         class HelloWorld(IdlStruct, typename="HelloWorld.Msg"):
            usedID: int32
            message: str

See also: :ref:`helloworld_idl`.