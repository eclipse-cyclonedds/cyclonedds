// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "../src/expand.c"

#include "CUnit/Test.h"
#include "mcpp_lib.h"

CU_Test(idlpp_expand, verbose_macro)
{
  char input[] = IDLPP_TEST_DATA_DIR "/verbose-macro.idl";
  for (int trace = 0; trace <= 1; trace++) {
    char *argv[6] = { "idlpp", "-P", "-v", NULL, NULL, NULL };
    int argc = 3;
    if (trace)
      argv[argc++] = "-K";
    argv[argc++] = input;
    mcpp_use_mem_buffers(TRUE);
    CU_ASSERT_EQ_FATAL(mcpp_lib_main(argc, argv), 0);
    const char *output = mcpp_get_mem_buffer(MCPP_OUT);
    CU_ASSERT_NEQ_FATAL(output, NULL);
    if (trace) {
      CU_ASSERT_NEQ(strstr(output, "/*<A "), NULL);
      CU_ASSERT_NEQ(strstr(output, "/*A>*/"), NULL);
    }
    /* Ignore tracing comments and whitespace, preserving the output tokens. */
    char tokens[16];
    size_t n = 0;
    for (const char *p = output; *p; ) {
      if (p[0] == '/' && p[1] == '*') {
        const char *end = strstr(p + 2, "*/");
        CU_ASSERT_NEQ_FATAL(end, NULL);
        p = end + 2;
      } else if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
      } else {
        CU_ASSERT_LT_FATAL(n, sizeof(tokens) - 1);
        tokens[n++] = *p++;
      }
    }
    tokens[n] = 0;
    CU_ASSERT_STREQ(tokens, "a/a");
    mcpp_use_mem_buffers(TRUE); /* Release output and reset the buffers. */
  }
}

CU_Test(idlpp_expand, trace_comment_prefix)
{
  static const struct {
    const char *prefix;
    const char *expected;
  } cases[] = {
    { "", " /*>*/" },
    { "/", "/ /*>*/" },
    { "*/", "*/" "/*>*/" },
    { "x/", "x/ /*>*/" },
    { "x*/", "x*/" "/*>*/" },
    { "x", "x/*>*/" }
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    char output[32];
    char next[] = "x";
    char *cp = next;
    strcpy(output, cases[i].prefix);
    char *op = output + strlen(output);
    option_flags.v = FALSE;
    CU_ASSERT_EQ(print_macro_inf(MAC_CALL_END, &cp, &op, output), 'x');
    CU_ASSERT_EQ(cp, &next[0]);
    CU_ASSERT_EQ(op, output + strlen(output));
    CU_ASSERT_STREQ(output, cases[i].expected);
  }
}

CU_Test(idlpp_expand, redundant_trace_closers)
{
  static const char macro_seq[] = {
    MAC_INF, MAC_CALL_END, 1, 1,
    MAC_INF, MAC_CALL_START, 1, 1,
    MAC_INF, MAC_CALL_END, 1, 1
  };
  static const char arg_seq[] = {
    MAC_INF, MAC_ARG_END, 1, 1, 1,
    MAC_INF, MAC_ARG_START, 1, 1, 1,
    MAC_INF, MAC_ARG_END, 1, 1, 1
  };
  const struct {
    const char *data;
    size_t size;
  } cases[] = {
    { macro_seq, sizeof(macro_seq) },
    { arg_seq, sizeof(arg_seq) }
  };
  option_flags.v = TRUE;
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    char *buf = malloc(cases[i].size);
    CU_ASSERT_NEQ_FATAL(buf, NULL);
    memcpy(buf, cases[i].data, cases[i].size);
    CU_ASSERT_EQ(chk_magic_balance(buf, buf + cases[i].size, TRUE, FALSE), buf);
    CU_ASSERT_EQ(memcmp(buf, cases[i].data, cases[i].size), 0);
    free(buf);
  }
}

