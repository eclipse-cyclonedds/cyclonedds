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
#include <inttypes.h>
#include <errno.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/io.h"

#include "jparser.h"
#include "parameters.h"

enum rule_id {
  RULE_ID_END,
  RULE_ID_COMMENT,
  RULE_ID_GLOBALS,
  RULE_ID_TEMPLATES,
  RULE_ID_NODES,
  RULE_ID_NODE,
  RULE_ID_FLOWS,
  RULE_ID_FLOW,
  RULE_ID_USES,
  RULE_ID_MODE,
  RULE_ID_TOPIC_NAME,
  RULE_ID_TOPIC_TYPE,
  RULE_ID_PARTITION,
  RULE_ID_NUM_KEYS,
  RULE_ID_SAMPLE_SIZE,
  RULE_ID_BURST_SIZE,
  RULE_ID_REGISTER,
  RULE_ID_RELIABLE,
  RULE_ID_HISTORY,
  RULE_ID_TRANSPORT_PRIO,
  RULE_ID_PUB_RATE,
  RULE_ID_USE_WRITER_LOAN,
  RULE_ID_SUBMODE,
  RULE_ID_SUBLATENCY,
  RULE_ID_COLLECT_STATS,
  RULE_ID_EXTENDED_STATS,
  RULE_ID_INTERFACE,
  RULE_ID_BANDWIDTH
};

struct node {
  ddsrt_avl_node_t node;
  char *name;
  char netload_if[256];
  double netload_bw;
  struct dataflow *head;
  struct dataflow *tail;
};

struct flow_template {
  ddsrt_avl_node_t node;
  char *name;
  struct dataflow *parameters;
};

struct context {
  ddsrt_avl_tree_t nodes;
  ddsrt_avl_tree_t templates;
  bool process_templates;
  struct node *curnode;
  struct flow_template *curtemplate;
  struct dataflow *curflow;
  struct global *globals;
};

static bool handle_global_parameter(struct jparser *parser, enum jparser_action_kind kind, const char *value, int label, void *args);
static bool handle_template (struct jparser *parser, enum jparser_action_kind kind, const char *value, int label, void *args);
static bool handle_node (struct jparser *parser, enum jparser_action_kind kind, const char *value, int label, void *args);
static bool handle_flow (struct jparser *parser, enum jparser_action_kind kind, const char *value, int label, void *args);
static bool handle_node_name (struct jparser *parser, enum jparser_action_kind kind, const char *name, int label, void *args);
static bool handle_node_parameter(struct jparser *parser, enum jparser_action_kind kind, const char *value, int label, void *args);
static bool handle_template_name(struct jparser *parser, enum jparser_action_kind kind, const char *name, int label, void *args);
static bool handle_flow_name (struct jparser *parser, enum jparser_action_kind kind, const char *name, int label, void *args);
static bool handle_flow_uses(struct jparser *parser, enum jparser_action_kind kind, const char *name, int label, void *args);
static bool handle_flow_parameter(struct jparser *parser, enum jparser_action_kind kind, const char *value, int label, void *args);

static const struct jparser_rule flow_rules[] = {
    { RULE_ID_FLOW,            "flow",            0, 0, handle_flow_name,      NULL },
    { RULE_ID_USES,            "uses",            0, 0, handle_flow_uses,      NULL },
    { RULE_ID_MODE,            "mode",            0, 0, handle_flow_parameter, NULL },
    { RULE_ID_TOPIC_NAME,      "topic_name",      0, 0, handle_flow_parameter, NULL },
    { RULE_ID_TOPIC_TYPE,      "topic_type",      0, 0, handle_flow_parameter, NULL },
    { RULE_ID_PARTITION,       "partition",       0, 0, handle_flow_parameter, NULL },
    { RULE_ID_NUM_KEYS,        "nkeys",           0, 0, handle_flow_parameter, NULL },
    { RULE_ID_SAMPLE_SIZE,     "size",            0, 0, handle_flow_parameter, NULL },
    { RULE_ID_BURST_SIZE,      "burst",           0, 0, handle_flow_parameter, NULL },
    { RULE_ID_REGISTER,        "register",        0, 0, handle_flow_parameter, NULL },
    { RULE_ID_RELIABLE,        "reliable",        0, 0, handle_flow_parameter, NULL },
    { RULE_ID_HISTORY,         "history",         0, 0, handle_flow_parameter, NULL },
    { RULE_ID_TRANSPORT_PRIO,  "transport_prio",  0, 0, handle_flow_parameter, NULL },
    { RULE_ID_PUB_RATE,        "pub_rate",        0, 0, handle_flow_parameter, NULL },
    { RULE_ID_USE_WRITER_LOAN, "use_writer_loan", 0, 0, handle_flow_parameter, NULL },
    { RULE_ID_COMMENT,         "comment",         0, 0, 0, NULL },
    { RULE_ID_END,             NULL,              0, 0, 0, NULL }
};

