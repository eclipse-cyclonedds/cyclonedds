#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dds/dds.h"
#include "dds/ddsrt/xmlparser.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/heap.h"

#include "domtree.h"

struct proc_arg {
  const char *fname;
  struct elem *root;
};

static int proc_elem_open (void *varg, uintptr_t parentinfo, uintptr_t *eleminfo, const char *name, int line)
{
  struct proc_arg * const arg = varg;
  struct elem *elem = ddsrt_malloc (sizeof (*elem));
  elem->file = arg->fname;
  elem->line = line;
  elem->name = ddsrt_strdup (name);
  elem->data = NULL;
  elem->attributes = NULL;
  elem->last_attr = NULL;
  elem->children = NULL;
  elem->last_child = NULL;
  elem->next = NULL;
  if (parentinfo)
    elem->parent = (struct elem *) parentinfo;
  else
  {
    elem->parent = NULL;
    arg->root = elem;
  }
  if (elem->parent)
  {
    if (elem->parent->children)
      elem->parent->last_child->next = elem;
    else
      elem->parent->children = elem;
    elem->parent->last_child = elem;
  }
  *eleminfo = (uintptr_t) elem;
  return 0;
}

static int proc_attr (void *varg, uintptr_t eleminfo, const char *name, const char *value, int line)
{
  struct proc_arg * const arg = varg;
  struct attr *attr = ddsrt_malloc (sizeof (*attr));
  struct elem *elem = (struct elem *) eleminfo;
  attr->file = arg->fname;
  attr->line = line;
  attr->name = ddsrt_strdup (name);
  attr->value = ddsrt_strdup (value);
  attr->next = NULL;
  if (elem->attributes)
    elem->last_attr->next = attr;
  else
    elem->attributes = attr;
  elem->last_attr = attr;
  return 0;
}

static int proc_elem_data (void *varg, uintptr_t eleminfo, const char *data, int line)
{
  (void) varg; (void) line;
  struct elem *elem = (struct elem *) eleminfo;
  elem->data = ddsrt_strdup (data);
  return 0;
}

static int proc_elem_close (void *varg, uintptr_t eleminfo, int line)
{
  (void) varg; (void) line; (void) eleminfo;
  return 0;
}

static void xml_error (void *varg, const char *msg, int line)
{
  (void) varg;
  fprintf (stderr, "%d:%s\n", line, msg);
  exit (1);
}

static void print_elem (const struct elem *elem, int indent)
{
  printf ("%*s<%s", indent, "", elem->name);
  for (const struct attr *a = elem->attributes; a; a = a->next)
    printf (" %s=\"%s\"", a->name, a->value);
  if (elem->data == NULL && elem->children == NULL)
    printf ("/>\n");
  else
  {
    printf (">");
    const bool newline =  (elem->children || strlen (elem->data) > 20);
    if (newline)
      printf ("\n");
    for (const struct elem *e = elem->children; e; e = e->next)
      print_elem (e, indent + 2);
    if (elem->data)
    {
      printf ("%*s%s", newline ? indent : 0, "", elem->data);
      if (newline)
        printf ("\n");
    }
    printf ("%*s</%s>\n", newline ? indent : 0, "", elem->name);
  }
}

const char *getattr (const struct elem *elem, const char *name)
{
  for (const struct attr *a = elem->attributes; a != NULL; a = a->next)
    if (strcmp (a->name, name) == 0)
      return a->value;
  return NULL;
}

struct elem *domtree_from_file (const char *fname)
{
  FILE *fp = fopen (fname, "r");
  if (fp == NULL)
    return NULL;
  const struct ddsrt_xmlp_callbacks xmlp_cb = {
    .elem_open = proc_elem_open,
    .attr = proc_attr,
    .elem_data = proc_elem_data,
    .elem_close = proc_elem_close,
    .error = xml_error
  };
  struct proc_arg proc_arg = {
    .fname = fname,
    .root = NULL
  };
  struct ddsrt_xmlp_state *xmlp_st = ddsrt_xmlp_new_file (fp, &proc_arg, &xmlp_cb);
  if (ddsrt_xmlp_parse (xmlp_st) < 0)
    proc_arg.root = NULL; // FIXME: leaks a bit
  ddsrt_xmlp_free (xmlp_st);
  fclose (fp);
  return proc_arg.root;
}

void domtree_print (const struct elem *root)
{
  print_elem (root, 0);
}
