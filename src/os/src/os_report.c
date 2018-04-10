/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifdef PIKEOS_POSIX
#include <lwip_config.h>
#endif

#include "os/os.h"
#include "os/os_project.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define OS_REPORT_TYPE_DEBUG     (1u)
#define OS_REPORT_TYPE_WARNING   (1u<<OS_REPORT_WARNING)
#define OS_REPORT_TYPE_ERROR     (1u<<OS_REPORT_ERROR)
#define OS_REPORT_TYPE_CRITICAL  (1u<<OS_REPORT_CRITICAL)
#define OS_REPORT_TYPE_FATAL     (1u<<OS_REPORT_FATAL)
#define OS_REPORT_TYPE_INFO      (1u<<OS_REPORT_INFO)

#define OS_REPORT_TYPE_FLAG(x)   (1u<<(x))
#define OS_REPORT_IS_ALWAYS(x)   ((x) & (OS_REPORT_TYPE_CRITICAL | OS_REPORT_TYPE_FATAL))
#define OS_REPORT_IS_WARNING(x)  ((x) & OS_REPORT_TYPE_WARNING)
#define OS_REPORT_IS_ERROR(x)    ((x) & (OS_REPORT_TYPE_ERROR | OS_REPORT_TYPE_CRITICAL | OS_REPORT_TYPE_FATAL))

#define OS_REPORT_FLAG_TYPE(x)   (((x) & OS_REPORT_TYPE_FATAL) ? OS_REPORT_FATAL : \
                                  ((x) & OS_REPORT_TYPE_CRITICAL) ? OS_REPORT_CRITICAL : \
                                  ((x) & OS_REPORT_TYPE_ERROR) ? OS_REPORT_ERROR : \
                                  ((x) & OS_REPORT_TYPE_WARNING) ? OS_REPORT_WARNING : \
                                  ((x) & OS_REPORT_TYPE_INFO) ? OS_REPORT_INFO : \
                                  ((x) & OS_REPORT_TYPE_DEBUG) ? OS_REPORT_DEBUG : \
                                  OS_REPORT_NONE)

#define MAX_FILE_PATH 2048

typedef struct os_reportStack_s {
    int count;
    unsigned typeset;
    const char *file;
    int lineno;
    const char *signature;
    os_iter *reports;  /* os_reportEventV1 */
} *os_reportStack;


/**
  * The information that is made available to a plugged in logger
  * via its TypedReport symbol.
  */
struct os_reportEventV1_s
{
    /** The version of this struct i.e. 1. */
    uint32_t version;
    /** The type / level of this report.
      * @see os_reportType */
    os_reportType reportType;
    /** Context information relating to where the even was generated.
      * May contain a function or compnent name or a stacktrace */
    char* reportContext;
    /** The source file name where the report even was generated */
    char* fileName;
    /** The source file line number where the report was generated */
    int32_t lineNo;
    /** An integer code associated with the event. */
    int32_t code;
    /** A description of the reported event */
    char *description;
    /** A string identifying the thread the event occurred in */
    char* threadDesc;
    /** A string identifying the process the event occurred in */
    char* processDesc;
};

#define OS_REPORT_EVENT_V1 1


typedef struct os_reportEventV1_s* os_reportEventV1;

static void os__report_append(_Inout_ os_reportStack _this, _In_ const os_reportEventV1 report);

static void os__report_fprintf(_Inout_ FILE *file, _In_z_ _Printf_format_string_ const char *format, ...);

static void os__report_free(_In_ _Post_invalid_ os_reportEventV1 report);

static void os__report_dumpStack(_In_z_ const char *context, _In_z_ const char *path, _In_ int line);

static FILE* error_log = NULL;
static FILE* info_log = NULL;

static os_mutex reportMutex;
static os_mutex infologcreateMutex;
static os_mutex errorlogcreateMutex;
static bool inited = false;
static bool StaleLogsRemoved = false;

