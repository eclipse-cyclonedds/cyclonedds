#include <stdio.h>
#include <stdint.h>

typedef void idl_tree_t;

#if WIN32
__declspec(dllexport)
#endif
int32_t generate(idl_tree_t *tree, const char *str);


int32_t generate(idl_tree_t *tree, const char *str)
{
  (void)tree;
  fprintf(stderr, "From %s: %s\n", __FILE__, str);
  return 0;
}