static const struct jparser_rule template_rules[] = {
    { RULE_ID_FLOW,            "name",            0, 0, handle_template_name,  NULL },
    { RULE_ID_MODE,            "mode",            0, 0, handle_flow_parameter, NULL },
    { RULE_ID_TOPIC_NAME,      "topic_name",      0, 0, handle_flow_parameter, NULL },
    { RULE_ID_TOPIC_TYPE,      "topic_type",      0, 0, handle_flow_parameter, NULL },
    { RULE_ID_PARTITION,       "partition",       0, 0, handle_flow_parameter, NULL },
    { RULE_ID_NUM_KEYS,        "nkeys",           0, 0, handle_flow_parameter, NULL },
    { RULE_ID_SAMPLE_SIZE,     "size",            0, 0, handle_flow_parameter, NULL },
    { RULE_ID_BURST_SIZE,      "burst",           0, 0, handle_flow_parameter, NULL },
    { RULE_ID_REGISTER,        "register",        0, 0, handle_flow_parameter, NULL },
    { RULE_ID_RELIABLE,        "reliable",        0, 0, handle_flow_parameter, NULL },
    { RULE_ID_HISTORY,         "history",         0, 0, handle_flow_parameter, NULL },
    { RULE_ID_TRANSPORT_PRIO,  "transport_prio",  0, 0, handle_flow_parameter, NULL },
    { RULE_ID_PUB_RATE,        "pub_rate",        0, 0, handle_flow_parameter, NULL },
    { RULE_ID_USE_WRITER_LOAN, "use_writer_loan", 0, 0, handle_flow_parameter, NULL },
    { RULE_ID_COMMENT,         "comment",         0, 0, 0, NULL },
    { RULE_ID_END,             NULL,              0, 0, 0, NULL }
};

static const struct jparser_rule node_rules[] = {
    { RULE_ID_NODE,       "node",      0,           0,           handle_node_name,      NULL},
    { RULE_ID_FLOWS,      "flows",     handle_flow, handle_flow, 0,                     flow_rules},
    { RULE_ID_INTERFACE,  "interface", 0,           0,           handle_node_parameter, NULL},
    { RULE_ID_BANDWIDTH,  "bandwidth", 0,           0,           handle_node_parameter, NULL},
    { RULE_ID_COMMENT,    "comment",   0,           0,           0,                     NULL },
    { RULE_ID_END,        NULL,        0,           0,           0,                     NULL }
};

static const struct jparser_rule global_rules[] = {
    { RULE_ID_SUBMODE,        "submode",        0, 0, handle_global_parameter, NULL},
    { RULE_ID_SUBLATENCY,     "sublatency",     0, 0, handle_global_parameter, NULL},
    { RULE_ID_COLLECT_STATS,  "collect_stats",  0, 0, handle_global_parameter, NULL},
    { RULE_ID_EXTENDED_STATS, "extended_stats", 0, 0, handle_global_parameter, NULL},
    { RULE_ID_COMMENT,        "comment",        0, 0, 0,                       NULL },
    { RULE_ID_END,            NULL,             0, 0, 0,                       NULL }
};

static const struct jparser_rule rules[] = {
    { RULE_ID_GLOBALS,   "globals",   0,               0,               0, global_rules},
    { RULE_ID_TEMPLATES, "templates", handle_template, handle_template, 0, template_rules},
    { RULE_ID_NODES,     "nodes",     handle_node,     handle_node,     0, node_rules},
    { RULE_ID_COMMENT,   "comment",   0,               0,               0, NULL },
    { RULE_ID_END,        NULL,       0,               0,               0, NULL }
};


static int compare_name (const void *a, const void *b);

static const ddsrt_avl_treedef_t node_tree_def = DDSRT_AVL_TREEDEF_INITIALIZER_INDKEY (offsetof (struct node, node), offsetof (struct node, name), compare_name, 0);
static const ddsrt_avl_treedef_t flow_tree_def = DDSRT_AVL_TREEDEF_INITIALIZER_INDKEY (offsetof (struct flow_template, node), offsetof (struct flow_template, name), compare_name, 0);