/**
 * Process global verbosity level for OS_REPORT output. os_reportType
 * values >= this value will be written.
 * This value defaults to OS_REPORT_INFO, meaning that all types 'above' (i.e.
 * other than) OS_REPORT_DEBUG will be written and OS_REPORT_DEBUG will not be.
 */
os_reportType os_reportVerbosity = OS_REPORT_INFO;

/**
 * Labels corresponding to os_reportType values.
 * @see os_reportType
 */
const char *os_reportTypeText [] = {
        "DEBUG",
        "INFO",
        "WARNING",
        "ERROR",
        "CRITICAL",
        "FATAL",
        "NONE"
};

enum os_report_logType {
    OS_REPORT_INFO_LOG,
    OS_REPORT_ERROR_LOG
};

static char * os_report_defaultInfoFileName = OS_PROJECT_NAME_NOSPACE_SMALL"-info.log";
static char * os_report_defaultErrorFileName = OS_PROJECT_NAME_NOSPACE_SMALL"-error.log";
static const char os_env_logdir[] = OS_PROJECT_NAME_NOSPACE_CAPS"_LOGPATH";
static const char os_env_infofile[] = OS_PROJECT_NAME_NOSPACE_CAPS"_INFOFILE";
static const char os_env_errorfile[] = OS_PROJECT_NAME_NOSPACE_CAPS"_ERRORFILE";
static const char os_env_verbosity[] = OS_PROJECT_NAME_NOSPACE_CAPS"_VERBOSITY";
static const char os_env_append[] = OS_PROJECT_NAME_NOSPACE_CAPS"_LOGAPPEND";
#if defined _WRS_KERNEL
static const char os_default_logdir[] = "/tgtsvr";
#else
static const char os_default_logdir[] = ".";
#endif

static _Ret_maybenull_ FILE *
os__open_file (
        _In_z_ const char * file_name)
{
    FILE *logfile=NULL;
    const char *dir = os_getenv (os_env_logdir);
    char * nomalized_dir;
    char *str;
    size_t len;
    os_result res = os_resultSuccess;

    if (dir == NULL) {
        dir = os_default_logdir;
    }

    len = strlen (dir) + 2; /* '/' + '\0' */
    str = os_malloc (len);
    if (str != NULL) {
        (void)snprintf (str, len, "%s/", dir);
        nomalized_dir = os_fileNormalize (str);
        os_free (str);
        if (nomalized_dir == NULL) {
            res = os_resultFail;
        }
        os_free(nomalized_dir);
    } else {
        res = os_resultFail;
    }

    if (res == os_resultSuccess) {
        logfile = fopen (file_name, "a");
    }

    return logfile;
}

static void
os__close_file (
    _In_z_ const char * file_name,
    _In_ _Post_invalid_ FILE *file)
{
    if (strcmp(file_name, "<stderr>") != 0 && strcmp(file_name, "<stdout>") != 0)
    {
        fclose(file);
    }
}

_Check_return_ _Ret_z_
static char *os__report_createFileNormalize(
        _In_z_ const char *file_dir,
        _In_z_ const char *file_name)
{
    char file_path[MAX_FILE_PATH];
    int len;

    len = snprintf(file_path, MAX_FILE_PATH, "%s/%s", file_dir, file_name);
    /* Note bug in glibc < 2.0.6 returns -1 for output truncated */
    if ( len < MAX_FILE_PATH && len > -1 ) {
        return (os_fileNormalize(file_path));
    } else {
        return os_strdup(file_name);
    }
}

/**
 * Return either a log file path string or a pseudo file name/path value
 * like <stdout> or <stderr>.
 * The result of os_report_file_path must be freed with os_free
 * @param override_variable An environment variable name that may hold a filename
 * or pseudo filename. If this var is set, and is not a pseudo filename,
 * the value of this var will be added to the value of env variable
 * {OS_PROJECT_NAME_NOSPACE_CAPS}_LOGPATH (if set or './' if not) to create the
 * log file path.
 * @param default_file If override_variable is not defined in the environment
 * this is the filename used.
 */
