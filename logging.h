/*
 * Copyright © 2015 Jon Mayo <jon@rm-f.net>
 */
#ifndef LOGGING_H
#define LOGGING_H
__attribute__((noreturn)) void die(const char *fmt, ...);
void warn(const char *fmt, ...);
void debug(const char *fmt, ...);
void verbose(const char *fmt, ...);
#endif
