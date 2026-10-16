// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

/* Exercise the buffer and diagnostic helpers without manufacturing a complete
   preprocessing run for their internal marker sequences. */
#include "../src/support.c"

#include "CUnit/Test.h"

static struct {
  MEMBUF buffers[MCPP_NUM_OUTDEST];
  int use_buffers;
  int mode, is_standard, debug, directive, token, error_count, warnings;
  int mb_check, mb_backslash, mb_escape;
  short *character_types;
  size_t (*read_mb)(int, char **, char **);
  struct option_flags_ options;
  struct std_limits_ limits;
  FILEINFO *file;
  char *macro;
  FILE *out, *err;
  int (*putc_func)(int, MCPP_OUTDEST);
  int (*puts_func)(const char *, MCPP_OUTDEST);
  int (*printf_func)(MCPP_OUTDEST, const char *, ...);
  jmp_buf exit;
} saved;

static void support_init(void)
{
  memcpy(saved.buffers, mem_buffers, sizeof(mem_buffers));
  saved.use_buffers = use_mem_buffers;
  saved.mode = mcpp_mode;
  saved.is_standard = standard;
  saved.debug = mcpp_debug;
  saved.directive = in_directive;
  saved.token = in_token;
  saved.error_count = errors;
  saved.warnings = warn_level;
  saved.mb_check = mbchk;
  saved.mb_backslash = bsl_in_mbchar;
  saved.mb_escape = bsl_need_escape;
  saved.character_types = char_type;
  saved.read_mb = mb_read;
  saved.options = option_flags;
  saved.limits = std_limits;
  saved.file = infile;
  saved.macro = macro_name;
  saved.out = fp_out;
  saved.err = fp_err;
  saved.putc_func = mcpp_fputc;
  saved.puts_func = mcpp_fputs;
  saved.printf_func = mcpp_fprintf;
  memcpy(saved.exit, error_exit, sizeof(error_exit));

  memset(mem_buffers, 0, sizeof(mem_buffers));
  use_mem_buffers = TRUE;
  mcpp_reset_def_out_func();
  mcpp_mode = STD;
  standard = TRUE;
  mb_init();
  mcpp_debug = in_directive = in_token = errors = warn_level = 0;
  memset(&option_flags, 0, sizeof(option_flags));
  option_flags.no_source_line = TRUE;
  infile = NULL;
  macro_name = NULL;
  fp_out = stdout;
  fp_err = stderr;
}

static void support_fini(void)
{
  for (int i = 0; i < MCPP_NUM_OUTDEST; i++)
    free(mem_buffers[i].buffer);
  memcpy(mem_buffers, saved.buffers, sizeof(mem_buffers));
  use_mem_buffers = saved.use_buffers;
  mcpp_mode = saved.mode;
  standard = saved.is_standard;
  mcpp_debug = saved.debug;
  in_directive = saved.directive;
  in_token = saved.token;
  errors = saved.error_count;
  warn_level = saved.warnings;
  mbchk = saved.mb_check;
  bsl_in_mbchar = saved.mb_backslash;
  bsl_need_escape = saved.mb_escape;
  char_type = saved.character_types;
  mb_read = saved.read_mb;
  option_flags = saved.options;
  std_limits = saved.limits;
  infile = saved.file;
  macro_name = saved.macro;
  fp_out = saved.out;
  fp_err = saved.err;
  mcpp_set_out_func(saved.putc_func, saved.puts_func, saved.printf_func);
  memcpy(error_exit, saved.exit, sizeof(error_exit));
}

static void clear_diagnostic(void)
{
  MEMBUF *buf = &mem_buffers[MCPP_ERR];
  if (buf->buffer) {
    buf->entry_pt = buf->buffer;
    buf->bytes_avail = buf->size;
    buf->buffer[0] = '\0';
  }
}