_Ret_z_
_Check_return_
static char *
os__report_file_path(
        _In_z_ const char * default_file,
        _In_opt_z_ const char * override_variable,
        _In_ enum os_report_logType type)
{
    const char *file_dir;
    const char *file_name = NULL;

    if (override_variable != NULL)
    {
        file_name = os_getenv(override_variable);
    }
    if (!file_name)
    {
        file_name = default_file;
    }

    file_dir = os_getenv(os_env_logdir);
    if (!file_dir)
    {
        file_dir = os_default_logdir;
    }
    else
    {
        /* We just need to check if a file can be written to the directory, we just use the
         * default info file as there will always be one created, if we used the variables
         * passed in we would create an empty error log (which is bad for testing) and we
         * cannot delete it as we open the file with append
         */
        if (type == OS_REPORT_INFO_LOG)
        {
            FILE *logfile;
            char *full_file_path;

            full_file_path = os_malloc(strlen(file_dir) + 1 + strlen(file_name) + 1);
            strcpy(full_file_path, file_dir);
            strcat(full_file_path, "/");
            strcat(full_file_path, file_name);
            logfile = fopen (full_file_path, "a");
            if (logfile)
            {
                fclose(logfile);
            }
            os_free (full_file_path);
        }
    }
    if (strcmp(file_name, "<stderr>") != 0 && strcmp(file_name, "<stdout>") != 0)
    {
        return (os__report_createFileNormalize(file_dir, file_name));
    }
    return os_strdup (file_name);
}

/**
 * Overrides the current minimum output level to be reported from
 * this process.
 * @param newVerbosity String holding either an integer value corresponding
 * to an acceptable (in range) log verbosity or a string verbosity 'name'
 * like 'ERROR' or 'warning' or 'DEBUG' or somesuch.
 * @return os_resultFail if the string contains neither of the above;
 * os_resultSuccess otherwise.
 */
static os_result
os__determine_verbosity(
    _In_z_ const char* newVerbosity)
{
    long verbosityInt;
    os_result result = os_resultFail;
    verbosityInt = strtol(newVerbosity, NULL, 0);

    if (verbosityInt == 0 && strcmp("0", newVerbosity)) {
        /* Conversion from int failed. See if it's one of the string forms. */
        while (verbosityInt < (long) (sizeof(os_reportTypeText) / sizeof(os_reportTypeText[0]))) {
            if (os_strcasecmp(newVerbosity, os_reportTypeText[verbosityInt]) == 0) {
                break;
            }
            ++verbosityInt;
        }
    }
    if (verbosityInt >= 0 && verbosityInt < (long) (sizeof(os_reportTypeText) / sizeof(os_reportTypeText[0]))) {
        /* OS_API_INFO label is kept for backwards compatibility. */
        os_reportVerbosity = (os_reportType)verbosityInt;
        result = os_resultSuccess;
    }

    return result;
}

void os__set_verbosity(void)
{
    const char * envValue = os_getenv(os_env_verbosity);
    if (envValue != NULL)
    {
        if (os__determine_verbosity(envValue) == os_resultFail)
        {
            OS_WARNING("os_reportInit", 0,
                    "Cannot parse report verbosity %s value \"%s\","
                    " reporting verbosity remains %s", os_env_verbosity, envValue, os_reportTypeText[os_reportVerbosity]);
        }
    }
}

/**
 * Get the destination for logging error reports. Env property {OS_PROJECT_NAME_NOSPACE_CAPS}_INFOFILE and
 * {OS_PROJECT_NAME_NOSPACE_CAPS}_LOGPATH controls this value.
 * If {OS_PROJECT_NAME_NOSPACE_CAPS}_INFOFILE is not set & this process is a service, default
 * to logging to a file named {OS_PROJECT_NAME_NOSPACE_SMALL}-info.log, otherwise
 * use standard out.
 * @see os_report_file_path
 */
