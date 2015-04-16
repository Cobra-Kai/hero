/* model.c : allocates and manipulates a model structure */
/* This software is PUBLIC DOMAIN as of January 2006. No copyright is claimed.
 * - Jon Mayo <jmayo@rm-f.net> */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "model.h"

struct model *model_create(void)
{
	struct model *mdl;
	mdl  =calloc(1, sizeof(*mdl));
	if (!mdl)
		return 0;
	mdl->bounding_box.min[0] = FLT_MAX;
	mdl->bounding_box.min[1] = FLT_MAX;
	mdl->bounding_box.min[2] = FLT_MAX;
	mdl->bounding_box.max[0] = FLT_MIN;
	mdl->bounding_box.max[1] = FLT_MIN;
	mdl->bounding_box.max[2] = FLT_MIN;

	return mdl;
}

static inline float maxf(float a, float b)
{
	return a > b ? a : b;
}

static inline float minf(float a, float b)
{
	return a < b ? a : b;
}

struct object *model_object_create(struct model *mdl, const char *tag,
	int use_global_vertex)
{
	struct object *tmp;
	tmp = realloc(mdl->object, sizeof *mdl->object * (mdl->nr_object + 1));
	if (!tmp)
		return 0;
	mdl->object = tmp;
	tmp = mdl->object + mdl->nr_object++;
	tmp->tag = tag ? strdup(tag) : 0;
	tmp->face = 0;
	tmp->nr_face = 0;
	tmp->vertex = 0;
	tmp->nr_vertex = 0;
	tmp->global_vertex = use_global_vertex;
	tmp->bounding_box.min[0] = tmp->bounding_box.min[1] = tmp->bounding_box.min[2] = FLT_MAX;
	tmp->bounding_box.max[0] = tmp->bounding_box.max[1] = tmp->bounding_box.max[2] = FLT_MIN;
	return tmp;
}

/* return the vertex number */
int model_vertex_add(struct model *mdl, float vertex0, float vertex1, float vertex2)
{
	int ret;
	float (*tmp)[3];
	tmp = realloc(mdl->vertex, (mdl->nr_vertex + 1) * sizeof *mdl->vertex);
	if (!tmp)
		return -1;

	mdl->vertex = tmp;
	ret = mdl->nr_vertex++;
	tmp = &mdl->vertex[ret];
	(*tmp)[0] = vertex0;
	(*tmp)[1] = vertex1;
	(*tmp)[2] = vertex2;

	/* we assume if a vertex is on the list that it is used */
	mdl->bounding_box.min[0] = maxf(vertex0, mdl->bounding_box.min[0]);
	mdl->bounding_box.min[1] = maxf(vertex1, mdl->bounding_box.min[1]);
	mdl->bounding_box.min[2] = maxf(vertex2, mdl->bounding_box.min[2]);
	mdl->bounding_box.max[0] = minf(vertex0, mdl->bounding_box.max[0]);
	mdl->bounding_box.max[1] = minf(vertex1, mdl->bounding_box.max[1]);
	mdl->bounding_box.max[2] = minf(vertex2, mdl->bounding_box.max[2]);

	return ret;
}

