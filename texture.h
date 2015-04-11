/*
 * Copyright © 2015 Jon Mayo <jon@rm-f.net>
 */
#ifndef TEXTURE_H
#define TEXTURE_H
int texture_load(const char *filename, GLint level, GLint internalFormat,
	int *width, int *height, GLint border, bool use_alpha);
#endif