static int compare_name (const void *a, const void *b)
{
  return strcmp (a, b);
}

static const struct multiplier frequency_units[] = {
  { "Hz", 1 },
  { "kHz", 1000 },
  { NULL, 0 }
};

static const struct multiplier size_units[] = {
  { "B", 1 },
  { "k", 1024 },
  { "M", 1048576 },
  { "kB", 1024 },
  { "KiB", 1024 },
  { "MB", 1048576 },
  { "MiB", 1048576 },
  { NULL, 0 }
};

static int lookup_multiplier (const struct multiplier *units, const char *suffix)
{
  while (*suffix == ' ')
    suffix++;
  if (*suffix == 0)
    return 1;
  else if (units == NULL)
    return 0;
  else
  {
    for (size_t i = 0; units[i].suffix; i++)
      if (strcmp (units[i].suffix, suffix) == 0)
        return units[i].mult;
    return 0;
  }
}

bool string_to_size (const char *str, uint32_t *val)
{
  unsigned x;
  int pos, mult;

  if (sscanf (str, "%u%n", &x, &pos) == 1 && (mult = lookup_multiplier (size_units, str + pos)) > 0)
    *val = x * (unsigned) mult;
  else
    return false;
  return true;
}

bool string_to_number (const char *str, uint32_t *val)
{
  char *endptr;
  int orig_errno = errno;

  errno = 0;
  *val = (uint32_t) strtoul (str, &endptr, 10);
  errno = orig_errno;
  if (*endptr != '\0')
    return false;
  return true;
}

bool string_to_frequency (const char *str, double *val)
{
  int pos, mult = 1;
  double r;

  if (strncmp (str, "inf", 3) == 0 && lookup_multiplier (frequency_units, str + 3) > 0)
    *val = HUGE_VAL;
  else if (sscanf (str, "%lf%n", &r, &pos) == 1 && (mult = lookup_multiplier (frequency_units, str + pos)) > 0)
    *val = r * mult;
  else
    return false;
  return true;
}

bool string_to_bool (const char *value, bool *result)
{
  if (strcmp (value, "true") == 0 || strcmp (value, "1") == 0)
    *result = true;
  else if (strcmp (value, "false") == 0 || strcmp (value, "0") == 0)
    *result = false;
  else
    return false;
  return true;
}

static bool flow_exists (struct node *node, const char *name)
{
  struct dataflow *flow;

  flow = node->head;
  while (flow != NULL && strcmp (flow->name, name) != 0)
    flow = flow->next;

  return (flow != NULL);
}

char * string_dup(const char *s)
{
  char *str;

  if ((str = malloc(strlen(s) + 1)) != NULL)
    strcpy(str, s);
  return str;
}

static bool handle_global_parameter(struct jparser *parser, enum jparser_action_kind kind, const char *value, int label, void *args)
{
  struct context *ctx = args;

  DDSRT_UNUSED_ARG(kind);

  switch (label)
  {
  case RULE_ID_SUBMODE:
    if (strcmp ("waitset", value) == 0)
      ctx->globals->submode = SM_WAITSET;
    else if (strcmp ("polling", value) == 0)
      ctx->globals->submode = SM_POLLING;
    else if (strcmp ("listener", value) == 0)
      ctx->globals->submode = SM_LISTENER;
    else
    {
      jparser_error (parser, "submode '%s' is invalid", value);
      return false;
    }
    return true;;
  case RULE_ID_SUBLATENCY:
    if (!string_to_bool (value, &ctx->globals->sublatency))
    {
      jparser_error (parser, "sublatency '%s' is not a valid boolean", value);
      return false;
    }
    return true;
  case RULE_ID_COLLECT_STATS:
    if (!string_to_bool (value, &ctx->globals->collect_stats))
    {
      jparser_error (parser, "collect_stats '%s' is not a valid boolean", value);
      return false;
    }
    return true;
  case RULE_ID_EXTENDED_STATS:
    if (!string_to_bool (value, &ctx->globals->extended_stats))
    {
      jparser_error (parser, "extended_stats '%s' is not a valid boolean", value);
      return false;
    }
    return true;
  default:
    assert(0);
    break;
  }
  return false;
}

