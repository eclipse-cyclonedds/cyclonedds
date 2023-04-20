// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>

#include <CUnit/Test.h>
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/dynlib.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__xevent.h"
#include "ddsi__thread.h"
#include "ddsi__tran.h"
#include "dds/security/core/dds_security_utils.h"
#include "loader.h"

struct plugin_info {
    void *context;
    ddsrt_dynlib_t lib_handle;
    plugin_init func_init;
    plugin_finalize func_fini;
};

struct plugins_hdl {
    struct plugin_info plugin_ac;
    struct plugin_info plugin_auth;
    struct plugin_info plugin_crypto;

    struct ddsi_domaingv gv;
    struct ddsi_tran_conn connection;
};

static void*
load_plugin(
        struct plugin_info *info,
        const char *name_lib,
        const char *name_init,
        const char *name_fini,
        void *args)
{
    dds_return_t result;
    void *plugin = NULL;
    assert(info);

    result = ddsrt_dlopen(name_lib, true, &info->lib_handle);
    if (result == DDS_RETCODE_OK && info->lib_handle) {

        result = ddsrt_dlsym(info->lib_handle, name_init, (void **)&info->func_init);
        if( result != DDS_RETCODE_OK || info->func_init == NULL) {
      char buf[200];
      ddsrt_dlerror(buf, 200);
           printf("ERROR: could not init %s\n. Invalid init function: %s: %s", name_lib, name_init, buf);
           return plugin;
        }

        result = ddsrt_dlsym(info->lib_handle, name_fini, (void **)&info->func_fini);
        if( result != DDS_RETCODE_OK || info->func_fini == NULL ) {
           printf("ERROR: could not init %s\n. Invalid fini function: %s", name_lib, name_fini);
           return plugin;
        }

        char * init_parameters = "";
        (void)info->func_init(init_parameters, &plugin, args);
        if (plugin) {
            info->context = plugin;
        } else {
            printf("ERROR: could not init %s\n", name_lib);
        }
    } else {
      char buffer[300];
      ddsrt_dlerror(buffer,300);
        printf("ERROR: could not load %s. %s\n", name_lib, buffer);
    }
    return plugin;
}

struct plugins_hdl*
load_plugins(
        dds_security_access_control **ac,
        dds_security_authentication **auth,
        dds_security_cryptography   **crypto,
        const struct ddsi_domaingv *gv_init)
{
    struct plugins_hdl *plugins = ddsrt_malloc(sizeof(struct plugins_hdl));
    assert(plugins);
    memset(plugins, 0, sizeof(struct plugins_hdl));

    if (gv_init)
      plugins->gv = *gv_init;
    plugins->connection.m_base.gv = &plugins->gv;
    plugins->gv.xevents = ddsi_xeventq_new (&plugins->gv, 0, 0);

    if (ac) {
        *ac = load_plugin(&(plugins->plugin_ac),
                          "dds_security_ac",
                          "init_access_control",
                          "finalize_access_control",
                          &plugins->gv);
        if (!(*ac)) {
            goto err;
        }
    }
    if (auth) {
        *auth = load_plugin(&(plugins->plugin_auth),
                            "dds_security_auth",
                            "init_authentication",
                            "finalize_authentication",
                            &plugins->gv);
        if (!(*auth)) {
            goto err;
        }
    }
    if (crypto) {
        *crypto = load_plugin(&(plugins->plugin_crypto),
                              "dds_security_crypto",
                              "init_crypto",
                              "finalize_crypto",
                              &plugins->gv);
        if (!(*crypto)) {
            goto err;
        }
    }

    ddsi_thread_states_init();
    ddsi_xeventq_start(plugins->gv.xevents, "TEST");
    return plugins;

err:
    unload_plugins(plugins);
    return NULL;
}

static void
unload_plugin(
        struct plugin_info *info)
{
    dds_return_t result;
    assert(info);

    if (info->lib_handle) {
        if (info->func_fini && info->context) {
            info->func_fini(info->context);
        }
        result = ddsrt_dlclose( info->lib_handle );
        if ( result != 0 ){
          printf( "Error occurred while closing the library\n");
        }
    }
}

void
unload_plugins(
        struct plugins_hdl *plugins)
{
    assert (plugins);
    unload_plugin(&(plugins->plugin_ac));
    unload_plugin(&(plugins->plugin_auth));
    unload_plugin(&(plugins->plugin_crypto));

    ddsi_xeventq_stop(plugins->gv.xevents);
    ddsi_xeventq_free(plugins->gv.xevents);
    ddsrt_free(plugins);

    (void)ddsi_thread_states_fini();
}

static size_t
regular_file_size(
         const char *filename)
{
    size_t sz = 0;
    /* Provided? */
    if (filename) {
        /* Accessible? */
#if _WIN32
        struct _stat stat_info;
        int ret = _stat( filename, &stat_info );
#else
        struct stat stat_info;
        int ret = stat( filename, &stat_info );
#endif
        if ( ret == 0 ) {
            /* Regular? */
#ifdef WIN32
      if (stat_info.st_mode & _S_IFREG) {
#else
      if (S_ISREG(stat_info.st_mode)) {
#endif
                /* Yes, so get the size. */
                sz = ( size_t ) stat_info.st_size;
            }
        }
    }
    return sz;
}

char *
load_file_contents(
    const char *filename)
{
    char *document = NULL;
    char *fname;
    size_t sz, r;
    FILE *fp;

    assert(filename);

    /* Get portable file name. */
    fname = DDS_Security_normalize_file( filename );
    if (fname) {
        /* Get size if it is a accessible regular file (no dir or link). */
        sz = regular_file_size(fname);
        if (sz > 0) {
            /* Open the actual file. */
            DDSRT_WARNING_MSVC_OFF(4996);
            fp = fopen(fname, "r");
            DDSRT_WARNING_MSVC_ON(4996);
            if (fp) {
                /* Read the content. */
                document = ddsrt_malloc(sz + 1);
                r = fread(document, 1, sz, fp);
                if (r == 0) {
                    ddsrt_free(document);
                    document = NULL;
                } else {
                    document[r] = '\0';
                }
                (void)fclose(fp);
            }
        }
        ddsrt_free(fname);
    }

    return document;
}
