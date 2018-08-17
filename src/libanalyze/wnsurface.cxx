#include "common.h"
#include "nanoflann.hpp"

#include "vmath.h"
#include "rt/geom.h"

struct NF_PC {
    std::vector<point_t *> pts;
};

struct NF_Adaptor {
    const struct NF_PC obj;
    NF_Adaptor(const struct NF_PC &obj_) : obj(obj_) { }
    inline const struct NF_PC& derived() const {return obj;}
    inline size_t kdtree_get_point_count() const { return derived().pts.size(); }
    inline fastf_t kdtree_get_pt(const size_t idx, int dim) const
    {
	if (dim == 0) return (*(derived().pts[idx]))[X];
	if (dim == 1) return (*(derived().pts[idx]))[Y];
	return (*(derived().pts[idx]))[Z];
    }
    template <class BBOX> bool kdtree_get_bbox(BBOX&) const {return false;}
};

/* Winding Number Meshing (as described in the paper at
 * http://www.dgp.toronto.edu/projects/fast-winding-numbers/):
 */

extern "C" void
wn_mesh(struct rt_pnts_internal *pnts)
{
    int pind = 0;
    fastf_t *ai;
    struct pnt_normal *pn, *pl;
    NF_PC cloud;
    const NF_Adaptor pc2kd(cloud);
    typedef nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<fastf_t, NF_Adaptor >, NF_Adaptor, 3 > nf_kdtree_t;

    /* 1.  Build kdtree for nanoflann to get nearest neighbor lookup */
    nf_kdtree_t index(3, pc2kd, nanoflann::KDTreeSingleIndexAdaptorParams(10));
    pl = (struct pnt_normal *)pnts->point;
    for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {
	cloud.pts.push_back(&(pn->v));
    }
    index.buildIndex();

    /* Calculate a[i] parameters for points */
    ai = (fastf_t *)bu_calloc(pnts->count, sizeof(fastf_t), "a[i] values");
    pl = (struct pnt_normal *)pnts->point;
    pind = 0;
    for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {
	const size_t num_results = 1;
	size_t ret_index;
	fastf_t out_dist_sq;

	/* 2.  Find k-nearest-neighbors set */
	nanoflann::KNNResultSet<fastf_t> resultSet(num_results);
	resultSet.init(&ret_index, &out_dist_sq);
	index.findNeighbors(resultSet, &(pn->v), nanoflann::SearchParams(10));

	/* MAYBE: Discard point if it doesn't have more than 2 other points
	 * within .01*radius of the bounding sphere of the object - in that situation
	 * it's too sparse to be a useful indicator and may even be some stray
	 * grazing ray reporting weirdness... */


	/* 3.  Find best fit plane for knn set.  Thoughts:
	 *
	 * Port https://github.com/lucasmaystre/svdlibc to libbn data types and use
	 * the SVD plane fitter from test_bot2nurbs.cpp */


	/* 4.  Project 3D points into plane, get 2D pnt set */


	/* 5.  Calculate Voronoi diagram in 2D.  Thoughts:
	 *
	 * Incorporate https://github.com/JCash/voronoi with (maybe) some
	 * additional mapping logic to make sure we can extract a polygon for a
	 * given point from the generated voronoi diagram.
	 */

	/* 6.  Calculate the area of the Voronoi polygon around the current
	 * point - paper uses TRIANGLE but couldn't we just use
	 * http://alienryderflex.com/polygon_area/ for the area? */

	/*ai[pind] = polygon_area(...);*/

	pind++;
    }

    /* 7. Figure out how to set up the actual WN calculation to make the input
     * used to drive polygonalization, based on:
     *
     * https://github.com/sideeffects/WindingNumber
     * https://www.sidefx.com/docs/hdk/_s_o_p___winding_number_8_c_source.html
     *
     */


    /* 8. The paper mentions using a "continuation method [Wyvill et al.  1986]
     * for voxelization and isosurface extraction."  That appears to be this
     * approach:
     *
     * http://www.unchainedgeometry.com/jbloom/papers.html
     * https://github.com/Tonsty/polygonizer
     *
     * although again we have to figure out how to feed its inputs using the WN
     */
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