static bool handle_node (struct jparser *parser, enum jparser_action_kind kind, const char *value, int label, void *args)
{
  struct context *ctx = args;

  DDSRT_UNUSED_ARG(value);
  DDSRT_UNUSED_ARG(label);

  if (kind == JPARSER_ACTION_ARRAY_OPEN)
  {
      ctx->curnode = malloc (sizeof (struct node));
      ctx->curnode->name = NULL;
      ctx->curnode->netload_if[0] = '\0';
      ctx->curnode->netload_bw = -1;
      ctx->curnode->head = NULL;
      ctx->curnode->tail = NULL;
  }
  else if (kind == JPARSER_ACTION_ARRAY_CLOSE)
  {
    if (ctx->curnode == NULL || ctx->curnode->name == NULL)
    {
      jparser_error (parser, "node name not specified");
      return false;
    }
    ddsrt_avl_insert(&node_tree_def, &ctx->nodes, ctx->curnode);
    ctx->curnode = NULL;
  }

  return true;
}

static bool handle_template (struct jparser *parser, enum jparser_action_kind kind, const char *value, int label, void *args)
{
  struct context *ctx = args;

  DDSRT_UNUSED_ARG(value);
  DDSRT_UNUSED_ARG(label);

  if (kind == JPARSER_ACTION_OBJECT_OPEN)
  {
    ctx->process_templates = true;
  }
  else if (kind == JPARSER_ACTION_OBJECT_CLOSE)
  {
    ctx->process_templates = false;
  }
  else if (kind == JPARSER_ACTION_ARRAY_OPEN)
  {
      ctx->curtemplate = malloc (sizeof (struct flow_template));
      ctx->curtemplate->name = NULL;
      ctx->curtemplate->parameters = dataflow_new();
  }
  else if (kind == JPARSER_ACTION_ARRAY_CLOSE)
  {
    if (ctx->curtemplate == NULL || ctx->curtemplate->name == NULL)
    {
      jparser_error (parser, "template name not specified");
      return false;
    }
    ddsrt_avl_insert(&flow_tree_def, &ctx->templates, ctx->curtemplate);
    ctx->curtemplate = NULL;
  }

  return true;
}

static bool handle_flow (struct jparser *parser, enum jparser_action_kind kind, const char *value, int label, void *args)
{
  struct context *ctx = args;

  DDSRT_UNUSED_ARG(value);
  DDSRT_UNUSED_ARG(label);

  if (kind == JPARSER_ACTION_ARRAY_OPEN)
  {
      ctx->curflow = dataflow_new ();
  }
  else if (kind == JPARSER_ACTION_ARRAY_CLOSE)
  {
    if (ctx->curflow == NULL || ctx->curflow->name == NULL)
    {
      jparser_error (parser, "flow name not specified");
      return false;
    }

    if (ctx->curflow->nkeyvals == 0)
        ctx->curflow->nkeyvals = 1;

    if (ctx->curflow->topicsel == OU && ctx->curflow->nkeyvals != 1)
    {
      jparser_error (parser, "-n %u invalid: topic OU has no key\n", ctx->curflow->nkeyvals);
      return false;
    }

    if (ctx->curflow->topicsel != KS && ctx->curflow->baggagesize != 0)
    {
      jparser_error (parser, "size %"PRIu32" invalid: only topic KS has a sequence", ctx->curflow->baggagesize);
      return false;
    }
    if (ctx->curflow->topicsel == KS && ctx->curflow->use_writer_loan)
    {
      jparser_error (parser,"topic KS is not supported with writer loans because it contains a sequence\n");
      return false;
    }

    if (ctx->curflow->baggagesize != 0 && ctx->curflow->baggagesize < 12)
    {
      jparser_error (parser, "size %"PRIu32" invalid: too small to allow for overhead\n", ctx->curflow->baggagesize);
      return false;
    }

    if (ctx->curflow->baggagesize > 0)
      ctx->curflow->baggagesize -= 12;

    ctx->curflow->tp_desc = get_topic_descriptor (ctx->curflow->topicsel);
    if (ctx->curnode->head == NULL)
    {
      ctx->curnode->head = ctx->curflow;
    }
    else
    {
      ctx->curnode->tail->next = ctx->curflow;
    }
    ctx->curnode->tail = ctx->curflow;
    ctx->curflow = NULL;
  }

  return true;
}

