/*                      D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2018 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** @addtogroup brep_defines
 *
 * @brief
 * These are definitions specific to libbrep, used throughout the library.
 *
 */
#ifndef BREP_DEFINES_H
#define BREP_DEFINES_H

#include "common.h"

/** @{ */
/** @file brep/defines.h */

#ifndef BREP_EXPORT
#  if defined(BREP_DLL_EXPORTS) && defined(BREP_DLL_IMPORTS)
#    error "Only BREP_DLL_EXPORTS or BREP_DLL_IMPORTS can be defined, not both."
#  elif defined(BREP_DLL_EXPORTS)
#    define BREP_EXPORT __declspec(dllexport)
#  elif defined(BREP_DLL_IMPORTS)
#    define BREP_EXPORT __declspec(dllimport)
#  else
#    define BREP_EXPORT
#  endif
#endif

#ifndef __cplusplus
/**
 * @brief Placeholder for ON_Brep to allow brep.h to compile when we're
 * compiling with a C compiler
 */
typedef struct _on_brep_placeholder {
        int dummy; /* MS Visual C hack which can be removed if the struct contains something meaningful */
} ON_Brep;
#endif

/** Maximum number of newton iterations on root finding */
#define BREP_MAX_ITERATIONS 100

/** Root finding threshold */
#define BREP_INTERSECTION_ROOT_EPSILON 1e-6

/* if threshold not reached what will we settle for close enough */
#define BREP_INTERSECTION_ROOT_SETTLE 1e-2

/** Jungle Gym epsilon */

/** tighten BREP grazing tolerance to 0.000017453(0.001 degrees) was using RT_DOT_TOL at 0.001 (0.05 degrees) **/
#define BREP_GRAZING_DOT_TOL 0.000017453

/** Use vector operations? For debugging */
#define DO_VECTOR 1

/** Maximum per-surface BVH depth */
#define BREP_MAX_FT_DEPTH 8
#define BREP_MAX_LN_DEPTH 20

#define SIGN(x) ((x) >= 0 ? 1 : -1)

/** Surface flatness parameter, Abert says between 0.8-0.9 */
#define BREP_SURFACE_FLATNESS 0.85
#define BREP_SURFACE_STRAIGHTNESS 0.75

/** Max newton iterations when finding closest point */
#define BREP_MAX_FCP_ITERATIONS 50

/** Root finding epsilon */
#define BREP_FCP_ROOT_EPSILON 1e-5

/** trim curve point sampling count for isLinear() check and possibly
 *  * growing bounding box
 *   */
#define BREP_BB_CRV_PNT_CNT 10

#define BREP_CURVE_FLATNESS 0.95

/** subdivision size factors */
#define BREP_SURF_SUB_FACTOR 1
#define BREP_TRIM_SUB_FACTOR 1

/**
 * The EDGE_MISS_TOLERANCE setting is critical in a couple of ways -
 * too small and the allowed uncertainty region near edges will be
 * smaller than the actual uncertainty needed for accurate solid
 * raytracing, too large and trimming will not be adequate. In order
 * to adapt this to UV space, the ratio of the Brep surface's
 * diagonal length and the tolerance is used to solve for a
 * corresponding UV space tolerance, given the 2D UV space diagonal
 * length of the surface in question.  Hence, the latter value
 * is not a constant but is adjusted to fit the given surface's UV space.
 *
 *   surface 3D diagonal length      surface 2D diagonal length
 *   ---------------------------  =  --------------------------
 *   BREP_3D_EDGE_MISS_TOLERANCE         uv_edge_miss_tol
 *
  */
#define BREP_3D_EDGE_BOUNDARY_TOLERANCE 0.1
#define BREP_EDGE_MISS_TOLERANCE 5e-3

#define BREP_SAME_POINT_TOLERANCE 1e-6

/* arbitrary calculation tolerance */
#define BREP_UV_DIST_FUZZ 0.000001

/* @todo: debugging crapola (clean up later) */
#define ON_PRINT4(p) "[" << (p)[0] << ", " << (p)[1] << ", " << (p)[2] << ", " << (p)[3] << "]"
#define ON_PRINT3(p) "(" << (p)[0] << ", " << (p)[1] << ", " << (p)[2] << ")"
#define ON_PRINT2(p) "(" << (p)[0] << ", " << (p)[1] << ")"
#define PT(p) ON_PRINT3(p)
#define PT2(p) ON_PRINT2(p)
#define IVAL(_ival) "[" << (_ival).m_t[0] << ", " << (_ival).m_t[1] << "]"
#define TRACE(s)
#define TRACE1(s)
#define TRACE2(s)
/* #define TRACE(s) std::cerr << s << std::endl; */
/* #define TRACE1(s) std::cerr << s << std::endl; */
/* #define TRACE2(s) std::cerr << s << std::endl; */

#ifdef __cplusplus
extern "C++" {
struct BrepTrimPoint
{
    ON_2dPoint p2d; /* 2d surface parameter space point */
    ON_3dPoint *p3d; /* 3d edge/trim point depending on whether we're using the 3d edge to generate points or the trims */
    double t;     /* corresponding trim curve parameter (ON_UNSET_VALUE if unknown or not pulled back) */
    double e;     /* corresponding edge curve parameter (ON_UNSET_VALUE if using trim not edge) */
};}
#endif



/** @} */

#endif  /* BREP_DEFINES_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
