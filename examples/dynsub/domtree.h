// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DOMTREE_H
#define DOMTREE_H

struct attr {
  const char *file;
  int line;
  char *name;
  char *value;
  struct attr *next;
};

struct elem {
  const char *file;
  int line;
  char *name;
  char *data;
  struct attr *attributes, *last_attr;
  struct elem *children, *last_child;
  struct elem *parent;
  struct elem *next;
};

const char *getattr (const struct elem *elem, const char *name);
struct elem *domtree_from_file (const char *fname);
void domtree_print (const struct elem *elem);

#endif