_Ret_z_
_Check_return_
static char *
os__get_info_file_name(void)
{
    char * file_name;
    os_mutexLock(&infologcreateMutex);
    file_name = os__report_file_path (os_report_defaultInfoFileName, os_env_infofile, OS_REPORT_INFO_LOG);
    os_mutexUnlock(&infologcreateMutex);
    return file_name;
}

/**
 * Get the destination for logging error reports. Env property {OS_PROJECT_NAME_NOSPACE_CAPS}_ERRORFILE and
 * {OS_PROJECT_NAME_NOSPACE_CAPS}_LOGPATH controls this value.
 * If {OS_PROJECT_NAME_NOSPACE_CAPS}_ERRORFILE is not set & this process is a service, default
 * to logging to a file named {OS_PROJECT_NAME_NOSPACE_SMALL}-error.log, otherwise
 * use standard error.
 * @see os_report_file_path
 */
_Ret_z_
_Check_return_
static char *
os__get_error_file_name(void)
{
    char * file_name;
    os_mutexLock(&errorlogcreateMutex);
    file_name = os__report_file_path (os_report_defaultErrorFileName, os_env_errorfile, OS_REPORT_ERROR_LOG);
    os_mutexUnlock(&errorlogcreateMutex);
    return file_name;
}

static void os__remove_stale_logs(void)
{
    if (!StaleLogsRemoved) {
        /* TODO: Only a single process or spliced (as 1st process) is allowed to
         * delete the log files. */
        /* Remove ospl-info.log and ospl-error.log.
         * Ignore the result because it is possible that they don't exist yet. */
        char * log_file_name;

        log_file_name = os__get_error_file_name();
        (void)os_remove(log_file_name);
        os_free(log_file_name);

        log_file_name = os__get_info_file_name();
        (void)os_remove(log_file_name);
        os_free(log_file_name);

        StaleLogsRemoved = true;
    }
}


static void os__check_removal_stale_logs(void)
{
    const char * envValue = os_getenv(os_env_append);
    if (envValue != NULL)
    {
        if (os_strcasecmp(envValue, "FALSE") == 0 ||
            os_strcasecmp(envValue, "0") == 0 ||
-           os_strcasecmp(envValue, "NO") == 0) {
          os__remove_stale_logs();
        }
    }
}

/**
 * Read environment properties. In particular ones that can't be left until
 * there is a requirement to log.
 */
void os_reportInit(
        _In_ bool forceReInit)
{
    if (forceReInit) {
        inited = false;
    }

    if (!inited)
    {
        os_mutexInit(&reportMutex);
        os_mutexInit(&errorlogcreateMutex);
        os_mutexInit(&infologcreateMutex);

        os__check_removal_stale_logs();
        os__set_verbosity();
    }
    inited = true;
}

void os_reportExit(void)
{
    char *name;
    os_reportStack reports;

    reports = os_threadMemGet(OS_THREAD_REPORT_STACK);
    if (reports) {
        os__report_dumpStack(OS_FUNCTION, __FILE__, __LINE__);
        os_iterFree(reports->reports, NULL);
        os_threadMemFree(OS_THREAD_REPORT_STACK);
    }
    inited = false;
    os_mutexDestroy(&reportMutex);

    if (error_log)
    {
        name = os__get_error_file_name();
        os__close_file(name, error_log);
        os_free (name);
        error_log = NULL;
    }

    if (info_log)
    {
        name = os__get_info_file_name();
        os__close_file(name, info_log);
        os_free (name);
        info_log = NULL;
    }
    os_mutexDestroy(&errorlogcreateMutex);
    os_mutexDestroy(&infologcreateMutex);
}

static void os__report_fprintf(
        _Inout_ FILE *file,
        _In_z_ _Printf_format_string_ const char *format, ...)
{
    int BytesWritten = 0;
    va_list args;
    va_start(args, format);
    BytesWritten = os_vfprintfnosigpipe(file, format, args);
    va_end(args);
    if (BytesWritten == -1) {
        /* error occurred ?, try to write to stdout. (also with no sigpipe,
         * stdout can also give broken pipe)
         */
        va_start(args, format);
        (void) os_vfprintfnosigpipe(stdout, format, args);
        va_end(args);
    }
}

