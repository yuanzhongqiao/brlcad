/*                   L O D _ P O L Y L I N E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Based off of code from https://github.com/bhaettasch/pop-buffer-demo
 * Copyright (c) 2016 Benjamin Hättasch and X3DOM
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/** @file lod_polyline.cpp
 *
 * This file implements level-of-detail routines for polyline based displays.
 */

#include "common.h"

#include <vector>

#include "vmath.h"
#include "bu/malloc.h"
#include "bv/defines.h"
#include "bg/aabb.h"
#include "bg/geom.h"
#include "bg/lod.h"
#include "bg/sample.h"

static int
rebuild_vect(std::vector<std::vector<fastf_t>> &point_arrays, struct bv_polyline_lod *l, point_t *pa, int pcnt, size_t ind)
{
    int ptotal = pcnt + 1;
    if (ptotal == l->pcnts[ind])
	return 0;
    l->pcnts[ind] = ptotal;
    point_arrays[ind].clear();
    point_arrays[ind].reserve(l->pcnts[ind]*3);
    for (int i = 0; i < l->pcnts[ind] - 1; i++) {
	for (int j = 0; j < 3; j++)
	    point_arrays[ind].push_back(pa[i][j]);
    }
    for (int j = 0; j < 3; j++)
	point_arrays[ind].push_back(pa[0][j]);
    point_arrays[ind].shrink_to_fit();
    l->parrays[ind] = (point_t *)point_arrays[ind].data();
    return 1;
}

static fastf_t
bbox_curve_count(
	point_t bbox_min, point_t bbox_max,
	fastf_t curve_scale,
	fastf_t s_size)
{
    fastf_t x_len, y_len, z_len, avg_len;

    x_len = fabs(bbox_max[X] - bbox_min[X]);
    y_len = fabs(bbox_max[Y] - bbox_min[Y]);
    z_len = fabs(bbox_max[Z] - bbox_min[Z]);

    avg_len = (x_len + y_len + z_len) / 3.0;

    fastf_t curve_spacing = s_size / 2.0;
    curve_spacing /= curve_scale;
    return avg_len / curve_spacing;
}

// We need a local copy of the geometry object data, so we
// don't have to persist the rt_db_internals which are (usually)
// the original source of this information
void *
obj_data_cpy(point_t *bbmin, point_t *bbmax, void *gd, unsigned int type)
{
    switch (type) {
	case ID_TOR:
	    {
		struct bg_torus *nt;
		struct bg_torus *st = (struct bg_torus *)gd;
		BG_TOR_CK_MAGIC(st);
		BU_GET(nt, struct bg_torus);
		*nt = *st;
		bg_tor_bbox(bbmin, bbmax, nt);
		return nt;
	    }
	default:
	    return NULL;
    }
}

class PolyLines;
struct bg_polyline_lod_internal {
    PolyLines *i;
};

class PolyLines {
    public:
	PolyLines(void *gd, unsigned int type);
	struct bv_polyline_lod *l;

	point_t bbmin = {INFINITY, INFINITY, INFINITY};
	point_t bbmax = {-INFINITY, -INFINITY, -INFINITY};
	int update(struct bview *v); // return 1 if changed, else 0

	bool valid = false;

    private:

	fastf_t prev_size = -1;

	std::vector<std::vector<fastf_t>> point_arrays;

	// Wireframe updating is specific to each object type
	int torus_update(struct bg_torus *tor, struct bview *v, fastf_t s_size);

	// gdata holds the object-specific data for this instance
	void *gdata = NULL;
	unsigned int data_type = 0;
};

PolyLines::PolyLines(void *gd, unsigned int type)
{
    data_type = type;
    gdata = obj_data_cpy(&bbmin, &bbmax, gd, type);
    if (gdata)
	valid = true;
}

static int
tor_ellipse_points(
	vect_t ellipse_A,
	vect_t ellipse_B,
	fastf_t point_spacing)
{
    fastf_t avg_radius, circumference;

    avg_radius = (MAGNITUDE(ellipse_A) + MAGNITUDE(ellipse_B)) / 2.0;
    circumference = M_2PI * avg_radius;

    return circumference / point_spacing;
}