CU_Test(idlpp_expand, trace_nesting_capacity)
{
  for (int verbose = 0; verbose <= 1; verbose++) {
    option_flags.v = verbose;
    for (int arg = 0; arg <= 1; arg++) {
      const char start[] = { MAC_INF, (char)(arg ? MAC_ARG_START : MAC_CALL_START), 1, 1, 1 };
      const char end[] = { MAC_INF, (char)(arg ? MAC_ARG_END : MAC_CALL_END), 1, 1, 1 };
      const size_t slen = arg ? ARG_S_LEN : MAC_S_LEN;
      const size_t elen = verbose ? slen : MAC_E_LEN;
      for (size_t depth = MAX_NEST_MAGICS - 1; depth <= MAX_NEST_MAGICS + 1; depth++) {
        const size_t size = depth * (slen + elen);
        char buf[(MAX_NEST_MAGICS + 1) * (ARG_S_LEN + ARG_E_LEN_V)] = { 0 };
        for (size_t i = 0; i < depth; i++) {
          memcpy(buf + i * slen, start, slen);
          memcpy(buf + depth * slen + i * elen, end, elen);
        }
        CU_ASSERT_EQ(chk_magic_balance(buf, buf + size, TRUE, FALSE), &buf[0]);
        for (size_t i = 0; i < depth; i++) {
          CU_ASSERT_EQ(memcmp(buf + i * slen, start, slen), 0);
          CU_ASSERT_EQ(memcmp(buf + depth * slen + i * elen, end, elen), 0);
        }
      }
    }
  }
}

CU_Test(idlpp_expand, truncated_trace_markers)
{
  static const struct {
    char bytes[ARG_S_LEN];
    size_t len;
  } cases[] = {
    { { MAC_INF, MAC_CALL_START, 1, 1 }, MAC_S_LEN },
    { { MAC_INF, MAC_CALL_END, 1, 1 }, MAC_E_LEN_V },
    { { MAC_INF, MAC_ARG_START, 1, 1, 1 }, ARG_S_LEN },
    { { MAC_INF, MAC_ARG_END, 1, 1, 1 }, ARG_E_LEN_V }
  };
  option_flags.v = TRUE;
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    for (size_t n = 0; n < cases[i].len; n++) {
      char *buf = malloc(n ? n : 1);
      CU_ASSERT_NEQ_FATAL(buf, NULL);
      memcpy(buf, cases[i].bytes, n);
      CU_ASSERT_EQ(chk_magic_balance(buf, buf + n, TRUE, FALSE), buf);
      CU_ASSERT_EQ(memcmp(buf, cases[i].bytes, n), 0);
      free(buf);
    }
  }
}

CU_Test(idlpp_expand, move_stray_argument_marker)
{
  static const char input[] = {
    MAC_INF, MAC_ARG_START, 1, 1, 1,
    MAC_INF, MAC_ARG_START, 1, 1, 2,
    MAC_INF, MAC_ARG_END, 1, 1, 2,
    MAC_INF, MAC_ARG_END, 1, 1, 3,
    MAC_INF, MAC_ARG_END, 1, 1, 1
  };
  static const char expected[] = {
    MAC_INF, MAC_ARG_START, 1, 1, 1,
    MAC_INF, MAC_ARG_START, 1, 1, 2,
    MAC_INF, MAC_ARG_END, 1, 1, 2,
    MAC_INF, MAC_ARG_END, 1, 1, 1,
    MAC_INF, MAC_ARG_END, 1, 1, 3
  };
  option_flags.v = TRUE;
  for (int move = 0; move <= 1; move++) {
    char buf[sizeof(input)];
    memcpy(buf, input, sizeof(buf));
    CU_ASSERT_EQ(chk_magic_balance(buf, buf + sizeof(buf), move, FALSE), &buf[0]);
    CU_ASSERT_EQ(memcmp(buf, move ? expected : input, sizeof(buf)), 0);
  }
}
