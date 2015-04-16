/*
 * Copyright © 2015 Jon Mayo <jon@rm-f.net>
 */
#include <stdlib.h>
#include <SDL.h>
#include "logging.h"

/* Logs a message, shows a dialog, then exits. */
__attribute__((noreturn)) void die(const char *fmt, ...);
void die(const char *fmt, ...)
{
	va_list ap;
	char buf[512];
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR,
		"%s", buf);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
		"ERROR starting Hero", buf, NULL);
	exit(EXIT_FAILURE);
}

void warn(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN,
		fmt, ap);
	va_end(ap);
}

void debug(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG,
		fmt, ap);
	va_end(ap);
}

void verbose(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE,
		fmt, ap);
	va_end(ap);
}

void info(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR,
		fmt, ap);
	va_end(ap);
}

void error(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR,
		fmt, ap);
	va_end(ap);
}