static bool handle_node_name(struct jparser *parser, enum jparser_action_kind kind, const char *name, int label, void *args)
{
  struct context *ctx = args;

  DDSRT_UNUSED_ARG(kind);
  DDSRT_UNUSED_ARG(label);

  assert(ctx->curnode);

  if (ctx->curnode->name != NULL)
  {
    jparser_error (parser, "node '%s' already specified", name);
    return false;
  }
  else if (ddsrt_avl_lookup (&node_tree_def, &ctx->nodes, name) != NULL)
  {
    jparser_error (parser, "node '%s' already exists", name);
    return false;
  }
  ctx->curnode->name = string_dup(name);
  return true;
}

static bool handle_template_name(struct jparser *parser, enum jparser_action_kind kind, const char *name, int label, void *args)
{
  struct context *ctx = args;

  DDSRT_UNUSED_ARG(kind);
  DDSRT_UNUSED_ARG(label);

  assert(ctx->curtemplate);

  if (ctx->curtemplate->name != NULL)
  {
    jparser_error (parser, "template '%s' already specified", name);
    return false;
  }
  else if (ddsrt_avl_lookup (&flow_tree_def, &ctx->templates, name) != NULL)
  {
    jparser_error (parser, "template '%s' already exists", name);
    return false;
  }
  ctx->curtemplate->name = string_dup(name);
  return true;
}

static bool handle_flow_name(struct jparser *parser, enum jparser_action_kind kind, const char *name, int label, void *args)
{
  struct context *ctx = args;

  DDSRT_UNUSED_ARG(kind);
  DDSRT_UNUSED_ARG(label);

  assert(ctx->curflow);

  if (ctx->curflow->name != NULL)
  {
    jparser_error (parser, "flow '%s' already specified", name);
    return false;
  }
  else if (flow_exists (ctx->curnode, name))
  {
    jparser_error (parser, "FAIL: flow '%s' already exists\n", name);
    return false;
  }
  ctx->curflow->name = string_dup(name);
  return true;
}

static bool handle_flow_uses(struct jparser *parser, enum jparser_action_kind kind, const char *name, int label, void *args)
{
  struct context *ctx = args;
  struct flow_template *template = NULL;

  DDSRT_UNUSED_ARG(kind);
  DDSRT_UNUSED_ARG(label);

  assert(ctx->curflow);

  template = ddsrt_avl_lookup (&flow_tree_def, &ctx->templates, name);
  if (template == NULL)
  {
    jparser_error (parser, "flow template '%s' not found", name);
    return false;
  }

  ctx->curflow->mode = template->parameters->mode;
  if (template->parameters->topicname)
  {
    free(ctx->curflow->topicname);
    ctx->curflow->topicname = string_dup(template->parameters->topicname);
  }
  if (template->parameters->partition)
  {
    free(ctx->curflow->partition);
    ctx->curflow->partition = string_dup(template->parameters->partition);
  }
  ctx->curflow->topicsel = template->parameters->topicsel;
  ctx->curflow->tp_desc = template->parameters->tp_desc;
  ctx->curflow->nkeyvals = template->parameters->nkeyvals;
  ctx->curflow->baggagesize = template->parameters->baggagesize;
  ctx->curflow->burstsize = template->parameters->burstsize;
  ctx->curflow->register_instances = template->parameters->register_instances;
  ctx->curflow->pub_rate = template->parameters->pub_rate;
  ctx->curflow->reliable = template->parameters->reliable;
  ctx->curflow->histdepth = template->parameters->histdepth;
  ctx->curflow->transport_prio = template->parameters->transport_prio;
  ctx->curflow->ping_frac = template->parameters->ping_frac;
  ctx->curflow->use_writer_loan = template->parameters->use_writer_loan;

  return true;
}

static bool handle_node_parameter(struct jparser *parser, enum jparser_action_kind kind, const char *value, int label, void *args)
{
  struct context *ctx = args;
  bool result = true;
  int pos;

  DDSRT_UNUSED_ARG(kind);

  switch (label)
  {
  case RULE_ID_INTERFACE:
    ddsrt_strlcpy (ctx->curnode->netload_if, value, sizeof (ctx->globals->netload_if));
    break;
  case RULE_ID_BANDWIDTH:
    if (sscanf (value, "%lf%n", &ctx->curnode->netload_bw, &pos) != 1)
    {
      jparser_error (parser, "bandwidth '%s' is valid", value);
      result = false;
    }
    break;
  }
  return result;
}

