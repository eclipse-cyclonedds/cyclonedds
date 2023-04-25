// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "idl/heap.h"
#include "idl/string.h"

#include "config.h"
#include "options.h"

static void print_description (const char *desc, int indent, int init_indent, int maxwidth)
{
  int cindent = indent;
  int pos = init_indent;
  while (*desc)
  {
    if (*desc == '\n')
    {
      int n = (int)strspn (desc + 1, " ");
      desc += 1 + n;
      cindent = indent + n;
      printf ("\n%*s", cindent, "");
      pos = cindent;
    }
    else
    {
      int n = (int)strcspn (desc, " \t\n");
      if (pos + n > maxwidth)
      {
  printf ("\n%*s", cindent, "");
  pos = cindent;
      }
      printf ("%*s%*.*s", (pos == cindent) ? 0 : 1, "", n, n, desc);
      pos += n + ((pos == cindent) ? 0 : 1);
      desc += n + (int)strspn (desc + n, " ");
    }
    while (*desc == '\t')
    {
      int n = 8 - (pos % 8);
      desc++;
      pos += printf ("%*.*s", n, n, "");
      cindent = pos;
    }
  }
  printf ("\n");
}

static int ascending(const void *va, const void *vb)
{
  const idlc_option_t *const *const a = va;
  const idlc_option_t *const *const b = vb;
  int la = idl_tolower((*a)->option);
  int lb = idl_tolower((*b)->option);
  if (la != lb)
    return la - lb;
  if ((*a)->option != (*b)->option)
    return la == (*a)->option ? -1 : 1;
  if (!(*a)->suboption)
    return !(*b)->suboption ? 0 : -1;
  if (!(*b)->suboption)
    return 1;
  return idl_strcasecmp((*a)->suboption, (*b)->suboption);
}

static int descending(const void *va, const void *vb)
{
  return -ascending(va, vb);
}

static idlc_option_t **sort_options(
  idlc_option_t **options, int(*cmp)(const void *, const void *))
{
  size_t len;
  idlc_option_t **vec;

  for (len=0; options[len]; len++) ;
  if (!(vec = idl_malloc((len+1) * sizeof(*vec))))
    return NULL;
  memcpy(vec, options, len * sizeof(*vec));
  vec[len] = NULL;
  qsort(vec, len, sizeof(*vec), cmp);
  return vec;
}

static bool isempty(const char *str)
{
  return !(str && *str);
}

static int format_option(
  char *str, size_t size, int indent, const idlc_option_t *option)
{
  int opt = option->option;
  const char *arg = option->argument;
  const char *subopt = option->suboption;

  if (!isempty(subopt) && !isempty(arg) && option->type)
    return snprintf(str, size, "%-*s-%c %s=%s", indent, "", opt, subopt, arg);
  if (!isempty(subopt))
    return snprintf(str, size, "%-*s-%c %s", indent, "", opt, subopt);
  if (!isempty(option->argument) && option->type)
    return snprintf(str, size, "%-*s-%c %s", indent, "", opt, arg);
  return snprintf(str, size, "%-*s-%c", indent, "",  opt);
}

#define WIDTH (78)
#define INDENT (25)

void print_help(
  const char *argv0, const char *rest, idlc_option_t **options)
{
  idlc_option_t **opts;
  printf("Usage: %s%s%s\n", argv0, rest ? " " : "", rest ? rest : "");
  if (!(opts = sort_options(options, &ascending)))
    return;
  printf("Options:\n");
  for (size_t i=0; opts[i]; i++) {
    int cnt, off = INDENT;
    char buf[WIDTH + 1];
    cnt = format_option(buf, sizeof(buf), 2, opts[i]);
    if (cnt <= off)
      printf("%-*s", off, buf);
    else
      printf("%s\n%*s", buf, off, "");
    print_description(opts[i]->help, off, off, WIDTH);
  }
  idl_free(opts);
}

void print_usage(
  const char *argv0, const char *rest)
{
  fprintf(stderr, "Usage: %s%s%s\n", argv0, rest ? " " : "", rest ? rest : "");
  fprintf(stderr, "Try '%s -h' for more information.\n", argv0);
}

