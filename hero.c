/*
 * Copyright © 2015 Jon Mayo <jon@rm-f.net>
 */
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#define GL_GLEXT_PROTOTYPES
#include <SDL.h>
#include <GL/gl.h>
#include <GL/glext.h>

struct config {
	int width, height;
	bool verbose; /* enable to turn on extra logging */
	bool use_vsync;
};

struct game_state {
	GLuint win_x, win_y, win_w, win_h; /* used to crop to preserve aspect */
	struct act {
		bool up, down, left, right;
		bool turn_left, turn_right;
	} act; /* actions */
	GLdouble player_x, player_y; /* current player position */
	GLdouble player_facing, player_height;
	Uint32 last_tick;
	bool text_input;
};

/* global configuration */
static struct config config = {
	.width = 960, .height = 540,
	.verbose = false,
	.use_vsync = false,
};

static bool keep_going = true;

/* Logs a message, shows a dialog, then exits. */
static __attribute__((noreturn)) void die(const char *fmt, ...);
static void die(const char *fmt, ...)
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

	// orange: glClearColor(1.0, 0.75, 0.0, 1.0);
	glClearColor(0.0, 0.0, 0.0, 1.0);
}

/** MVC: Model - represent the data */

/* a sector is a convex 2D polygon. each side is a wall or portal. */
#define MAX_SIDES 64
struct sector_vertex {
	GLfloat x, y;
};

struct map_sector {
	GLfloat floor_height, ceil_height;
	unsigned num_sides;
	struct sector_vertex sides_xy[MAX_SIDES]; /* in clockwise order */
};

const struct map_sector dummy_sector = {
	.floor_height = 0.0,
	.ceil_height = 2.0,
	.num_sides = 4,
	.sides_xy[0].x = 1,
	.sides_xy[0].y = 2,
	.sides_xy[1].x = 5,
	.sides_xy[1].y = 7,
	.sides_xy[2].x = 5,
	.sides_xy[2].y = 11,
	.sides_xy[3].x = 1,
	.sides_xy[3].y = 16,
};

/* calculates the center of a sector through averaging every vertex. */
static void sector_find_center(const struct map_sector *sec, GLdouble *x, GLdouble *y)
{
	unsigned i;
	GLdouble total_x = 0.0, total_y = 0.0;
	for (i = 0; i < sec->num_sides; i++) {
		const struct sector_vertex *cur = &sec->sides_xy[i];
		total_x += cur->x;
		total_y += cur->y;
	}
	if (x)
		*x = total_x / sec->num_sides;
	if (y)
		*y = total_y / sec->num_sides;
}

/** MVC: View - take the model and show it **/

/* draw one sector */
static void sector_draw(struct game_state *state, const struct map_sector *sec)
{
	unsigned i;
	const struct sector_vertex *last = &sec->sides_xy[sec->num_sides - 1];
	GLfloat floor_height = sec->floor_height;
	GLfloat ceil_height = sec->ceil_height;
	const GLfloat colors[7][3] = {
		{ 1.0, 1.0, 1.0, },
		{ 0.0, 1.0, 1.0, },
		{ 1.0, 0.0, 1.0, },
		{ 1.0, 1.0, 0.0, },
		{ 1.0, 0.0, 0.0, },
		{ 0.0, 1.0, 0.0, },
		{ 0.0, 0.0, 1.0, },
	};

	for (i = 0; i < sec->num_sides; i++) {
		const struct sector_vertex *cur = &sec->sides_xy[i];
		// TODO: don't use immediate mode!
		glBegin(GL_TRIANGLE_STRIP);
		glColor3fv(colors[i]);
		/*
		glVertex3f(last->x, last->y, ceil_height);
		glVertex3f(cur->x, cur->y, ceil_height);
		glVertex3f(last->x, last->y, floor_height);
		glVertex3f(cur->x, cur->y, floor_height);
		*/
		glVertex3f(last->x, ceil_height, -last->y);
		glVertex3f(cur->x, ceil_height, -cur->y);
		glVertex3f(last->x, floor_height, -last->y);
		glVertex3f(cur->x, floor_height, -cur->y);
		glEnd();
		last = cur;
	}
}

/* TODO: draw all sectors visible to player's camera */
static void game_paint(void)
{
	SDL_Window *win = SDL_GL_GetCurrentWindow();
	struct game_state *state = SDL_GetWindowData(win, "game");
	SDL_assert(state != NULL);

	glScissor(state->win_x, state->win_y, state->win_w, state->win_h);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	/*
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);
	*/

	/* setup projection matrix */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	double nearest = 0.125; /* how close you can get before it's clipped. */
	glFrustum(nearest, -nearest, -nearest, nearest, nearest, 1000.0);

	/* setup world coordinates */
	glMatrixMode(GL_MODELVIEW);

#if 0
	/* Test code */
	glLoadIdentity();
	glBegin(GL_TRIANGLE_STRIP);
	glColor3f(0.0, 1.0, 0.0);
	glVertex3f(0.0625, 0.0, -1.0);
	glColor3f(1.0, 0.0, 0.0);
	glVertex3f(0.0, -0.0625, -1.0);
	glColor3f(0.0, 0.0, 1.0);
	glVertex3f(0.0, 0.0, -1.0);
	glEnd();
#endif

	glLoadIdentity();
	glRotatef(state->player_facing, 0.0, 1.0, 0.0);
	glTranslatef(-state->player_x, -state->player_height, state->player_y);
	sector_draw(state, &dummy_sector);
}

