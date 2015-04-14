/*
 * Copyright © 2015 Jon Mayo <jon@rm-f.net>
 */
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <strings.h> /* for ffs() */
#define GL_GLEXT_PROTOTYPES
#include <SDL.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>
#include "logging.h"
#include "texture.h"

#define ARRAY_SIZE(a) (sizeof (a) / sizeof *(a))

struct world *world;

struct config {
	int width, height;
	bool verbose; /* enable to turn on all logging */
	bool debug; /* enable to turn on debug logging */
	bool use_vsync;
};

struct game_state {
	GLuint win_x, win_y, win_w, win_h; /* used to crop to preserve aspect */
	GLdouble player_x, player_y; /* current player position */
	GLdouble player_z; /* non-zero if we're flying, added to height */
	GLdouble player_facing, player_height;
	GLdouble player_tilt; /* for look up/down */
	unsigned player_sector;
	Uint32 last_tick;
	bool text_input;
	/** player input **/
	struct act {
		bool up, down, left, right;
		bool turn_left, turn_right;
		bool fly_up, fly_down;
		bool look_up, look_down;
	} act; /* actions */
	SDL_GameController *gamepad;
};

/* global configuration */
static struct config config = {
	.width = 960, .height = 540,
	.verbose = false,
	.debug = false,
	.use_vsync = false,
};

static bool keep_going = true;

#if 0 /* some debug code */

static inline void *My_GetWindowData(SDL_Window *win, const char *tag)
{
	debug("%s():tag=%s\n", __func__, tag);
	return SDL_GetWindowData(win, tag);
}

static inline void *My_SetWindowData(SDL_Window *win, const char *tag, void *p)
{
	debug("%s():tag=%s ptr=%p\n", __func__, tag, p);
	return SDL_SetWindowData(win, tag, p);
}

#define SDL_GetWindowData My_GetWindowData
#define SDL_SetWindowData My_SetWindowData
#endif

/** GL setup & initialization **/

static void setup_gl(void)
{
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
	unsigned sector_number;
	GLfloat floor_height, ceil_height;
	unsigned num_sides;
	/* must be in counter-clockwise order */
	struct sector_vertex sides_xy[MAX_SIDES];
	unsigned char color[MAX_SIDES];
	unsigned short destination_sector[MAX_SIDES]; /* portal != 0xffff */
};

#define SECTOR_NONE (0xffff)

const struct map_sector *sector_get(unsigned num)
{
	static const struct map_sector dummy_sector0 = {
		.sector_number = 0,
		.floor_height = 0.0,
		.ceil_height = 2.0,
		.num_sides = 4,
		.sides_xy = {
			{ 1, 16, },
			{ 5, 11, },
			{ 5, 7, },
			{ 1, 2, },
		},
		.destination_sector =
			{ SECTOR_NONE, SECTOR_NONE, SECTOR_NONE, 1, },
		.color = { 0, 1, 2, 3 },
	};
	static const struct map_sector dummy_sector1 = {
		.sector_number = 1,
		.floor_height = 0.0,
		.ceil_height = 2.0,
		.num_sides = 4,
		.sides_xy = {
			{ 8, 6, },
			{ 10, 1, },
			{ 1, 2, },
			{ 5, 7, },
		},
		.destination_sector =
			{ SECTOR_NONE, SECTOR_NONE, SECTOR_NONE, 0, },
		.color = { 4, 5, 6, 7 },
	};
	switch (num) {
	case 0:
		return &dummy_sector0;
	case 1:
		return &dummy_sector1;
	}
	return NULL;
}

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

/* compiled world sector */
struct wsector {
	GLuint display_list;
};

struct world {
	unsigned num_textures;
	GLuint *tex_ids;
	struct wsector *sectors;
	unsigned num_sectors;
	unsigned max_sectors; /* allocated sectors */
};

struct world *world_new(void)
{
	// TODO: don't hard code these filenames
	const char *texfiles[] = {
		"assets/461223101.jpg",
		"assets/461223102.jpg",
		"assets/461223103.jpg",
		"assets/461223104.jpg",
		"assets/461223105.jpg",
	};
	const char tex_max = ARRAY_SIZE(texfiles);

	struct world *world = calloc(1, sizeof(*world));
	world->num_textures = tex_max;
	world->tex_ids = calloc(tex_max, sizeof(*world->tex_ids));