int model_object_vertex_add(struct object *o, float vertex0, float vertex1, float vertex2)
{
	float (*tmp)[3];
	if (o->global_vertex) {
		fprintf(stderr, "ERROR: model_object_vertex_add() called when global_vertex = %d\n", o->global_vertex);
		return 0;
	}
	tmp = realloc(o->vertex, (o->nr_vertex + 1) * sizeof *o->vertex);
	if (!tmp)
		return 0;

	o->vertex = tmp;
	tmp = &o->vertex[o->nr_vertex++];
	(*tmp)[0] = vertex0;
	(*tmp)[1] = vertex1;
	(*tmp)[2] = vertex2;

	/* we assume if a vertex is on the list that it is used */
	o->bounding_box.min[0] = maxf(vertex0, o->bounding_box.min[0]);
	o->bounding_box.min[1] = maxf(vertex1, o->bounding_box.min[1]);
	o->bounding_box.min[2] = maxf(vertex2, o->bounding_box.min[2]);
	o->bounding_box.max[0] = minf(vertex0, o->bounding_box.max[0]);
	o->bounding_box.max[1] = minf(vertex1, o->bounding_box.max[1]);
	o->bounding_box.max[2] = minf(vertex2, o->bounding_box.max[2]);

	/* TODO: update the model bounding box too */
#if 0
	mdl->bounding_box.min[0] = maxf(vertex0, mdl->bounding_box.min[0]);
	mdl->bounding_box.min[1] = maxf(vertex1, mdl->bounding_box.min[1]);
	mdl->bounding_box.min[2] = maxf(vertex2, mdl->bounding_box.min[2]);
	mdl->bounding_box.max[0] = minf(vertex0, mdl->bounding_box.max[0]);
	mdl->bounding_box.max[1] = minf(vertex1, mdl->bounding_box.max[1]);
	mdl->bounding_box.max[2] = minf(vertex2, mdl->bounding_box.max[2]);
#endif
	return 1;
}

int model_object_face_add(struct object *o, unsigned face0, unsigned face1, unsigned face2)
{
	unsigned (*tmp)[3];
	assert(o != NULL);
	/* TODO: check face0,face1,face2 for proper nr_vertex range */
	tmp = realloc(o->face, (o->nr_face + 1) * sizeof *o->face);
	if (!tmp)
		return 0;

	o->face = tmp;
	tmp = &o->face[o->nr_face++];
	(*tmp)[0] = face0;
	(*tmp)[1] = face1;
	(*tmp)[2] = face2;

	return 1;
}

void model_object_free(struct object *o)
{
	if (!o) return;
	free(o->tag);
	free(o->face);
	free(o->vertex);
	o->face = 0;
	o->nr_face = 0;
	o->vertex = 0;
	o->nr_vertex = 0;
}

/* verify that the model object makes sense */
int model_verify(struct model *m)
{
	struct object *o;
	int i,j;
	unsigned nr_vertex;

	for (i = 0; i < m->nr_object; i++) {
		o = m->object + i;
		if (o->global_vertex) {
			nr_vertex = m->nr_vertex;
		} else {
			nr_vertex = o->nr_vertex;
		}
		for (j = 0; j < o->nr_face; j++) {
			if (o->face[i][0] >= nr_vertex || o->face[i][1] >= nr_vertex || o->face[i][2] >= nr_vertex)
				return 0; /* failure */
		}
	}
	return 1;
}

void model_free(struct model *m)
{
	int i;
	if (!m) return;
	for (i = 0; i < m->nr_object; i++)
		model_object_free(m->object + i);
	free(m->object);
	m->object = 0;
	free(m);
}

void model_dump(struct model *m)
{
	struct object *o;
	int i, j, k;
	for (i = 0; i < m->nr_object; i++) {
		int nr_vertex;
		float (*vertex)[3];

		o = m->object + i;
		if (o->global_vertex) {
			nr_vertex = m->nr_vertex;
			vertex = m->vertex;
		} else {
			nr_vertex = o->nr_vertex;
			vertex = o->vertex;
		}
		printf("[%s]\n", o->tag ? o->tag : "noname");
		printf("  faces = %d vertices = %d\n", o->nr_face, nr_vertex);
		for (j = 0; j < nr_vertex; j++)
			printf("  vertex[%u] = { %f %f %f }\n", j, vertex[j][0], vertex[j][1], vertex[j][2]);
		for (j = 0; j < o->nr_face; j++) {
			printf("  face[%u] = { %u %u %u } /* ", j, o->face[j][0], o->face[j][1], o->face[j][2]);
			for (k = 0; k < 3; k++) {
				int vi = o->face[j][k];
				if (vi < nr_vertex) {
					printf(" (%f, %f, %f)", vertex[vi][0], vertex[vi][1] , vertex[vi][2]);
				} else {
					printf(" (Undefined)");
				}
			}
			printf("\n");
		}
	}
}