/** MVC: Controller - process inputs and alter the model over time. **/

/* process a key */
static void game_process_key(bool down, SDL_Keysym keysym, SDL_Window *win)
{
	struct game_state *state = SDL_GetWindowData(win, "game");
	SDL_assert(state != NULL);

	switch (keysym.sym) {
	case SDLK_e:
	case SDLK_UP:
		state->act.up = down;
		break;
	case SDLK_d:
	case SDLK_DOWN:
		state->act.down = down;
		break;
	case SDLK_w:
	case SDLK_LEFT:
		state->act.left = down;
		break;
	case SDLK_r:
	case SDLK_RIGHT:
		state->act.right = down;
		break;
	case SDLK_s:
		state->act.turn_left = down;
		break;
	case SDLK_f:
		state->act.turn_right = down;
		break;
	case SDLK_ESCAPE:
		keep_going = false;
		break;
	case SDLK_BACKQUOTE:
		if (!down)
			break;
		if (state->text_input)
			SDL_StopTextInput();
		else
			SDL_StartTextInput(); /* show on-screen keyboard */
		state->text_input ^= 1;
		SDL_Log("Text input is %s\n", state->text_input ? "on" : "off");
		break;
#if 0
	case SDLK_RETURN:
		/* if we're entering text, stop it  */
		if (state->text_input) {
			SDL_StopTextInput();
			state->text_input = false;
			// TODO: call some function to apply the buffer
		}
		break;
#endif
	}
}

static void game_process_ticks(SDL_Window *win)
{
	struct game_state *state = SDL_GetWindowData(win, "game");
	SDL_assert(state != NULL);

	Uint32 now = SDL_GetTicks();
	Sint32 elapsed = SDL_TICKS_PASSED(now, state->last_tick);
	SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION,
		"elapsed=%d ms\n", elapsed);

	double player_speed = 10.0; /* reciprocal speed: 10 ms per 1 unit */
	double player_turn_speed = 0.25;

	double theta = state->player_facing * M_PI / 180.0;
	double r = elapsed / player_speed;
	double ax = r * sin(theta);
	double ay = r * cos(theta);

	/*
	SDL_Log("key state: up=%d down=%d left=%d right=%d\n",
		state->act.up, state->act.down,
		state->act.left, state->act.right);
	*/
	// TODO: handle wall collision
	if (state->act.down) {
		state->player_y -= ay;
		state->player_x -= ax;
	}
	if (state->act.up) {
		state->player_y += ay;
		state->player_x += ax;
	}
	if (state->act.left) {
		state->player_y -= ax;
		state->player_x -= ay;
	}
	if (state->act.right) {
		state->player_y += ax;
		state->player_x += ay;
	}
	if (state->act.turn_left) {
		state->player_facing += elapsed / player_turn_speed;
	}
	if (state->act.turn_right) {
		state->player_facing -= elapsed / player_turn_speed;
	}

	/* put player_facing between 0 and 360. [0.0, 360.0) */
	state->player_facing = fmod(state->player_facing, 360.0);
	if (state->player_facing < 0.0)
		state->player_facing = 360.0 + state->player_facing;

	state->last_tick = now;
}

/** Initialization functions **/

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

	/* use OpenGL 2.1 - has all needed features and is widely supported */
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

	if (config.verbose) {
		SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
		warn("Using debugging logs.\n");
	} else {
		SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION,
			SDL_LOG_PRIORITY_WARN);
		warn("No debugging logs, use -verbose to see them.\n");
	}

	SDL_Log("For controls see http://use-esdf.org/\n");

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

	/* put us in the center of the map */
	sector_find_center(&dummy_sector,
		&main_state->player_x, &main_state->player_y);
	main_state->player_height = 1.0;

	/* Main event loop */
	main_state->last_tick = SDL_GetTicks();
	keep_going = true;
	while (keep_going) {
		/* Process every event */
		SDL_Event ev;
		/* wait no more than 15ms to avoid exceeding 67 fps */
		while (SDL_WaitEventTimeout(&ev, 15)) {
			switch (ev.type) {
			case SDL_QUIT:
				keep_going = false;
				break;
			case SDL_TEXTINPUT:
				// TODO: get main_state from windowID
				if (main_state->text_input) {
					// TODO: do something with this event
					SDL_Log("text input:\"%.32s\"\n",
						ev.text.text);
				}
				break;
			case SDL_KEYUP:
				game_process_key(false, ev.key.keysym,
					SDL_GetWindowFromID(ev.key.windowID));
				break;
			case SDL_KEYDOWN:
				game_process_key(true, ev.key.keysym,
					SDL_GetWindowFromID(ev.key.windowID));
				break;
			}
		}

		game_process_ticks(main_window);
		SDL_Log("player x=%g y=%g facing=%g\n",
			main_state->player_x, main_state->player_y,
			main_state->player_facing);
		/* Render/Paint */
		SDL_GL_MakeCurrent(main_window, main_context); /* not needed for single window applications */
		game_paint();
		SDL_GL_SwapWindow(main_window);
	}

	SDL_DestroyWindow(main_window);
	SDL_Quit();

	return 0;
}
