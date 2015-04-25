/*
 * Copyright © 2015 Jon Mayo <jon@rm-f.net>
 */
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>
#include "logging.h"
#include "model.h"
#include "modeldraw.h"

static int object_draw(struct object *obj, unsigned nr_vertex,
	GLfloat (*vertex)[3])
{
	/* select local or global vertex table */
	if (!obj->global_vertex) {
		debug("Using local vertex pool\n");
		nr_vertex = obj->nr_vertex;
		vertex = (GLfloat(*)[3])obj->vertex;
	} else {
		debug("Using global vertex pool\n");
	}

	int f;
	for (f = 0; f < obj->nr_face; f++) {
		glBegin(GL_TRIANGLES);
		unsigned a = obj->face[f][0];
		unsigned b = obj->face[f][1];
		unsigned c = obj->face[f][2];
		assert(a < nr_vertex && b < nr_vertex && c < nr_vertex);
		glVertex3fv(vertex[a]);
		glVertex3fv(vertex[b]);
		glVertex3fv(vertex[c]);
		glEnd();
	}

	return 0;
}

int model_draw(struct model *mdl)
{
	int i;
	for (i = 0; i < mdl->nr_object; i++) {
		object_draw(mdl->object + i, mdl->nr_vertex,
			(GLfloat(*)[3])mdl->vertex);
	}
	return 0;
}
