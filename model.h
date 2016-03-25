/* model.h : allocates and manipulates a model structure */
/* This software is PUBLIC DOMAIN as of January 2006. No copyright is claimed.
 * - Jon Mayo <jmayo@rm-f.net> */
#ifndef MODEL_H
#define MODEL_H

struct object {
	char *tag;
	int global_vertex;
	int has_normals;
	int nr_vertex;
	float (*vertex)[3];
	int nr_face;
	unsigned (*face)[3];
	struct {
		float min[3];
		float max[3];
	} bounding_box;
	/* TODO: material settings should be saved */
	/* TODO: texture coordinates should be saved */
	/* TODO: normals should be saved */
};

struct model {
	int nr_object;
	struct object *object;
	int nr_vertex;
	float (*vertex)[3];
	/* TODO: model_object_vertex_add() needs to update the bounding box */
	struct {
		float min[3];
		float max[3];
	} bounding_box;
};

struct model *model_create();
struct object *model_object_create(struct model *mdl, const char *tag, int use_global_vertex);
int model_vertex_add(struct model *mdl, float vertex0, float vertex1, float vertex2);
int model_object_vertex_add(struct object *o, float vertex0, float vertex1, float vertex2);
int model_object_face_add(struct object *o, unsigned face0, unsigned face1, unsigned face2);
void model_object_free(struct object *o);
int model_verify(struct model *m);
void model_free(struct model *m);
void model_dump(struct model *m);
/* TODO: model_strip() to remove unneeded features (normals, texture coordinates, materials) */
/* TODO: model_vertex_compact()
 * some file formats store the vertex array per-object, some
 * store them globally and every object uses the same vertex array.
 * supporting both then translating everything to the later form is
 * preferable */
#endif
