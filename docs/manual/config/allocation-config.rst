.. _allocation_config:

*********************
Memory Allocation Configuration
*********************

Users have the flexibility to customize the memory allocation strategy for |var-project| by leveraging their own memory allocation libraries. To utilise your preferred memory allocation functions, follow these steps:

1. Include `dds/ddsrt/heap.h`
   To gain access to `ddsrt_allocation_ops_t` and `ddsrt_set_allocator`.

2. Set the allocation operations
   Define a `ddsrt_allocation_ops_t` structure that contains pointers to the functions you want to use for memory allocation and deallocation (`malloc`, `calloc`, `realloc`, and `free`).

3. Provide `ddsrt` the operations
   Utilize the `ddsrt_set_allocator` function to set the ops chosen in step 2.

4. Link with custom library
   Compile and link your application with the library that contains the functions specified in the `ddsrt_allocation_ops_t` structure created in step 2.

By following these steps, you can tailor the memory allocation mechanism of |var-project| to meet your specific requirements.