CU_Test(idlpp_support, append_terminator)
{
  static const size_t lengths[][2] = {
    { 0, 0 },
    { BUF_INCR_SIZE - 1, 1 },
    { BUF_INCR_SIZE, 1 },
    { BUF_INCR_SIZE + 1, BUF_INCR_SIZE + 1 }
  };
  char *text = malloc(BUF_INCR_SIZE + 2);
  CU_ASSERT_NEQ_FATAL(text, NULL);
  memset(text, 'x', BUF_INCR_SIZE + 1);
  text[BUF_INCR_SIZE + 1] = '\0';

  for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
    MEMBUF buf = { 0 };
    size_t used = 0;
    for (size_t j = 0; j < 2; j++) {
      append_to_buffer(&buf, text, lengths[i][j]);
      used += lengths[i][j];
      CU_ASSERT_GT(buf.size, used);
      CU_ASSERT_EQ(buf.bytes_avail, buf.size - used);
      CU_ASSERT_EQ(buf.entry_pt, buf.buffer + used);
      CU_ASSERT_EQ(buf.buffer[used], '\0');
      CU_ASSERT_EQ(strspn(buf.buffer, "x"), used);
    }
    free(buf.buffer);
  }
  free(text);
}

CU_Test(idlpp_support, append_size_overflow, .init = support_init, .fini = support_fini)
{
  if (setjmp(error_exit) == 0) {
    MEMBUF buf = { 0 };
    append_to_buffer(&buf, "", (size_t) -1);
    CU_FAIL_FATAL("an unrepresentable length must fail before copying");
  }
  CU_ASSERT_STREQ(mcpp_get_mem_buffer(MCPP_ERR), "Output buffer too large\n");
  clear_diagnostic();

  if (setjmp(error_exit) == 0) {
    char byte;
    MEMBUF buf = { &byte, &byte, (size_t) -1 - BUF_INCR_SIZE + 1, 0 };
    append_to_buffer(&buf, "x", 1);
    CU_FAIL_FATAL("an unrepresentable capacity must fail before reallocating");
  }
  CU_ASSERT_STREQ(mcpp_get_mem_buffer(MCPP_ERR), "Output buffer too large\n");
}

CU_Test(idlpp_support, quote_error, .init = support_init, .fini = support_fini)
{
  static const int delimiters[] = { '"', '\'', '<' };
  char output[128];
  /* A small limit also checks that error paths do not diagnose quote length. */
  std_limits.str_len = 1;
  warn_level = 4;
  for (size_t i = 0; i < sizeof(delimiters) / sizeof(delimiters[0]); i++) {
    char text[] = "unfinished\n";
    FILEINFO file = { 0 };
    file.buffer = file.bptr = text;
    infile = &file;
    CU_ASSERT_EQ(scan_quote(delimiters[i], output, output + sizeof(output) - 1, TRUE), NULL);
    CU_ASSERT_EQ(in_token, FALSE);
    CU_ASSERT_NEQ_FATAL(mcpp_get_mem_buffer(MCPP_ERR), NULL);
    CU_ASSERT_NEQ(strstr(mcpp_get_mem_buffer(MCPP_ERR), "Unterminated"), NULL);
    CU_ASSERT_EQ(strstr(mcpp_get_mem_buffer(MCPP_ERR), "Quotation longer"), NULL);
    clear_diagnostic();
  }

  char text[] = "valid\"";
  FILEINFO file = { 0 };
  file.buffer = file.bptr = text;
  infile = &file;
  CU_ASSERT_EQ(scan_quote('"', output, output + sizeof(output) - 1, TRUE), output + 7);
  CU_ASSERT_STREQ(output, "\"valid\"");
  CU_ASSERT_NEQ(strstr(mcpp_get_mem_buffer(MCPP_ERR), "Quotation longer"), NULL);
}

CU_Test(idlpp_support, diagnostic_newline, .init = support_init, .fini = support_fini)
{
  static const char marker[] = { IN_SRC, 1, '\n', EOS };
  static const char text_marker[] = { 'x', IN_SRC, 1, '\n', EOS };
  static const char newline_marker[] = { 'x', '\n', IN_SRC, 1, '\n', EOS };
  static const char marker_newline[] = { IN_SRC, 1, '\n', '\n', EOS };
  static const struct {
    const char *input;
    const char *output;
  } cases[] = {
    { NULL, "\n" },
    { "", "\n" },
    { "\n", "\n" },
    { "x\n", "x\n" },
    { "x\ny", "x y\n" },
    { "x ", "x \n" },
    { marker, "\n" },
    { text_marker, "x\n" },
    { newline_marker, "x \n" },
    { marker_newline, "\n" }
  };
  mcpp_debug = MACRO_CALL;
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    do_msg("error", "%s", cases[i].input, 0L, NULL);
    CU_ASSERT_STREQ(mcpp_get_mem_buffer(MCPP_ERR), cases[i].output);
    clear_diagnostic();
  }
}
