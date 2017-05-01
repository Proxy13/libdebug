libdebug
--------

I .. keep reinventing this wheel.  So a few years ago I decided to stop
reinventing this wheel and just implement a very simple, very basic
thread aware debug layer.

This only has C bindings because, well, I write a lot of C.
C++ bindings wouldn't be that hard.

The targets were:

* Runtime extensible
* Runtime configurable
* Debugging must be able to be left in and must not be evaluated unless
  it's about to be written out somewhere
* .. (or has a good chance of it.)
* Logging must be deferred so the caller doesn't block waiting for
  blocking file operations, network operations, etc to occur
* File logging must support close/re-open of logfile, for log rotation

The TL;DR is this:

* This is based on the very useful Squid runtime debugging from yesteryear
* And from some work I did in a yesteryear project called 'libiapp'
  (think libevent, but with async file io, and async debugging, before
  libevent1 was a thing.)
* You allocate debug sections at runtime
* You can set debug levels (like syslog) as well as turn on/off specific
  debug bitmap entries to be able to have sub-sections inside of
  a debug section
* There are configurable sections for three major log destinations:
  + an optional file
  + optionally stderr
  + optionally syslog
* timestamps are already inserted, so you don't have to bother!
* DEBUG(section, level|bitmask, "message", ...) logs message with varargs
  expansion, but /only/ evaluates the varargs and does the heavy lifting
  in creating the log string if section, level, bitmask are enabled
* DEBUG_WARN(section, level|bitmask, "message", ...) is the same as above,
  but instead does a warn() if the current section, level, bitmask are
  enabled
* Evaluation of the initial section, level, bitmask checks are done inline
  and should be pretty fast - so yes, you can keep the bulk of your
  debugging always compiled in!
* Logging (at the moment) is deferred into a logging thread so logging
  to potentially blocking things (such as files, stderr, etc) doesn't
  slow down the rest of the program.

To use it:

* read debug.h
* call debug_init() early
* allocate debug sections using r = debug_register("name");
* Call debug using your debug section
  DEBUG(r, DEBUG_LVL_DEBUG, "%s: test\n", __func__);
* debug_shutdown() when you're done
* debug_setmask() and debug_setmask_str() control the enabled
  debugging information per destination (stderr, syslog, file) as well
  as the debug level/bitmask as appropriate.

TODO:

* Actual Documentation!
* C++ bindings!
* a trace buffer to log binary logging information at runtime;
* a crash buffer/handler, to dump error messages and trace buffer information
  out during a program crash;
* make deferred-to-thread logging configurable (eg log big errors to
  stderr immediately, defer everything else);
* log to a shared memory buffer / debug manager if multiple programs are
  using this library.
* Optionally log directly compressed information to a rotating logfile, for
  embedded targets.