static bool handle_flow_parameter(struct jparser *parser, enum jparser_action_kind kind, const char *value, int label, void *args)
{
  struct context *ctx = args;
  struct dataflow *flow;
  bool result = true;
  uint32_t uval;

  DDSRT_UNUSED_ARG(kind);

  if (ctx->process_templates)
  {
    flow = ctx->curtemplate->parameters;
  }
  else
  {
    flow = ctx->curflow;
  }

  switch (label)
  {
  case RULE_ID_MODE:
    if (strcmp (value, "pub") == 0)
      flow->mode = FLOW_PUB;
    else if (strcmp (value, "sub") == 0)
      flow->mode = FLOW_SUB;
    else
    {
      jparser_error (parser, "mode '%s' not expected only 'pub' or 'sub' allowed", value);
      result = false;
    }
    break;
  case RULE_ID_TOPIC_NAME:
    free(flow->topicname);
    flow->topicname = string_dup (value);
    break;
  case RULE_ID_TOPIC_TYPE:
    result = get_topicsel_from_string (value, &flow->topicsel);
    break;
  case RULE_ID_PARTITION:
    free(flow->partition);
    flow->partition = string_dup (value);
    break;
  case RULE_ID_NUM_KEYS:
    if (!string_to_number (value, &flow->nkeyvals))
    {
      jparser_error (parser, "nkeys '%s' is not a valid number", value);
      result = false;
    }
    break;
  case RULE_ID_SAMPLE_SIZE:
    if (!string_to_size (value, &flow->baggagesize))
    {
      jparser_error (parser, "sample_size '%s' is not a valid number", value);
      result = false;
    }
    break;
  case RULE_ID_BURST_SIZE:
    if (!string_to_size (value, &flow->burstsize))
    {
      jparser_error (parser, "burst_size '%s' is not a valid number", value);
      result = false;
    }
    break;
  case RULE_ID_REGISTER:
    if (!string_to_bool (value, &flow->register_instances))
    {
      jparser_error (parser, "register '%s' is not a valid booleam", value);
      result = false;
    }
    break;
  case RULE_ID_RELIABLE:
    if (!string_to_bool (value, &flow->reliable))
    {
      jparser_error (parser, "reliable '%s' is not a valid boolean", value);
      result = false;
    }
    break;
  case RULE_ID_HISTORY:
    if (!string_to_number (value, &uval))
    {
      jparser_error (parser, "history '%s' is not a valid number", value);
      result = false;
    }
    flow->histdepth = (int32_t) uval;
    break;
  case RULE_ID_TRANSPORT_PRIO:
    if (!string_to_number (value, &flow->transport_prio))
    {
      jparser_error (parser, "transport_prio '%s' is not a valid number", value);
      result = false;
    }
    break;
  case RULE_ID_PUB_RATE:
    if (!string_to_frequency (value, &flow->pub_rate))
    {
      jparser_error (parser, "pub_rate '%s' is not a valid number", value);
      result = false;
    }
    break;
  case RULE_ID_USE_WRITER_LOAN:
    if (!string_to_bool (value, &flow->use_writer_loan))
    {
      jparser_error (parser, "reliable '%s' is not a valid boolean", value);
      result = false;
    }
    break;
  default:
    result = false;
    break;
  }

  return result;
}


static void node_free (struct node *node)
{
  if (node)
  {
    free (node->name);
    while (node->head)
    {
      struct dataflow *flow = node->head;
      node->head = flow->next;
      dataflow_free (flow);
    }
    free (node);
  }
}

static void node_free_wrapper (void *a)
{
  node_free (a);
}

static void flow_template_free (struct flow_template *template)
{
  if (template)
  {
    free (template->name);
    dataflow_free(template->parameters);
    free (template);
  }
}

static void flow_template_free_wrapper (void *a)
{
  flow_template_free (a);
}

