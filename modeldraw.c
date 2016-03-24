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

static void cross_product(GLfloat c[3], GLfloat a[3], GLfloat b[3])
{
	c[0] = a[1] * b[2] - a[2] * b[1];
	c[1] = a[2] * b[0] - a[1] * b[2];
	c[2] = a[0] * b[1] - a[1] * b[0];
}

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
	glEnable(GL_NORMALIZE);
	/* calculate normals if none are present in the model */
	unsigned has_normals = obj->has_normals;
	for (f = 0; f < obj->nr_face; f++) {
		glBegin(GL_TRIANGLES);
		unsigned a = obj->face[f][0];
		unsigned b = obj->face[f][1];
		unsigned c = obj->face[f][2];
		assert(a < nr_vertex && b < nr_vertex && c < nr_vertex);
		GLfloat normal[3];
		if (!has_normals) {
			/* the winding is swapped every other triangle */
			if (f % 2) {
				cross_product(normal, vertex[b], vertex[c]);
			} else {
				cross_product(normal, vertex[c], vertex[b]);
			}
		}
		if (!has_normals)
			glNormal3fv(normal);
		glVertex3fv(vertex[a]);
		if (!has_normals)
			glNormal3fv(normal);
		glVertex3fv(vertex[b]);
		if (!has_normals)
			glNormal3fv(normal);
		glVertex3fv(vertex[c]);
		glEnd();
	}
	glDisable(GL_NORMALIZE);

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
