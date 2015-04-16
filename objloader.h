/* objloader.h : a library to load a subset of the Wavefront OBJ model format.
 *
 * Created by Jon Mayo <jon@rm-f.net> on 7/26/06.
 * This software is PUBLIC DOMAIN as of July 2006. No copyright is claimed.
 *
 * Updated 4/14/15 jdm:
 *	updated coding style and renamed everything.
 *	fixed bug in obj_save.
 */

#ifndef OBJLOADER_H
#define OBJLOADER_H
#include "model.h"
struct model *obj_load(const char *filename);
struct model *obj_load_from_file(FILE *f, const char *filename);
int obj_save(const char *filename, struct model *m);
#endif