bool read_parameters (const char *fname, const char *node_name, struct dataflow **flows, struct global *globals)
{
  bool result = false;
  struct context ctx;

  ctx.curnode = NULL;
  ctx.curtemplate = NULL;
  ctx.curflow = NULL;
  ctx.globals = globals;
  ctx.process_templates = false;
  ddsrt_avl_init (&node_tree_def, &ctx.nodes);
  ddsrt_avl_init (&flow_tree_def, &ctx.templates);

  if ((result = jparser_parse(fname, rules, &ctx)))
  {
    struct node *node = ddsrt_avl_lookup (&node_tree_def, &ctx.nodes, node_name);
    if (node == NULL)
    {
      fprintf (stderr, "Node '%s' not found\n", node_name);
      result = false;
    }
    else
    {
      *flows = node->head;
      if (node->netload_bw > 0 && node->netload_if[0] != '\0')
      {
        memcpy(globals->netload_if, node->netload_if, sizeof(globals->netload_if));
        globals->netload_bw = node->netload_bw;
      }
      node->head = NULL;
      node->tail = NULL;
    }
  }

  flow_template_free(ctx.curtemplate);
  dataflow_free (ctx.curflow);
  node_free (ctx.curnode);
  ddsrt_avl_free (&node_tree_def, &ctx.nodes, node_free_wrapper);
  ddsrt_avl_free (&flow_tree_def, &ctx.templates, flow_template_free_wrapper);

  return result;
}

void init_globals (struct global *args)
{
  args->dur = HUGE_VAL;
  args->is_publishing = false;
  args->tref = DDS_INFINITY;
  args->netload_if[0] = '\0';
  args->netload_bw = -1;
  args->submode = SM_LISTENER;
  args->pingpongmode = SM_LISTENER;
  args->ping_intv = DDS_INFINITY;
  args->substat_every_second = false;
  args->extended_stats = false;
  args->minmatch = 0;
  args->initmaxwait = 0;
  args->maxwait = HUGE_VAL;
  args->rss_check = false;
  args->rss_factor = 1;
  args->rss_term = 0;
  args->min_received = 0;
  args->min_roundtrips = 0;
  args->livemem_check = false;
  args->livemem_factor = 1;
  args->livemem_term = 0;
  args->sublatency = false;
}

struct dataflow * dataflow_new (void)
{
  struct dataflow *flow = malloc (sizeof (*flow));
  flow->name = NULL;
  flow->mode = FLOW_DEFAULT;
  flow->topicname = NULL;
  flow->partition = NULL;
  flow->topicsel = KS;
  flow->tp_desc = NULL;
  flow->nkeyvals = 1;
  flow->baggagesize = 0;
  flow->burstsize = 1;
  flow->register_instances = true;
  flow->pub_rate = 0.0;
  flow->reliable = true;
  flow->histdepth = 0;
  flow->ping_frac = 0;
  flow->use_writer_loan = false;
  flow->next  = NULL;
  return flow;
}

void dataflow_free (struct dataflow *flow)
{
  if (flow)
  {
    free (flow->name);
    free (flow->topicname);
    free (flow->partition);
    free (flow);
  }
}

bool get_topicsel_from_string (const char *name, enum topicsel *topicsel)
{
  if (strcmp (name, "KS") == 0) *topicsel = KS;
  else if (strcmp (name, "K32") == 0) *topicsel = K32;
  else if (strcmp (name, "K256") == 0) *topicsel = K256;
  else if (strcmp (name, "OU") == 0) *topicsel = OU;
  else if (strcmp (name, "UK16") == 0) *topicsel = UK16;
  else if (strcmp (name, "UK1") == 0) *topicsel = UK1k;
  else if (strcmp (name, "UK64") == 0) *topicsel = UK64k;
  else if (strcmp (name, "S16") == 0) *topicsel = S16;
  else if (strcmp (name, "S256") == 0) *topicsel = S256;
  else if (strcmp (name, "S4k") == 0) *topicsel = S4k;
  else if (strcmp (name, "S32k") == 0) *topicsel = S32k;
  else return false;
  return true;
}

const dds_topic_descriptor_t * get_topic_descriptor (enum topicsel topicsel)
{
  const dds_topic_descriptor_t *tp_desc = NULL;
  switch (topicsel)
  {
  case KS:     tp_desc = &KeyedSeq_desc; break;
  case K32:    tp_desc = &Keyed32_desc;  break;
  case K256:   tp_desc = &Keyed256_desc; break;
  case OU:     tp_desc = &OneULong_desc; break;
  case UK16:   tp_desc = &Unkeyed16_desc; break;
  case UK1k:   tp_desc = &Unkeyed1k_desc; break;
  case UK64k:  tp_desc = &Unkeyed64k_desc; break;
  case S16:    tp_desc = &Struct16_desc; break;
  case S256:   tp_desc = &Struct256_desc; break;
  case S4k:    tp_desc = &Struct4k_desc; break;
  case S32k:   tp_desc = &Struct32k_desc; break;
  }
  return tp_desc;
}
