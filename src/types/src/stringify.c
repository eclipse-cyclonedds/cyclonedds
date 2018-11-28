/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdio.h>
#include <stdbool.h>

#include "stringify.h"
#include "typetree.h"
#include "type_walker.h"

extern void dds_ts_stringify(dds_ts_node_t *context, char *buffer, size_t len)
{
  dds_ts_walker_t *walker = dds_ts_create_walker(context);

  dds_ts_walker_def_proc(walker, "module");

    dds_ts_walker_for_all_modules(walker);
      dds_ts_walker_emit(walker, "module ");
      dds_ts_walker_emit_name(walker);
      dds_ts_walker_emit(walker, "{");
      dds_ts_walker_call_proc(walker, "module");
      dds_ts_walker_emit(walker, "}");
    dds_ts_walker_end_for(walker);

    dds_ts_walker_for_all_structs(walker);
      dds_ts_walker_emit(walker, "struct ");
      dds_ts_walker_emit_name(walker);
      dds_ts_walker_emit(walker, "{");
      dds_ts_walker_for_all_members(walker);
        dds_ts_walker_emit_type(walker);
        dds_ts_walker_emit(walker, " ");
        dds_ts_walker_for_all_declarators(walker);
	  dds_ts_walker_emit_name(walker);
	  dds_ts_walker_emit(walker, ",");
        dds_ts_walker_end_for(walker);
        dds_ts_walker_emit(walker, ";");
      dds_ts_walker_end_for(walker);
      dds_ts_walker_emit(walker, "}");
    dds_ts_walker_end_for(walker);

  dds_ts_walker_end_def(walker);

  dds_ts_walker_main(walker);
    dds_ts_walker_call_proc(walker, "module");
  dds_ts_walker_end(walker);

  dds_ts_walker_execute(walker, buffer, len);

  dds_ts_walker_free(walker);
}
