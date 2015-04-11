/*
 * Copyright © 2015 Jon Mayo <jon@rm-f.net>
 */
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>
#include "logging.h"
#include "texture.h"

/** Texture loading routines **/
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/* load an image from a file, allocate a new texture, copy image into texture.
 * pass level=-1 to generate mipmaps, else LOD=level and must load mipmaps manually.
 * return width and height to info on the aspect ratio for NPOT textures.
 * Operates on the currently bound texture, example:
	glGenTextures(tex_max, tex_ids);
	for (i = 0; i < tex_max; i++) {
		glBindTexture(GL_TEXTURE_2D, tex_ids[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		e = texture_load(filename, -1, GL_RGBA, &width, &height, 0, true);
		assert(e == 0);
	}
 */
int texture_load(const char *filename, GLint level, GLint internalFormat, int *width, int *height, GLint border, bool use_alpha)
{
	int w, h, comps;
	unsigned char *data = stbi_load(filename, &w, &h, &comps, use_alpha ? 4 : 3);
	if (!data) {
		warn("%s:error loading:%s\n", filename, stbi_failure_reason());
		return -1;
	}

	verbose("%s:%d:loaded %dx%d\n", filename, level, w, h);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	// TODO: What to do with not-power-of-2 textures?
	if (level >= 0) {
		glTexImage2D(GL_TEXTURE_2D, level, internalFormat, w, h, border,
			use_alpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, data);
	} else {
		gluBuild2DMipmaps(GL_TEXTURE_2D, internalFormat, w, h,
			use_alpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, data);
	}
	stbi_image_free(data);

	if (width)
		*width = w;
	if (height)
		*height = h;

	return 0;
}