static _Ret_notnull_ FILE* os__get_info_file (void)
{
    if (info_log == NULL) {
      char * name = os__get_info_file_name();
      info_log = os__open_file(name);
      if (!info_log)
      {
          info_log = stdout;
      }
      os_free (name);
    }
    return info_log;
}

static _Ret_notnull_ FILE* os__get_error_file (void)
{
    if (error_log == NULL) {
      char * name = os__get_error_file_name();
      error_log = os__open_file(name);
      if (!error_log)
      {
          error_log = stderr;
      }
      os_free (name);
    }
    return error_log;
}

static void os__sectionReport(
        _Pre_notnull_ _Post_notnull_ os_reportEventV1 event,
        _In_ bool useErrorLog)
{
    os_time ostime;
    FILE *log = useErrorLog ? os__get_error_file() : os__get_info_file();

    ostime = os_timeGet();
    os_mutexLock(&reportMutex);
    os__report_fprintf(log,
        "----------------------------------------------------------------------------------------\n"
        "Report      : %s\n"
        "Internals   : %s/%s/%d/%d/%d.%09d\n",
        event->description,
        event->reportContext,
        event->fileName,
        event->lineNo,
        event->code,
        ostime.tv_sec,
        ostime.tv_nsec);
    fflush (log);
    os_mutexUnlock(&reportMutex);
}

static void os__headerReport(
        _Pre_notnull_ _Post_notnull_ os_reportEventV1 event,
        _In_ bool useErrorLog)
{
    os_time ostime;
    char node[64];
    char date_time[128];
    FILE *log = NULL;
    if (useErrorLog)
      log = os__get_error_file();
    else log = os__get_info_file();

    ostime = os_timeGet();
    os_ctime_r(&ostime, date_time, sizeof(date_time));

    if (os_gethostname(node, sizeof(node)-1) == os_resultSuccess)
    {
        node[sizeof(node)-1] = '\0';
    }
    else
    {
        strcpy(node, "UnkownNode");
    }

    os_mutexLock(&reportMutex);

    os__report_fprintf(log,
        "========================================================================================\n"
        "ReportType  : %s\n"
        "Context     : %s\n"
        "Date        : %s\n"
        "Node        : %s\n"
        "Process     : %s\n"
        "Thread      : %s\n"
        "Internals   : %s/%d/%s/%s/%s\n",
        os_reportTypeText[event->reportType],
        event->description,
        date_time,
        node,
        event->processDesc,
        event->threadDesc,
        event->fileName,
        event->lineNo,
        OSPL_VERSION_STR,
        OSPL_INNER_REV_STR,
        OSPL_OUTER_REV_STR);

    fflush (log);

    os_mutexUnlock(&reportMutex);
}

static void os__defaultReport(
        _Pre_notnull_ _Post_notnull_ os_reportEventV1 event)
{
    os_time ostime;
    char node[64];
    char date_time[128];
    FILE *log;

    switch (event->reportType) {
    case OS_REPORT_DEBUG:
    case OS_REPORT_INFO:
    case OS_REPORT_WARNING:
        log = os__get_info_file();
        break;
    case OS_REPORT_ERROR:
    case OS_REPORT_CRITICAL:
    case OS_REPORT_FATAL:
    default:
        log = os__get_error_file();
        break;
    }

    ostime = os_timeGet();
    os_ctime_r(&ostime, date_time, sizeof(date_time));

    if (os_gethostname(node, sizeof(node)-1) == os_resultSuccess)
    {
        node[sizeof(node)-1] = '\0';
    }
    else
    {
        strcpy(node, "UnkownNode");
    }

    os_mutexLock(&reportMutex);
    os__report_fprintf (log,
        "========================================================================================\n"
        "Report      : %s\n"
        "Date        : %s\n"
        "Description : %s\n"
        "Node        : %s\n"
        "Process     : %s\n"
        "Thread      : %s\n"
        "Internals   : %s/%s/%s/%s/%s/%d/%d/%d.%09d\n",
        os_reportTypeText[event->reportType],
        date_time,
        event->description,
        node,
        event->processDesc,
        event->threadDesc,
        OSPL_VERSION_STR,
        OSPL_INNER_REV_STR,
        OSPL_OUTER_REV_STR,
        event->reportContext,
        event->fileName,
        event->lineNo,
        event->code,
        ostime.tv_sec,
        ostime.tv_nsec);
    fflush (log);
    os_mutexUnlock(&reportMutex);
}



