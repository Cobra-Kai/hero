/*
 * Copyright © 2015 Jon Mayo <jon@rm-f.net>
 */
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <SDL.h>
#include <GL/gl.h>

struct config {
	int width, height;
	bool verbose; /* enable to turn on extra logging */
	bool use_vsync;
};

struct game_state {
	GLuint win_x, win_y, win_w, win_h; /* used to crop to preserve aspect */
};

/* global configuration */
static struct config config = {
	.width = 960, .height = 540,
	.verbose = true,
	.use_vsync = false,
};

static bool keep_going = true;

static __attribute__((noreturn)) void die(const char *fmt, ...);
static void die(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR,
		fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

static void warn(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN,
		fmt, ap);
	va_end(ap);
}

static void setup_gl(void)
{
	SDL_Window *win = SDL_GL_GetCurrentWindow();

	/* setup the config.use_vsync option */
	if (SDL_GL_GetSwapInterval() != -1) {
		if (SDL_GL_SetSwapInterval(config.use_vsync ? 1 : 0) < 0)
			warn("unable to configure VSync\n");
	} else {
		warn("Configuring VSync is not supported!\n");
	}

	glClearColor(1.0, 0.75, 0.0, 1.0);
}

static void game_paint(void)
{
	SDL_Window *win = SDL_GL_GetCurrentWindow();
	struct game_state *state = SDL_GetWindowData(win, "game");
	SDL_assert(state != NULL);

	glScissor(state->win_x, state->win_y, state->win_w, state->win_h);
	glClear(GL_COLOR_BUFFER_BIT);
}

static void usage(const char *argv0)
{
	// TODO: should we replace this with SDL_Log() ?
	fprintf(stderr, "%s [-geometry %dx%d]\n",
		argv0, config.width, config.height);
	exit(EXIT_FAILURE);
}

static void parse_args(int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; ) {
		const char *cur = argv[i++];

		if (!strcmp(cur, "-help") || !strcmp(cur, "-h")) {
			usage(argv[0]);
		} else if (!strcmp(cur, "-geometry") || !strcmp(cur, "-geom")) {
			if (i >= argc) {
				fprintf(stderr, "ERROR at %s\n", cur);
				usage(argv[0]);
			}
			const char *arg = argv[i++];
			if (sscanf(arg, "%dx%d", &config.width, &config.height) != 2) {
				fprintf(stderr, "ERROR at %s\n", cur);
				usage(argv[0]);
			}
		} else if (!strcmp(cur, "-verbose")) {
			config.verbose = true;
		} else if (!strcmp(cur, "-vsync")) {
			config.use_vsync = true;
		} else if (!strcmp(cur, "-novsync") ||
			!strcmp(cur, "-no-vsync")) {
			config.use_vsync = false;
		} else {
			fprintf(stderr, "ERROR unknown option %s\n", cur);
			usage(argv[0]);
		}
	}
}

int main(int argc, char **argv)
{
	parse_args(argc, argv);

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
		die("SDL Could not initialize! (%s)\n", SDL_GetError());

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

	if (config.verbose) {
		SDL_LogSetAllPriority(SDL_LOG_PRIORITY_WARN);
	} else {
		SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION,
			SDL_LOG_PRIORITY_INFO);
	}

	SDL_Window *main_window = SDL_CreateWindow("Hero",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		config.width, config.height,
		SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	if (!main_window)
		die("SDL Could not create window! (%s)\n", SDL_GetError());

	/* create the game related data and attach it to a window */
	struct game_state *main_state = calloc(1, sizeof(*main_state));
	int width, height;
	SDL_GetWindowSize(main_window, &width, &height);
	main_state->win_x = 0;
	main_state->win_y = 0;
	main_state->win_w = width;
	main_state->win_h = height;
	SDL_SetWindowData(main_window, "game", main_state);

	/** GL related setup **/
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
				SDL_Log("ext=%.32s\n", ev.text.text);
				break;
			}
		}

		/* Render/Paint */
		SDL_GL_MakeCurrent(main_window, main_context); /* not needed for single window applications */
		game_paint();
		SDL_GL_SwapWindow(main_window);
	}

	SDL_DestroyWindow(main_window);
	SDL_Quit();

	return 0;
}
