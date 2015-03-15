/*
 * Copyright © 2015 Jon Mayo <jon@rm-f.net>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <SDL.h>
#include <GL/gl.h>

struct config {
	int width, height;
};

/* default configuration */
static struct config config = {
	.width = 640, .height = 480,
};

static bool keep_going = true;

static __attribute__((noreturn)) void die(const char *fmt, ...);
static void die(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

static void warn(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "Warning: ");
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static void setup_gl(void)
{
	SDL_Window *win = SDL_GL_GetCurrentWindow();
	if (SDL_GL_SetSwapInterval(1) < 0)
		warn("unable to configure VSync\n");

	glClearColor(1.0, 1.0, 0.0, 1.0);
}

static void render(void)
{
	SDL_Window *win = SDL_GL_GetCurrentWindow();
	glClear(GL_COLOR_BUFFER_BIT);
}

int main()
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
		die("SDL Could not initialize! (%s)\n", SDL_GetError());

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

	SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

	SDL_Window *main_window = SDL_CreateWindow("Hero",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		config.width, config.height,
		SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	if (!main_window)
		die("SDL Could not create window! (%s)\n", SDL_GetError());

#if 0
	/* If we're not using GL ... */
	if (0) {
	SDL_Surface *main_surface = SDL_GetWindowSurface(main_window);
	if (!main_surface)
		die("SDL could not get surface buffer! (%s)\n", SDL_GetError());
	}
#endif

	SDL_GLContext main_context = SDL_GL_CreateContext(main_window);
	if (!main_context)
		die("SDL could not create GL context! (%s)\n", SDL_GetError());

	setup_gl();

	/*** Controller ***/
	SDL_StartTextInput(); /* show on-screen keyboard */
	keep_going = true;
	while (keep_going) {
		/* Process every event */
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			switch (ev.type) {
			case SDL_QUIT:
				keep_going = false;
				break;
			case SDL_TEXTINPUT:
				// TODO: do something with this event
				fprintf(stderr, "Info: text=%.32s\n", ev.text.text);
				break;
			}
		}

		/* Render/Paint */
		SDL_GL_MakeCurrent(main_window, main_context); /* not needed for single window applications */
		render();
		SDL_GL_SwapWindow(main_window);
	}

	SDL_DestroyWindow(main_window);
	SDL_Quit();

	return 0;
}