void os_report_message(
        _In_ os_reportType type,
        _In_z_ const char *context,
        _In_z_ const char *path,
        _In_ int32_t line,
        _In_ int32_t code,
        _In_z_ const char *message)
{
    char *file;
    char procid[256], thrid[64];
    os_reportStack stack;

    struct os_reportEventV1_s report = { OS_REPORT_EVENT_V1, /* version */
            OS_REPORT_NONE, /* reportType */
            NULL, /* reportContext */
            NULL, /* fileName */
            0, /* lineNo */
            0, /* code */
            NULL, /* description */
            NULL, /* threadDesc */
            NULL /* processDesc */
    };

    file = (char *)path;

    /* Only figure out process and thread identities if the user requested an
       entry in the default log file or registered a typed report plugin. */
    os_procNamePid (procid, sizeof (procid));
    os_threadFigureIdentity (thrid, sizeof (thrid));

    report.reportType = type;
    report.reportContext = (char *)context;
    report.fileName = (char *)file;
    report.lineNo = line;
    report.code = code;
    report.description = (char *)message;
    report.threadDesc = thrid;
    report.processDesc = procid;

    stack = (os_reportStack)os_threadMemGet(OS_THREAD_REPORT_STACK);
    if (stack && stack->count) {
        if (report.reportType != OS_REPORT_NONE) {
            os__report_append (stack, &report);
        }
    } else {
        os__defaultReport (&report);
    }
}

void os_report(
        _In_ os_reportType type,
        _In_z_ const char *context,
        _In_z_ const char *path,
        _In_ int32_t line,
        _In_ int32_t code,
        _In_z_ _Printf_format_string_ const char *format,
        ...)
{
    char buf[OS_REPORT_BUFLEN];
    va_list args;

    if (!inited) {
        return;
    }

    if (type < os_reportVerbosity) {
        return;
    }

    va_start (args, format);
    (void)os_vsnprintf (buf, sizeof(buf), format, args);
    va_end (args);

    os_report_message(type, context, path, line, code, buf);
}

/*****************************************
 * Report-stack related functions
 *****************************************/
void os_report_stack(void)
{
    os_reportStack _this;

    if (inited == false) {
        return;
    }
    _this = (os_reportStack)os_threadMemGet(OS_THREAD_REPORT_STACK);
    if (!_this) {
        /* Report stack does not exist yet, so create it */
        _this = os_threadMemMalloc(OS_THREAD_REPORT_STACK, sizeof(struct os_reportStack_s));
        if (_this) {
            _this->count = 1;
            _this->typeset = 0;
            _this->file = NULL;
            _this->lineno = 0;
            _this->signature = NULL;
            _this->reports = os_iterNew();
        } else {
            OS_ERROR("os_report_stack", 0,
                    "Failed to initialize report stack (could not allocate thread-specific memory)");
        }
    } else {
        /* Use previously created report stack */
        if (_this->count == 0) {
            _this->file = NULL;
            _this->lineno = 0;
            _this->signature = NULL;
        }
        _this->count++;

    }
}

void os_report_stack_free(void)
{
    os_reportStack _this;
    os_reportEventV1 report;

    _this = (os_reportStack)os_threadMemGet(OS_THREAD_REPORT_STACK);
    if (_this) {
        while((report = os_iterTake(_this->reports, -1))) {
            os__report_free(report);
        }
        os_iterFree(_this->reports, NULL);
        os_threadMemFree(OS_THREAD_REPORT_STACK);
    }
}