static void print_error(
  const char *argv0, const char *errstr, char opt, const char *subopt)
{
  if (!isempty(subopt)) {
    int len = (int)strcspn(subopt, "=");
    fprintf(stderr,"%s%s%c %.*s\n",argv0, errstr, opt, len, subopt);
  } else {
    fprintf(stderr,"%s%s%c\n",argv0, errstr, opt);
  }
}

static int matches_option(
  int opt, const char *arg, const idlc_option_t *option)
{
  char chr;
  size_t len;

  if (option->option != opt)
    return -1;
  if (!option->suboption || !*option->suboption)
    return 0;
  len = strlen(option->suboption);
  if (!arg || strncmp(option->suboption, arg, len) != 0)
    return -1;
  chr = option->type ? '=' : '\0';
  return arg[len] == chr ? (int)len + (chr == '=') : -1;
}

static int handle_option(
  int opt, const char *arg, const idlc_option_t *option)
{
  (void)opt;
  switch (option->type) {
    case IDLC_FLAG:
      *option->store.flag = 1;
      break;
    case IDLC_STRING:
      *option->store.string = arg;
      break;
    case IDLC_FUNCTION:
      return (option->store.function)(option, arg);
  }
  return 0;
}

static int handle_options(
  int argc, char **argv, const char *optstr, idlc_option_t **options)
{
  int opt, off, ret;

  while ((opt = getopt(argc, argv, optstr)) != EOF) {
    size_t i;
    if (opt == '?')
      return IDLC_BAD_OPTION;
    if (opt == ':')
      return IDLC_NO_ARGUMENT;
    for (i=0; options[i]; i++) {
      if ((off = matches_option(opt, optarg, options[i])) == -1)
        continue;
      assert(off >= 0);
      assert(optarg || off == 0);
      if (!(ret = handle_option(opt, optarg ? optarg+off : NULL, options[i])))
        break;
      else if (ret == IDLC_NO_ARGUMENT)
        print_error(argv[0], ": option requires an argument -- ", (char)opt, optarg);
      else if (ret == IDLC_BAD_ARGUMENT)
        print_error(argv[0], ": illegal argument -- ", (char)opt, optarg);
      return ret;
    }
    if (options[i])
      continue;
    print_error(argv[0], ": illegal option -- ", (char)opt, optarg);
    return IDLC_BAD_OPTION;
  }
  return 0;
}

static int make_optstring(idlc_option_t **options, char **optstrp)
{
  char *str;
  unsigned char opt, seen[256], expect;
  size_t len = 0, pos = 0;

  memset(seen, 0, sizeof(seen));
  for (size_t i=0; options[i]; i++) {
    opt = (unsigned char)options[i]->option;
    expect = 1;
    if (!isempty(options[i]->suboption))
      expect = 2;
    else if (options[i]->type && options[i]->argument)
      expect = 3;
    if (!seen[opt])
      seen[opt] = expect;
    else if (seen[opt] != expect)
      return IDLC_BAD_INPUT;
    len += (size_t)(1 + (expect > 1));
  }
  if (seen['h'] != 1) /* -h is required and cannot have suboptions */
    return IDLC_BAD_INPUT;
  if (!(str = idl_calloc(len + 1, sizeof(*str))))
    return IDLC_NO_MEMORY;
  memset(seen, 0, sizeof(seen));
  for (size_t i=0; options[i]; i++) {
    opt = (unsigned char)options[i]->option;
    if (seen[opt])
      continue;
    str[pos++] = options[i]->option;
    if (!isempty(options[i]->suboption))
      str[pos++] = ':';
    else if (options[i]->type && options[i]->argument)
      str[pos++] = ':';
    seen[opt] = 1;
  }
  str[pos] = '\0';
  *optstrp = str;
  return 0;
}

int parse_options(
  int argc, char **argv, idlc_option_t **options)
{
  int ret = IDLC_NO_MEMORY;
  char *optstr = NULL;
  idlc_option_t **opts = NULL;

  if ((ret = make_optstring(options, &optstr)))
    goto err_optstr;
  /* order is important if the same flag is used for options with and
     without suboptions. e.g. "-o" and "-o subopt". if the option with no
     suboption is encountered first, options with suboptions are never
     triggered */
  if (!(opts = sort_options(options, &descending)))
    goto err_sort;
  ret = handle_options(argc, argv, optstr, opts);
  idl_free(opts);
err_sort:
  idl_free(optstr);
err_optstr:
  return ret;
}
