/* objloader.c : a library to load a subset of the Wavefront OBJ model format.
 *
 * Created by Jon Mayo <jon@rm-f.net> on 7/26/06.
 * This software is PUBLIC DOMAIN as of July 2006. No copyright is claimed.
 *
 * Updated 4/14/15 jdm:
 *	updated coding style and renamed everything.
 *	fixed bug in obj_save.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include "objloader.h"
#include "model.h"
#include "logging.h"

#define MAX_LINE_LEN	1024
#define MAX_TESS	6	/* biggest polygon we will tessellate */

static int parse_face_data(const char *filename, unsigned line,
	struct model *m, struct object **curr_grp, char *buf)
{
	char *tmp, *idx = buf;
	unsigned i, j;
	int data[3][MAX_TESS];
	if (!*curr_grp) {
		debug("%s:%u:face data before group name\n", filename, line);
		*curr_grp = model_object_create(m, "ungrouped", 1);
		if (!*curr_grp) {
			debug("%s:%u:face data allocation error\n",
				filename, line);
			return 0;
		}
	}

	/*
	for(i = 0; i < MAX_TESS; i++) {
		for(j = 0; j < 3; j++) {
			data[j][i]=-1;
		}
	}
	*/
	/* "v" - vertex index */
	/* "vt" - texture index */
	/* "vn" - normal index */

	/* TODO: error if there are too many sides to the polygon face */
	for (i = 0; i < MAX_TESS; i++) {
		unsigned n;

		while (isspace(*idx))
			idx++;
		if (!*idx) {
			verbose("break[%u] %s:%u\n", i, filename, line);
			break;
		}
		for (j = 0; j < 3; j++) {
			if (*idx == '/') {
				/* ignore field */
				data[j][i] = -1;
				idx++;
				continue;
			}
			if (isspace(*idx))
				break;
			n = strtol(idx, &tmp, 10);
			if (idx == tmp) {
				warn("%s:%u:face data corrupt.\n", filename, line);
				verbose("IDX='%s'\n", idx);
				return 0;
			}
			data[j][i] = n;
			idx = tmp;
			if (*idx != '/') {
				idx++;
				break;
			}
			idx++;
		}
	}
	debug("does this match? %d/%d/%d %d/%d/%d %d/%d/%d\n",
		data[0][0], data[1][0], data[2][0],
		data[0][1], data[1][1], data[2][1],
		data[0][2], data[1][2], data[2][2]);
	debug("\t%s\n", buf);
	if (data[0][0] == -1 || data[0][1] == -1 || data[0][2] == -1) {
		warn("%s:%u:Vertex field not supplied %d/%d/%d\n",
			filename, line, data[0][0], data[0][1], data[0][2]);
		debug("buf='%s'\n", buf);
		return 0;
	}

	debug("buf='%s'\n", buf);
	if (i == 3) {
		if (!model_object_face_add(*curr_grp,
			data[0][0] - 1, data[0][1] - 1, data[0][2] - 1)) {
			return 0;
		}
		debug("data='%u %u %u'\n", data[0][0], data[0][1], data[0][2]);
	} else if (i == 4) {
		if (!model_object_face_add(*curr_grp,
			data[0][0] - 1, data[0][1] - 1, data[0][2] - 1)) {
			return 0;
		}
		debug("data='%u %u %u'\n", data[0][0], data[0][1], data[0][2]);
		if (!model_object_face_add(*curr_grp,
			data[0][2] - 1, data[0][3] - 1, data[0][0] - 1)) {
			return 0;
		}
		debug("data='%u %u %u'\n", data[0][2], data[0][3], data[0][0]);
	} else {
		warn("Tessellation needed: sides = %u\n", i);
	}

	for (; i < MAX_TESS; i++) {
		for (j = 0; j < 3; j++) {
			data[j][i] = -1;
		}
	}

	if (data[1][0] == -1 || data[1][1] == -1 || data[1][2] == -1)
		warn("%s:%u:Texture field not supplied\n", filename, line);
	if (data[2][0] == -1 || data[2][1] == -1 || data[2][2] == -1)
		warn("%s:%u:Normal field not supplied\n", filename, line);
	return 1;
}


