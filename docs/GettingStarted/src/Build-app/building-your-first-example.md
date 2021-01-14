## Building Your First Example

To test the complete workflow of building a DDS based application a simple _Hello World!_ is used. Although this application does not reveal all the power of building a data-centric application, it has the merit to introduce you to the basic steps to build a DDS application.

This chapter focuses on building this example, without analyzing the source code, which will be the subject of the next chapter.

The procedure used to build the _Hello World!_ example can also be used for building your own applications.

On Linux, if you have not specified an installation directory, it is advised to copy the Cyclone DDS examples to your preferred directory. Otherwise, you can find them in your `<install-location> directory` as mentioned in the previous chapter.

Six files are available under the _Hello_ _World!_ root directory to support building the example. And for this chapter, we mainly describe the **CMakeLists.txt, HelloWorldData.idl, publisher.c,** and **subscriber.c**.