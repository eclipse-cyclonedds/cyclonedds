# Logging and tracing in Cyclone DDS
A lot of effort has gone into providing as much useful information as possible
when a log message is written and a fair number of mechanisms were put in
place to be able to do so.

The difficulty with logging in Cyclone DDS is the fact that it is both used by
user applications and internal services alike. Both use-cases have different
needs with regard to logging. e.g. a service may just want to write all
information to a log file, whereas a user application may want to display the
message in a graphical user interface or reformat it and write it to stdout.

> This document does not concern itself with error handling, return codes,
> etc. Only logging and tracing are covered.


## Design
The logging and tracing api offered by Cyclone DDS is a merger of the reporting
functionality that existed in the various modules that Cyclone is made out of.
Why the API needed to change is documented in the section
[Issues with previous implementation].

The new and improved logging mechanism in Cyclone DDS allows the user to simply
register a callback function, as is customary for libraries, that matches the
signature `dds_log_write_fn_t`. The callback can be registered by passing it
to `dds_set_log_sink` or `dds_set_trace_sink`. The functions also allow for
registering a `void *` that will be passed along on every invocation of the
the callback. To unregister a log or trace sink, call the respective function
and pass NULL for the callback. This will cause Cyclone DDS to reinstate the
default handler. aka stdout, stderr, or the file specified in the configuration
file.

```C
typedef void(*dds_log_write_fn_t)(dds_log_data_t *, void *);

struct dds_log_data {
  uint32_t priority;
  const char *file;
  uint32_t line;
  const char *function;
  const char *message;
};

typedef struct dds_log_data dds_log_data_t;

dds_return_t dds_set_log_sink(dds_log_write_fn_t *, const void *);
dds_return_t dds_set_trace_sink(dds_log_write_fn_t *, const void *);
```

> Cyclone DDS offers both a logging and a tracing callback to allow the user to
> easily send traces to a different device than the log messages. The trace
> function receives both the log messages and trace messages, while the log
> function will only receive the log messages.

> The user is responsible for managing resources himself. Cyclone DDS will not
> attempt to free *userdata* under any circumstance. The user should revert to
> the default handler, or register a different sink before invalidating the
> userdata pointer.

> The *dds_set_log_sink* and *dds_set_trace_sink* functions are synchronous.
> It is guaranteed that on return no thread in the Cyclone DDS stack will
> reference the sink or the *userdata*.