	glGenTextures(tex_max, world->tex_ids);
	debug("%s():%d:error=%#x\n", __func__, __LINE__, glGetError());
	int i;
	for (i = 0; i < tex_max; i++) {
		debug("Binding texture %d\n", world->tex_ids[i]);
		glBindTexture(GL_TEXTURE_2D, world->tex_ids[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		int width, height;
		int e = texture_load(texfiles[i], -1, GL_RGBA, &width, &height, 0, false);
		assert(e == 0);
		verbose("%s:texture=%dx%d\n", texfiles[i], width, height);
	}

	return world;
}

/* draw one sector
 * set ttl=0 to only draw this sector */
static void sector_gen_visible(const struct map_sector *sec, int ttl)
{
	/* limit our depth */
	if (ttl < 0)
		return;
	if (!sec)
		return; /* TODO: maybe draw some empty void? */
	unsigned i;
	const struct sector_vertex *last = &sec->sides_xy[sec->num_sides - 1];
	GLfloat floor_height = sec->floor_height;
	GLfloat ceil_height = sec->ceil_height;
#if 0 /* use colors */
	const GLfloat colors[][3] = {
		{ 1.0, 1.0, 1.0, },
		{ 0.0, 1.0, 1.0, },
		{ 1.0, 0.0, 1.0, },
		{ 1.0, 1.0, 0.0, },
		{ 1.0, 0.0, 0.0, },
		{ 0.0, 1.0, 0.0, },
		{ 0.0, 0.0, 1.0, },

		{ 0.5, 0.5, 0.5, },
		{ 0.0, 0.5, 0.5, },
		{ 0.5, 0.0, 0.5, },
		{ 0.5, 0.5, 0.0, },
		{ 0.5, 0.0, 0.0, },
		{ 0.0, 0.5, 0.0, },
		{ 0.0, 0.0, 0.5, },
	};
#endif

	for (i = 0; i < sec->num_sides; i++) {
		const struct sector_vertex *cur = &sec->sides_xy[i];
		unsigned short destination_sector = sec->destination_sector[i];
		if (destination_sector == SECTOR_NONE) {
			// TODO: don't use immediate mode!
#if 1 /* use textures */
			glBindTexture(GL_TEXTURE_2D, world->tex_ids[i % world->num_textures]);
			glEnable(GL_TEXTURE_2D);
			glBegin(GL_TRIANGLE_STRIP);
#endif
#if 0 /* use colors */
			glColor3fv(colors[sec->color[i] % ARRAY_SIZE(colors)]);
#endif
			/* find the length of the wall */
			GLfloat x = last->x - cur->x;
			GLfloat y = last->y - cur->y;
			GLfloat length = sqrt(x*x + y*y);
			/* draw simple repeating texture coordinates for a wall texture */
			glTexCoord2f(length, ceil_height);
			glVertex3f(last->x, ceil_height, -last->y);
			glTexCoord2f(0.0, ceil_height);
			glVertex3f(cur->x, ceil_height, -cur->y);
			glTexCoord2f(length, floor_height);
			glVertex3f(last->x, floor_height, -last->y);
			glTexCoord2f(0.0, floor_height);
			glVertex3f(cur->x, floor_height, -cur->y);
			glEnd();
			glBindTexture(GL_TEXTURE_2D, 0);
			glDisable(GL_TEXTURE_2D);
		} else if (ttl > 0) { /* only recurse if ttl > 0 */
			// debug("portal %d = %hu\n", i, destination_sector);
			const struct map_sector *newsec =
				sector_get(destination_sector);
			/* let the depth buffer mask off the room */
			// TODO: optimize with a scissor test of the wall's bbox
			// TODO: add additional modelview matrix
			/* recurse into the portal */
			sector_gen_visible(newsec, ttl - 1);
		}
		last = cur;
	}
}

/* add sector to world */
static int world_sector_add(struct world *world, unsigned n,
	const struct map_sector *sec)
{
	if (n > INT_MAX / 2)
		return -1; /* the formula below can't handle large n */
	if (n >= world->max_sectors) {
		unsigned newmax = 1U << (ffs(n) + 1);
		struct wsector *newsec = realloc(world->sectors,
			sizeof(*newsec) * newmax);
		if (!newsec)
			return -1;
		/* clear from old max to new max */
		memset(newsec + world->max_sectors, 0,
			sizeof(*newsec) * (newmax - world->max_sectors));
		/* done, we can now save the entries */
		world->sectors = newsec;
		world->max_sectors = newmax;
	}

	// TODO: write the slot
	struct wsector *wsec = &world->sectors[n];
	wsec->display_list = glGenLists(1);
	glNewList(wsec->display_list, GL_COMPILE);
	sector_gen_visible(sec, 0);
	glEndList();

	return 0; /* success */
}

/** MVC: View - take the model and show it **/

/* draw one sector */
static void sector_draw(struct game_state *state, const struct map_sector *sec, int ttl)
{
	unsigned i;
	/* limit our depth */
	if (ttl < 0)
		return;
	if (!sec)
		return; /* TODO: maybe draw some empty void? */
	/* draw the entire room */
	glCallList(world->sectors[sec->sector_number].display_list);
	/* find any portals for this room and draw them */
	for (i = 0; i < sec->num_sides; i++) {
		unsigned short destination_sector = sec->destination_sector[i];

		/* only recurse if ttl > 0 */
		if (destination_sector != SECTOR_NONE && ttl > 0) {
			const struct map_sector *newsec =
				sector_get(destination_sector);
			sector_draw(state, newsec, ttl - 1);
		}
	}
}

static void sector_print(const struct map_sector *sec)
{
	unsigned i;

	// TODO: print more information
	debug("dest:");
	for (i = 0; i < sec->num_sides; i++) {
		debug(" %hx", sec->destination_sector[i]);
	}
	debug("\n");
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
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);

	/* setup projection matrix */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	double nearest = 0.125; /* how close you can get before it's clipped. */
	glFrustum(nearest, -nearest, -nearest, nearest, nearest, 1000.0);

	/* setup world coordinates */
	glMatrixMode(GL_MODELVIEW);

	glLoadIdentity();
	glRotatef(state->player_tilt, -1.0, 0.0, 0.0);
	glRotatef(state->player_facing, 0.0, 1.0, 0.0);
	glTranslatef(-state->player_x, -state->player_height - state->player_z,
		state->player_y);
	/* draw up to 10 sectors deep */
	sector_draw(state, sector_get(state->player_sector), 10);
}

/** MVC: Controller - process inputs and alter the model over time. **/

/* process a key */
static void game_process_key(bool down, SDL_Keysym keysym, SDL_Window *win)
{
	struct game_state *state = SDL_GetWindowData(win, "game");
	SDL_assert(state != NULL);

	switch (keysym.sym) {
	case SDLK_HOME: /* Fly Up */
		state->act.fly_up = down;
		break;
	case SDLK_END: /* Fly Down */
		state->act.fly_down = down;
		break;
	case SDLK_PAGEUP: /* Look Up */
		state->act.look_up = down;
		break;
	case SDLK_PAGEDOWN: /* Look Down */
		state->act.look_down = down;
		break;
	case SDLK_UP:
	case SDLK_e:
		state->act.up = down;
		break;
	case SDLK_DOWN:
	case SDLK_d:
		state->act.down = down;
		break;
	case SDLK_w:
		state->act.left = down;
		break;
	case SDLK_r:
		state->act.right = down;
		break;
	case SDLK_LEFT:
	case SDLK_s:
		state->act.turn_left = down;
		break;
	case SDLK_RIGHT:
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

/* process input from gamepad/joystick */
static void game_process_gamepad_button(bool down,
	SDL_ControllerButtonEvent *cbutton, SDL_Window *win)
{
	SDL_assert(cbutton != NULL);
	SDL_assert(win != NULL);
	if (!cbutton || !win)
		return;
	struct game_state *state = SDL_GetWindowData(win, "game");
	SDL_assert(state != NULL);
	switch ((SDL_GameControllerButton)cbutton->button) {
	case SDL_CONTROLLER_BUTTON_DPAD_UP:
		state->act.up = down;
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
		state->act.down = down;
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
		state->act.left = down;
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
		state->act.right = down;
		break;
	case SDL_CONTROLLER_BUTTON_Y:
		state->act.look_up = down;
		break;
	case SDL_CONTROLLER_BUTTON_A:
		state->act.look_down = down;
		break;
	case SDL_CONTROLLER_BUTTON_X:
		state->act.fly_up = down;
		break;
	case SDL_CONTROLLER_BUTTON_B:
		state->act.fly_down = down;
		break;
	case SDL_CONTROLLER_BUTTON_BACK:
	case SDL_CONTROLLER_BUTTON_GUIDE:
	case SDL_CONTROLLER_BUTTON_START:
	case SDL_CONTROLLER_BUTTON_LEFTSTICK:
	case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
	case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
	case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
		/* not mapped to any function */
		break;
	case SDL_CONTROLLER_BUTTON_INVALID:
	case SDL_CONTROLLER_BUTTON_MAX:
		/* not a valid button - put here to suppress warnings */
		break;
	}
}

/* TODO: turn inputs into a vector instead of treating it a digital input. */
static void game_process_gamepad_axis(SDL_ControllerAxisEvent *caxis,
	SDL_Window *win)
{
	SDL_assert(caxis != NULL);
	SDL_assert(win != NULL);
	if (!caxis || !win)
		return;
	struct game_state *state = SDL_GetWindowData(win, "game");
	SDL_assert(state != NULL);
	switch ((SDL_GameControllerAxis)caxis->axis) {
	case SDL_CONTROLLER_AXIS_LEFTX:
		if (caxis->value < -10) {
			state->act.turn_left = true;
			state->act.turn_right = false;
		} else if (caxis->value > 10) {
			state->act.turn_left = false;
			state->act.turn_right = true;
		} else {
			state->act.turn_left = false;
			state->act.turn_right = false;
		}
		break;
	case SDL_CONTROLLER_AXIS_LEFTY:
		if (caxis->value < -10) {
			state->act.up = true;
			state->act.down = false;
		} else if (caxis->value > 10) {
			state->act.up = false;
			state->act.down = true;
		} else {
			state->act.up = false;
			state->act.down = false;
		}
		break;
	case SDL_CONTROLLER_AXIS_RIGHTX:
	case SDL_CONTROLLER_AXIS_RIGHTY:
	case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
	case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
		/* not mapped to any function */
		break;
	case SDL_CONTROLLER_AXIS_INVALID:
	case SDL_CONTROLLER_AXIS_MAX:
		/* not a valid axis - put here to suppress warnings */
		break;
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
		state->player_x -= ax;
		state->player_y -= ay;
	}
	if (state->act.up) {
		state->player_x += ax;
		state->player_y += ay;
	}
	if (state->act.left) {
		double ax_left = r * sin(theta + M_PI / 2);
		double ay_left = r * cos(theta + M_PI / 2);
		state->player_x += ax_left;
		state->player_y += ay_left;
	}
	if (state->act.right) {
		double ax_right = r * sin(theta - M_PI / 2);
		double ay_right = r * cos(theta - M_PI / 2);
		state->player_x += ax_right;
		state->player_y += ay_right;
	}
	if (state->act.turn_left) {
		state->player_facing += elapsed / player_turn_speed;
	}
	if (state->act.turn_right) {
		state->player_facing -= elapsed / player_turn_speed;
	}
	if (state->act.fly_up) {
		state->player_z += r;
	}
	if (state->act.fly_down) {
		state->player_z -= r;
	}
	if (state->act.look_up) {
		state->player_tilt += elapsed / player_turn_speed;
	}
	if (state->act.look_down) {
		state->player_tilt -= elapsed / player_turn_speed;
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
		} else if (!strcmp(cur, "-debug")) {
			config.debug = true;
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

/* Keyboard controls:
 *
 * Move_Forward		UP	E
 * Move_Backward	DOWN	D
 * Turn_Left		LEFT	S
 * Turn_Right		RIGHT	F
 * Strafe_Left			W
 * Strafe_Right			R
 * Look_Up		PGUP
 * Look_Down		PGDN
 * Fly_Up		HOME
 * Fly_Down		END
 */
int main(int argc, char **argv)
{
	parse_args(argc, argv);

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
		die("SDL Could not initialize! (%s)\n", SDL_GetError());

	/* use OpenGL 2.1 - has all needed features and is widely supported */
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

	if (config.verbose) {
		SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
		warn("Using verbose logs.\n");
	} else if (config.debug) {
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

	/* Configure the player gamepad */
	if (SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt") == -1)
		warn("gamecontrollerdb.txt:%s\n", SDL_GetError());
	if (SDL_NumJoysticks() > 0) {
		main_state->gamepad = SDL_GameControllerOpen(0);
	}

	if (!main_state->gamepad) {
		warn("No joysticks found\n");
	} else {
		SDL_Log("controller: %s\n",
			SDL_GameControllerName(main_state->gamepad));
	}

	/** GL related setup **/
	SDL_GLContext main_context = SDL_GL_CreateContext(main_window);
	if (!main_context)
		die("SDL could not create GL context! (%s)\n", SDL_GetError());
	setup_gl();

	/* establish a world */
	world = world_new();
	assert(world != NULL);

	// TODO: replace this with some kind of map loading routine
	world_sector_add(world, 0, sector_get(0));
	world_sector_add(world, 1, sector_get(1));

	/* put us in the center of the map of the 1st sector */
	main_state->player_sector = 1;
	sector_find_center(sector_get(main_state->player_sector),
		&main_state->player_x, &main_state->player_y);
	main_state->player_height = 1.0;

	/* print the starting sector */
	sector_print(sector_get(main_state->player_sector));

	/* Main event loop */
	SDL_GameControllerEventState(SDL_ENABLE);
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
			case SDL_CONTROLLERBUTTONDOWN:
				game_process_gamepad_button(true, &ev.cbutton,
					main_window); // TODO: look up window
				break;
			case SDL_CONTROLLERBUTTONUP:
				game_process_gamepad_button(false, &ev.cbutton,
					main_window); // TODO: look up window
				break;
			case SDL_CONTROLLERAXISMOTION:
				game_process_gamepad_axis(&ev.caxis,
					main_window); // TODO: look up window
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

	SDL_GameControllerClose(main_state->gamepad);
	main_state->gamepad = NULL;

	SDL_DestroyWindow(main_window);
	SDL_Quit();

	return 0;
}
