// Copyright(c) 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <assert.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/io.h"

#include "jparser.h"

struct jparser {
  FILE *f;
  char c;
  int line;
  int pos;
  void *args;
};

void jparser_error (struct jparser *parser, char *fmt, ...)
{
  char *msg;
  va_list ap;

  va_start (ap, fmt);
  (void)ddsrt_vasprintf (&msg, fmt, ap);
  va_end (ap);
  fprintf (stderr, "parser error at char '%c' line %u at %u: %s\n", parser->c, parser->line, parser->pos,  msg);
  ddsrt_free (msg);
}


static char readchar (struct jparser *parser)
{
  parser->c = (char)fgetc (parser->f);
  parser->pos++;
  if (parser->c == '\n')
  {
    parser->line++;
    parser->pos = 0;
  }
  return parser->c;
}

static char skip_whitespace (struct jparser *parser)
{
  while (parser->c != EOF && strchr (" \t\n\r", parser->c))
    (void)readchar (parser);
  return parser->c;
}

static char * read_string (struct jparser *parser)
{
  char buffer[256];
  char c;
  uint32_t i = 0;

  while ((c = readchar (parser)) != EOF && c != '"' && c != '\r' && c != '\n' && i < sizeof (buffer))
  {
    buffer[i++] = (char)c;
  }
  if (c != '"')
  {
    jparser_error (parser, "expected closing quote");
    return NULL;
  }
  buffer[i] = '\0';
  (void)readchar (parser);

  return ddsrt_strdup (buffer);
}

#define TOKEN_DELIMERS ",}] \r\n"

static char * read_token (struct jparser *parser)
{
  char buffer[256];
  char c = parser->c;
  uint32_t i = 0;

  do {
    buffer[i++] = c;
  } while ((c = readchar (parser)) != EOF && strchr (TOKEN_DELIMERS, c) == NULL && i < sizeof (buffer));
  buffer[i] = '\0';

  return ddsrt_strdup (buffer);
}

static bool parse_object (struct jparser *parser, const struct jparser_rule * const rules);
static bool parse_value (struct jparser *parser, const struct jparser_rule * const rules, const char *name);

static bool parse_array (struct jparser *parser, const struct jparser_rule * const rules, const char *name)
{
  char c;

  do {
    (void)readchar (parser);

    if (rules->open_callback && !rules->open_callback (parser, JPARSER_ACTION_ARRAY_OPEN, name, rules->label, parser->args))
    {
      return false;
    }
    else if (!parse_value (parser, rules, name))
    {
      return false;
    }
    else if (rules->close_callback && !rules->close_callback (parser, JPARSER_ACTION_ARRAY_CLOSE, name, rules->label, parser->args))
    {
      return false;
    }
    c = skip_whitespace (parser);
  } while (c == ',');

  if ((c != ']' && c != EOF))
  {
    jparser_error (parser, "expected ]");
    return false;
  }
  (void)readchar (parser);

  return true;
}

static bool parse_value (struct jparser *parser, const struct jparser_rule * const rules, const char *name)
{
  bool retval;
  char c;
  char *str;

  c = skip_whitespace (parser);
  if (c == '"')
  {
    if ((str = read_string (parser)) == NULL)
    {
      return false;
    }
    retval = !rules->value_callback || rules->value_callback (parser, JPARSER_ACTION_VALUE, str, rules->label, parser->args);
    ddsrt_free (str);
  }
  else if (c == '{')
  {
    return parse_object (parser, rules->children ? rules->children : rules);
  }
  else if (c == '[')
  {
    retval = parse_array(parser, rules, name);
  }
  else
  {
    if ((str = read_token (parser)) == NULL)
      return false;
    retval = !rules->value_callback || rules->value_callback (parser, JPARSER_ACTION_VALUE, str, rules->label, parser->args);
    ddsrt_free (str);
  }

  return retval;
}

static bool parse_element (struct jparser *parser, const struct jparser_rule * const rules)
{
  char c;
  char *name = NULL;
  int idx;

  c = skip_whitespace (parser);
  if (c != '"')
  {
    jparser_error (parser, "expected opening quote");
    return false;
  }

  if ((name = read_string (parser)) == NULL)
  {
    jparser_error (parser, "expect element name");
    return false;
  }
  else if (strlen (name) == 0)
  {
    jparser_error (parser, "expect element name");
    goto fail;
  }

  for (idx = 0; ((rules[idx].label != JPARSER_END_RULE) && (rules[idx].name != NULL) && (strcmp (rules[idx].name, name) != 0)); idx++)
  {
    if (rules[idx].label == JPARSER_END_RULE)
    {
      jparser_error (parser, "expect element name '%s' not found", name);
      goto fail;
    }
  }

  if (rules[idx].open_callback && !rules[idx].open_callback (parser, JPARSER_ACTION_OBJECT_OPEN, name, rules[idx].label, parser->args))
    goto fail;

  c = skip_whitespace (parser);
  if (c != ':')
  {
    jparser_error (parser, "expected colon");
    goto fail;
  }

  (void)readchar (parser);
  if (!parse_value (parser, &rules[idx], name))
    goto fail;

  if (rules[idx].close_callback && !rules[idx].close_callback (parser, JPARSER_ACTION_OBJECT_CLOSE, name, rules[idx].label, parser->args))
    goto fail;

  ddsrt_free (name);
  return true;

fail:
  ddsrt_free (name);
  return false;
}

static bool parse_object (struct jparser *parser, const struct jparser_rule * const rules)
{
  char c;
  const struct jparser_rule *r = rules;

  do {
    (void)readchar (parser);
    if (!parse_element (parser, r))
    {
      return false;
    }
    c = skip_whitespace (parser);
  } while (c == ',');

  if ((c != '}' && c != EOF))
  {
    jparser_error (parser, "expected }");
    return false;
  }
  (void)readchar (parser);

  return true;
}

bool jparser_parse (const char *fname, const struct jparser_rule * const rules, void *args)
{
  bool result = false;
  struct jparser parser;
  char c;

  parser.f = fopen (fname, "r");
  if (parser.f == NULL) {
    fprintf (stderr, "Failed to open script '%s'\n", fname);
    return false;
  }

  parser.line = 1;
  parser.pos = 0;
  parser.args = args;
  parser.c = 0;

  c = skip_whitespace (&parser);
  if (c == '{')
    result =  parse_object (&parser, rules);
  else if (c == '[')
    result = parse_array (&parser, rules, NULL);
  else
    jparser_error(&parser, "expected '{' or '['");

  fclose(parser.f);
  return result;
}