struct model *obj_load_from_file(FILE *f, const char *filename)
{
	struct model *m;
	struct object *curr_grp = 0;
	char buf[MAX_LINE_LEN];
	char *idx, *cmd, *tmp;
	unsigned line = 0;
	double v[3];
	unsigned i;
	m = model_create();
	while (fgets(buf, sizeof buf, f)) {
		line++;
		idx = buf;
		while (isspace(*idx))
			idx++;
		if (*idx == '#' || !*idx)
			continue; /* comment or blank line */
		cmd = idx;
		while (*idx && !isspace(*idx))
			idx++;
		if (*idx) {
			*idx = 0;
			idx++;
		}
		if (!strcmp(cmd, "v")) { /* geometry vertex */
			for (i = 0; i < 3; i++) {
				while (isspace(*idx))
					idx++;
				v[i] = strtod(idx, &tmp);
				if (idx == tmp) /* failed */
					goto error;
				idx = tmp;
			}
			/*
			verbose("match? \"%s\" [%f,%f,%f]\n",
				buf+2, v[0], v[1], v[2]);
			*/
			if (model_vertex_add(m, v[0], v[1], v[2]) < 0) {
				warn("%s:%u:vertex data error\n",
					filename, line);
				goto error;
			}
		} else if (!strcmp(cmd, "vt")) { /* vertex texture */
			for (i = 0; i < 2; i++) {
				while (isspace(*idx))
					idx++;
				v[i] = strtod(idx, &tmp);
				if (idx == tmp) { /* failed */
					goto error;
				}
				idx = tmp;
			}
			debug("%s:%u:ignoring texture coord [U:%f, V:%f]\n",
				filename, line, v[0], v[1]);
		} else if (!strcmp(cmd, "vn")) { /* normal vector */
			for (i = 0; i < 3; i++) {
				while (isspace(*idx))
					idx++;
				v[i] = strtod(idx, &tmp);
				if (idx == tmp) { /* failed */
					goto error;
				}
				idx = tmp;
			}
			debug("%s:%u:ignoring normal [%f, %f, %f]\n",
				filename, line, v[0], v[1], v[2]);
		} else if (!strcmp(cmd, "p")) { /* polygon */

		} else if (!strcmp(cmd, "l")) { /* line */

		} else if (!strcmp(cmd, "f")) { /* face */
			if (!parse_face_data(filename, line, m, &curr_grp, idx))
				goto error;
		} else if (!strcmp(cmd, "g")) { /* set group name */
			tmp = idx;
			while (*idx && *idx != '\r' && *idx != '\n')
				idx++;
			if (*idx)
				*idx = 0;
			curr_grp = model_object_create(m, tmp, 1);
			debug("curr_grp=%p\n", curr_grp);
		} else if (!strcmp(cmd, "o")) { /* object name */
			tmp = idx;
			while (*idx && *idx != '\r' && *idx != '\n')
				idx++;
			if (*idx)
				*idx = 0;
			debug("%s:%u:ignoring object name '%s'\n",
				filename, line, tmp);
#if 0
		} else if (!strcmp(cmd, "vp")) { /* point in the parameter space of a curve */

		} else if (!strcmp(cmd, "mg")) { /* merging group and merge resolution */

		} else if (!strcmp(cmd, "s")) { /* smoothing group */

		} else if (!strcmp(cmd, "cstype")) {

		} else if (!strcmp(cmd, "deg")) {

		} else if (!strcmp(cmd, "step")) {

		} else if (!strcmp(cmd, "bmat")) {

		} else if (!strcmp(cmd, "lod")) {

		} else if (!strcmp(cmd, "usemap")) {

		} else if (!strcmp(cmd, "usemtl")) {

		} else if (!strcmp(cmd, "mtllib")) {

		} else if (!strcmp(cmd, "shadow_obj")) {

		} else if (!strcmp(cmd, "trace_obj")) {

		} else if (!strcmp(cmd, "bsp")) { /* obsolete */

		} else if (!strcmp(cmd, "bzp")) { /* obsolete */

		} else if (!strcmp(cmd, "cdc")) { /* obsolete */

		} else if (!strcmp(cmd, "res")) { /* obsolete */

		} else if (!strcmp(cmd, "c_interp")) {

		} else if (!strcmp(cmd, "bevel")) {

		} else if (!strcmp(cmd, "curv")) {

		} else if (!strcmp(cmd, "curv2")) {

		} else if (!strcmp(cmd, "surf")) {

		} else if (!strcmp(cmd, "parm")) {

		} else if (!strcmp(cmd, "trim")) {

		} else if (!strcmp(cmd, "end")) {

		} else if (!strcmp(cmd, "hole")) {

		} else if (!strcmp(cmd, "sp")) {

		} else if (!strcmp(cmd, "scrv")) {

		} else if (!strcmp(cmd, "ctech")) {

		} else if (!strcmp(cmd, "stech")) {

		} else if (!strcmp(cmd, "con")) {
#endif
		} else {
			debug("%s:%u:I don't know how to handle OBJ type '%s'!\n",
				filename, line, cmd);
		}
	}
	if (!model_verify(m)) {
		debug("%s:model verification failed\n", filename);
		goto error;
	}
	return m;
error:
	model_free(m);
	debug("%s:%u:failed!\n", filename, line);
	return 0;
}

struct model *obj_load(const char *filename)
{
	FILE *f;
	struct model *m;
	f = fopen(filename, "r");
	if (!f) {
		warn("%s:%s\n", filename, strerror(errno));
#if 0
		char cwd[PATH_MAX];
		getcwd(cwd, sizeof cwd);
		fprintf(stderr, "CWD=\"%s\"\n", cwd);
#endif
		return 0;
	}
	m = obj_load_from_file(f, filename);
	fclose(f);
	return m;
}

int obj_save(const char *filename, struct model *m)
{
	FILE *out;
	struct object *o;
	int i, j, k;

	out = fopen(filename, "w");
	for (i = 0; i < m->nr_object; i++) {
		int nr_vertex;
		float (*vertex)[3];

		o = m->object + i;
		if (o->global_vertex) {
			nr_vertex = m->nr_vertex;
			vertex = m->vertex;
		} else {
			/* TODO: handle per object vertex tables */
			goto error;
			/*
			nr_vertex=o->nr_vertex;
			vertex=o->vertex;
			*/
		}
		printf("[%s]\n", o->tag?o->tag:"noname");
		printf("  faces=%d vertices=%d\n", o->nr_face, nr_vertex);
		for (j = 0; j < nr_vertex; j++)
			fprintf(out, "v %f %f %f\n",
				vertex[j][0], vertex[j][1], vertex[j][2]);
		for (j = 0; j < o->nr_face; j++) {
			fprintf(out, "f");
			for (k = 0; k < 3; k++)
				fprintf(out, " %u", o->face[j][k] + 1);
			fprintf(out, "\n");
		}

	}

	fclose(out);
	return 1;

error:
	fclose(out);
	return 0;
}