static void
os__report_stack_unwind(
        _Inout_ os_reportStack _this,
        _In_ bool valid,
        _In_z_ const char *context,
        _In_z_ const char *path,
        _In_ int line)
{
    struct os_reportEventV1_s header;
    os_reportEventV1 report;
    char *file;
    bool useErrorLog;
    os_reportType reportType = OS_REPORT_ERROR;

    if (!valid) {
        if (OS_REPORT_IS_ALWAYS(_this->typeset)) {
            valid = true;
        }
    } else {
        reportType = OS_REPORT_FLAG_TYPE(_this->typeset);
    }

    useErrorLog = OS_REPORT_IS_ERROR(_this->typeset);

    /* Typeset will be set when a report was appended. */
    if (valid && (_this->typeset != 0)) {
        char proc[256], procid[256];
        char thr[64], thrid[64];
        os_procId pid;
        uintmax_t tid;

        assert (context != NULL);
        assert (path != NULL);

        file = (char *)path;

        pid = os_procIdSelf ();
        tid = os_threadIdToInteger (os_threadIdSelf ());

        os_procNamePid (procid, sizeof (procid));
        os_procName (proc, sizeof (proc));
        os_threadFigureIdentity (thrid, sizeof (thrid));
        os_threadGetThreadName (thr, sizeof (thr));

        header.reportType = reportType;
        header.description = (char *)context;
        header.processDesc = procid;
        header.threadDesc = thrid;
        header.fileName = file;
        header.lineNo = line;

        os__headerReport (&header, useErrorLog);
    }

    while ((report = os_iterTake(_this->reports, -1))) {
        if (valid) {
            os__sectionReport (report, useErrorLog);
        }
        os__report_free(report);
    }

    _this->typeset = 0;
}

static void
os__report_dumpStack(
        _In_z_ const char *context,
        _In_z_ const char *path,
        _In_ int line)
{
    os_reportStack _this;

    if (inited == false) {
        return;
    }
    _this = os_threadMemGet(OS_THREAD_REPORT_STACK);
    if ((_this) && (_this->count > 0)) {
        os__report_stack_unwind(_this, true, context, path, line);
    }
}

void os_report_flush(
        _In_ bool valid,
        _In_z_ const char *context,
        _In_z_ const char *path,
        _In_ int line)
{
    os_reportStack _this;

    if (inited == false) {
        return;
    }
    _this = os_threadMemGet(OS_THREAD_REPORT_STACK);
    if ((_this) && (_this->count)) {
        if (_this->count == 1) {
            os__report_stack_unwind(_this, valid, context, path, line);
            _this->file = NULL;
            _this->signature = NULL;
            _this->lineno = 0;
        }
        _this->count--;
    }
}

#define OS__STRDUP(str) (str != NULL ? os_strdup(str) : os_strdup("NULL"))

static void
os__report_append(
    _Inout_ os_reportStack _this,
    _In_ const os_reportEventV1 report)
{
    os_reportEventV1 copy;

    copy = os_malloc(sizeof(*copy));
    copy->code = report->code;
    copy->description = OS__STRDUP(report->description);
    copy->fileName = OS__STRDUP(report->fileName);
    copy->lineNo = report->lineNo;
    copy->processDesc = OS__STRDUP(report->processDesc);
    copy->reportContext = OS__STRDUP(report->reportContext);
    copy->reportType = report->reportType;
    copy->threadDesc = OS__STRDUP(report->threadDesc);
    copy->version = report->version;
    _this->typeset |= OS_REPORT_TYPE_FLAG(report->reportType);
    os_iterAppend(_this->reports, copy);
}

static void
os__report_free(
    _In_ _Post_invalid_ os_reportEventV1 report)
{
    os_free(report->description);
    os_free(report->fileName);
    os_free(report->processDesc);
    os_free(report->reportContext);
    os_free(report->threadDesc);
    os_free(report);
}