To minimize the amount of information that is outputted when tracing is
enabled, the user can enable/disable tracing per category. Actually the
priority member that is passed to the handler consists of the priority,
e.g. error, info, etc and (if it's a trace message) the category.

To be specific. The last four bits of the 32-bit integer contain the priority.
The other bits implicitly indicate it's a trace message and are reserved to
specify the category to which a message belongs.

```C
#define DDS_LC_FATAL 1u
#define DDS_LC_ERROR 2u
#define DDS_LC_WARNING 4u
#define DDS_LC_INFO 8u
#define DDS_LC_CONFIG 16u
...
```

> DDSI is the Cyclone DDS module that the log and trace mechanism originated
> from. For that reason not all categories make sense to use in API code and
> some categories that you would expect may be missing. For now the categories
> are a work-in-progress and may be changed without prior notice.

To control which messages are passed to the registered sinks, the user
must call `dds_set_log_mask` or `dds_set_trace_mask`. Whether or not to call
the internal log and trace functions depends on it. The user is strongly
discouraged to enable all categories and filter messages in the registered
handler, because of the performance impact!

> Tests have shown performance to decrease by roughly 5% if the decision on
> whether or not to write the message is done outside the function. The reason
> obviously not being the *if*-statement, but the creation of the stack frame.

For developer convenience, a couple of macro's are be introduced so the
developer does not have to write boilerplate code each time. The
implementation will roughly follow what is specified below.

```C
#define DDS_FATAL(fmt, ...)
#define DDS_ERROR(fmt, ...)
#define DDS_INFO(fmt, ...)
#define DDS_WARNING(fmt, ...)
#define DDS_TRACE(cat, fmt, ...)
```

> Log and trace messages are finalized by a newline. If a newline is not
> present, the buffer is not flushed. This is can be used to extend messages,
> e.g. to easily append summaries and decisions, and already used throughout
> the ddsi module. The newline is replaced by a null character when the
> message is passed to a sink.

### Default handlers and behavior

If the user does not register a sink, messages are printed to the default
location. Usually this means a cyclonedds.log file created in the working
directory, but the location can be changed in the configuration file. By
default only error and warning messages will be printed.

> As long as no file is open (or e.g. a syslog connection is established),
> messages are printed to stderr.

For convenience a number of log handlers will ship with Cyclone. Initially
the set will consist of a handler that prints to stdout/stderr and one that
prints to a file. However, at some point it would be nice to ship handlers
that can print to the native log api offered by a target. e.g.

 * Windows Event Log on Microsoft Windows
 * Syslog on Linux
 * Unified Logging on macOS
 * logMsg or the ED&R (Error Detection and Reporting) subsystem on VxWorks

> For now it is unclear what configuration options are available for all the
> default handlers or how the API to update them will look exactly.

## Log message guidelines
* Write concise reports.
 * Do not leave out information, but also don't turn it into an essay.
   e.g. a message for a failed write operation could include topic and
   partition names to indicate the affected system scope.
* Write consistent reports.
 * Use the name of the parameter as it appears in the documentation for that
   language binding to reference a parameter where applicable.
 * Use the same formatting style as other messages in the same module.
   * e.g. use "could not ..." or "failed to ..." consistently.
 * Avoid duplicate reports as much as possible.
   e.g. if a problem is reported in a lower layer, do not report it again when
   the error is propagated.
 * Discuss with one of the team members if you must deviate from the
   formatting rules/guidelines and update this page if applicable.
* Report only meaningful events.


## Issues with previous implementation
* Multiple files are opened. DDSI, by default, writes to cyclonedds-trace.log,
  but also contains info, warning and error log messages written by DDSI
  itself. All other modules write to cyclone-info.log and cyclone-error.log.
  Options are available, but they're all over the place. Some are adjusted by
  setting certain environment variables, others are configured by modifying the
  configuration file. Not exactly what you'd call user friendly. Also, simply
  writing a bunch of files from a library is not considered good practice.

* Cyclone only offers writing to a *FILE* handle. The filenames mentioned
  above by default, but a combination of stdout/stderr can also be used. There
  is no easy way to display them in a GUI or redirect them to a logging API.
  e.g. syslog on Linux or the Windows Event Log on Microsoft Windows.

* The report stack is a cumbersome mechanism to use and (to a certain extend)
  error prone. Every function (at least every function in the "public" APIs)
  must start with a report stack instruction and end with a report flush
  instruction.

* Cyclone assumes files can always be written. For a number of supported
  targets, e.g. FreeRTOS and VxWorks, this is often not the case. Also,
  filling the memory with log files is probably undesirable.

* Cyclone (except for DDSI) does not support log categories to selectively
  enable/disable messages that the user is interested in. Causing trace logs
  to contain (possibly) too much information.

* The report mechanism expects an error code, but it's unclear what "type" of
  code it is. It can be a valid errno value, os_result, utility result, DDS
  return code, etc. While the problem can be fixed by adding a module field to
  the log message struct. The message is often generated in the layer above
  the layer that the error originated from. Apart from that, the report
  callback is not the place to handle errors gracefully. Return codes and
  exceptions are the mechanism to do that.

* The logging API is different across modules. DDSI uses the ddsi_log macro
  family, the abstraction layer uses the OS_REPORT family and DDS uses the
  dds_report family. Apart from the macros used, the information that would
  end up in the report callback is also different.