int
PolyLines::torus_update(struct bg_torus *tor, struct bview *v, fastf_t s_size)
{
    int ret = 0;
    if (!l || !tor || !v)
	return 0;

    vect_t a, b, tor_a, tor_b, tor_h, center;
    fastf_t mag_a, mag_b, mag_h;
    int points_per_ellipse;

    BG_TOR_CK_MAGIC(tor);

    VMOVE(l->bmin, bbmin);
    VMOVE(l->bmax, bbmax);

    // Calculate some convenience values from the torus
    VMOVE(tor_a, tor->a);
    mag_a = tor->r_a;
    VSCALE(tor_h, tor->h, tor->r_h);
    mag_h = tor->r_h;
    VCROSS(tor_b, tor_a, tor_h);
    VUNITIZE(tor_b);
    VSCALE(tor_b, tor_b, mag_a);
    mag_b = mag_a;

    // Using view and size info, determine a sampling interval
    fastf_t point_spacing = bg_sample_spacing(v, s_size);

    // Based on the initial recommended sampling, see how many
    // points this particular object will require
    VJOIN1(a, tor_a, mag_h / mag_a, tor_a);
    VJOIN1(b, tor_b, mag_h / mag_b, tor_b);
    points_per_ellipse = tor_ellipse_points(a, b, point_spacing);
    if (points_per_ellipse < 6) {
	// If we're this small, just draw a point
	if (point_arrays.size() == 1)
	    return 0;
	point_arrays.clear();
	point_arrays.shrink_to_fit();
	point_arrays.reserve(1);
	point_arrays.push_back(std::vector<fastf_t>());
	point_arrays[0].reserve(3);
	for (int i = 0; i < 3; i++)
	    point_arrays[0].push_back(tor->v[i]);
	l->array_cnt = 1;
	l->parrays = (point_t **)bu_realloc(l->parrays, l->array_cnt * sizeof(point_t *), "points array alloc");
	l->pcnts = (int *)bu_realloc(l->pcnts, l->array_cnt * sizeof(int), "point array counts array alloc");
	l->pcnts[0] = 1;
	l->parrays[0] = (point_t *)point_arrays[0].data();
	return 1;
    }

    // To allocate the correct memory, we first determine how many polylines we will
    // be creating for this particular object
    l->array_cnt = 4;
    int num_ellipses = bbox_curve_count(l->bmin, l->bmax, v->gv_s->curve_scale, s_size);
    if (num_ellipses < 3)
	num_ellipses = 3;
    l->array_cnt += num_ellipses;

    // If the array is pre-existing, resize to what we need
    if (point_arrays.size() != (size_t)l->array_cnt) {

	// Something is different in the data - we're updating
	ret = 1;

	while (point_arrays.size() > (size_t)l->array_cnt) {
	    point_arrays.pop_back();
	}
	point_arrays.shrink_to_fit();
	point_arrays.reserve(l->array_cnt);
	for (size_t i = point_arrays.size(); i < (size_t)l->array_cnt; i++) {
	    point_arrays.push_back(std::vector<fastf_t>());
	}

	// Set up the public facing containers
	l->parrays = (point_t **)bu_realloc(l->parrays, l->array_cnt * sizeof(point_t *), "points array alloc");
	l->pcnts = (int *)bu_realloc(l->pcnts, l->array_cnt * sizeof(int), "point array counts array alloc");
	for (int i = 0; i < l->array_cnt; i++) {
	    l->pcnts[i] = 0;
	}
    }

    /* plot outer circular contour */
    point_t *pa;
    int pcnt = bg_ell_sample(&pa, tor->v, a, b, points_per_ellipse);
    ret += rebuild_vect(point_arrays, l, pa, pcnt, 0);
    bu_free(pa, "pnts");

    /* plot inner circular contour */
    VJOIN1(a, tor_a, -1.0 * mag_h / mag_a, tor_a);
    VJOIN1(b, tor_b, -1.0 * mag_h / mag_b, tor_b);
    points_per_ellipse = tor_ellipse_points(a, b, point_spacing);
    pcnt = bg_ell_sample(&pa, tor->v, a, b, points_per_ellipse);
    ret += rebuild_vect(point_arrays, l, pa, pcnt, 1);
    bu_free(pa, "pnts");


    /* Draw parallel circles to show the primitive's most extreme points along
     * +h/-h.
     */
    points_per_ellipse = tor_ellipse_points(tor_a, tor_b, point_spacing);
    VADD2(center, tor->v, tor_h);
    pcnt = bg_ell_sample(&pa, center, tor_a, tor_b, points_per_ellipse);
    ret += rebuild_vect(point_arrays, l, pa, pcnt, 2);
    bu_free(pa, "pnts");


    VJOIN1(center, tor->v, -1.0, tor_h);
    pcnt = bg_ell_sample(&pa, center, tor_a, tor_b, points_per_ellipse);
    ret += rebuild_vect(point_arrays, l, pa, pcnt, 3);
    bu_free(pa, "pnts");


    /* draw circular radial cross sections */
    VMOVE(b, tor_h);
    fastf_t radian_step = M_2PI / num_ellipses;
    fastf_t radian = 0;
    for (int i = 0; i < num_ellipses; ++i) {
	bg_ell_radian_pnt(center, tor->v, tor_a, tor_b, radian);

	VJOIN1(a, center, -1.0, tor->v);
	VUNITIZE(a);
	VSCALE(a, a, mag_h);

	pcnt = bg_ell_sample(&pa, center, a, b, points_per_ellipse);
	ret += rebuild_vect(point_arrays, l, pa, pcnt, i+4);
	bu_free(pa, "pnts");

	radian += radian_step;
    }

    return ret;
}


int
PolyLines::update(struct bview *v)
{
    if (!gdata || !data_type)
	return 0;

    // If our size hasn't changed substantially, don't bother updating
    fastf_t delta = fabs(v->gv_size - prev_size);
    if (delta < 100)
	return 0;

    switch (data_type) {
	case ID_TOR:
	    {
		struct bg_torus *t = (struct bg_torus *)gdata;
		BG_TOR_CK_MAGIC(t);
		int ret = torus_update(t, v, l->s->s_size);
		if (ret)
		    prev_size = v->gv_size;
		return ret;
	    }
	default:
	    return 0;
    }

    return 0;
}

extern "C" struct bv_polyline_lod *
bg_polyline_lod_create(void *gd, unsigned int type)
{
    PolyLines *pl = new PolyLines(gd, type);
    if (!pl->valid) {
	delete pl;
	return NULL;
    }

    BU_GET(pl->l, struct bv_polyline_lod);
    VMOVE(pl->l->bmin, pl->bbmin);
    VMOVE(pl->l->bmax, pl->bbmax);
    BU_GET(pl->l->i, struct bg_polyline_lod_internal);
    pl->l->parrays = NULL;
    pl->l->pcnts = NULL;
    struct bg_polyline_lod_internal *i = (struct bg_polyline_lod_internal *)pl->l->i;
    i->i = pl;

    return pl->l;
}

extern "C" void
bg_polyline_lod_destroy(struct bv_polyline_lod *l)
{
    if (!l)
	return;

    delete ((struct bg_polyline_lod_internal *)l->i)->i;
    BU_PUT(l->i, struct bg_polyline_lod_internal);
    BU_PUT(l, struct bv_polyline_lod);
}

int
bg_polyline_lod_view(struct bv_scene_obj *s, struct bview *v)
{
    struct bv_polyline_lod *l = (struct bv_polyline_lod *)s->draw_data;
    struct bg_polyline_lod_internal *i = (struct bg_polyline_lod_internal *)l->i;
    int ret = i->i->update(v);
    if (ret) {
	s->s_dlist_stale = 1;
    }
    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
