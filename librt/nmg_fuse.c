/*
 *			N M G _ F U S E . C
 *
 *  Routines to "fuse" entities together that are geometrically identical
 *  (within tolerance) into entities that share underlying geometry
 *  structures, so that the relationship is explicit.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"
#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "./debug.h"

/* XXX Move to raytrace.h */
RT_EXTERN(struct vertexuse 	*nmg_is_vertex_in_face, (CONST struct vertex *v,
				CONST struct face *f));
RT_EXTERN(struct edge_g_lseg	*nmg_pick_best_edge_g, (struct edgeuse *eu1,
				struct edgeuse *eu2, CONST struct rt_tol *tol));
RT_EXTERN(void			nmg_pr_radial_list, (CONST struct rt_list *hd,
				CONST struct rt_tol *tol));


/* XXX Move to nmg.h */
#define NMG_TBL_LEN(p)	((p)->end)

/* XXX move to rtlist.h */
#define RT_LIST_FOR_BACKWARDS(p,structure,hp)	\
	(p)=RT_LIST_LAST(structure,hp); \
	RT_LIST_NOT_HEAD(p,hp); \
	(p)=RT_LIST_PLAST(structure,p)

/* Process all the list members except hp and the actual head */
#define RT_LIST_FOR_CIRC(p,structure,hp)	\
	(p)=RT_LIST_PNEXT_CIRC(structure,hp); \
	(p) != (hp); \
	(p)=RT_LIST_PNEXT_CIRC(structure,p)


/* XXX Move to nmg_info.c */
/*
 *			N M G _ I S _ V E R T E X _ I N _ F A C E
 *
 *  Returns -
 *	vu	One use of vertex 'v' in face 'f'.
 *	NULL	If there are no uses of 'v' in 'f'.
 */
struct vertexuse *
nmg_is_vertex_in_face( v, f )
CONST struct vertex	*v;
CONST struct face	*f;
{
	struct vertexuse	*vu;

	NMG_CK_VERTEX(v);
	NMG_CK_FACE(f);

	for( RT_LIST_FOR( vu, vertexuse, &v->vu_hd ) )  {
		CONST register struct edgeuse	*eu;
		CONST register struct loopuse	*lu;
		CONST register struct faceuse	*fu;

		if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )  continue;
		if( *(eu = vu->up.eu_p)->up.magic_p != NMG_LOOPUSE_MAGIC )  continue;
		lu = eu->up.lu_p;
		if( *lu->up.magic_p != NMG_FACEUSE_MAGIC )  continue;
		fu = lu->up.fu_p;
		if( fu->f_p != f )  continue;
		return vu;
	}
	return (struct vertexuse *)NULL;
}

/*
 *			N M G _ I S _ C O M M O N _ B I G L O O P
 *
 *  Do two faces share by topology at least one loop of 3 or more vertices?
 *
 *  Require that at least three distinct edge geometries be involved.
 *
 * XXX Won't catch sharing of faces with only self-loops and no edge loops.
 */
int
nmg_is_common_bigloop( f1, f2 )
CONST struct face	*f1;
CONST struct face	*f2;
{
	CONST struct faceuse	*fu1;
	CONST struct loopuse	*lu1;
	CONST struct edgeuse	*eu1;
	CONST long		*magic1 = (long *)NULL;
	CONST long		*magic2 = (long *)NULL;
	int	nverts;
	int	nbadv;
	int	got_three;

	NMG_CK_FACE(f1);
	NMG_CK_FACE(f2);

	fu1 = f1->fu_p;
	NMG_CK_FACEUSE(fu1);

	/* For all loopuses in fu1 */
	for( RT_LIST_FOR( lu1, loopuse, &fu1->lu_hd ) )  {
		if( RT_LIST_FIRST_MAGIC(&lu1->down_hd) == NMG_VERTEXUSE_MAGIC )
			continue;
		nverts = 0;
		nbadv = 0;
		magic1 = (long *)NULL;
		magic2 = (long *)NULL;
		got_three = 0;
		for( RT_LIST_FOR( eu1, edgeuse, &lu1->down_hd ) )  {
			nverts++;
			NMG_CK_EDGE_G_LSEG(eu1->g.lseg_p);
			if( !magic1 )  {
				magic1 = eu1->g.magic_p;
			} else if( !magic2 )  {
				if( eu1->g.magic_p != magic1 )  {
					magic2 = eu1->g.magic_p;
				}
			} else {
				if( eu1->g.magic_p != magic1 &&
				    eu1->g.magic_p != magic2 )  {
					got_three = 1;
				}
			}
			if( nmg_is_vertex_in_face( eu1->vu_p->v_p, f2 ) )
				continue;
			nbadv++;
			break;
		}
		if( nbadv <= 0 && nverts >= 3 && got_three )  return 1;
	}
	return 0;
}

/*
 *			N M G _ R E G I O N _ V _ U N I Q U E
 *
 *  Ensure that all the vertices in r1 are still geometricaly unique.
 *  This will be true after nmg_region_both_vfuse() has been called,
 *  and should remain true throughout the intersection process.
 */
void
nmg_region_v_unique( r1, tol )
struct nmgregion	*r1;
CONST struct rt_tol	*tol;
{
	int	i;
	int	j;
	struct nmg_ptbl	t;

	NMG_CK_REGION(r1);
	RT_CK_TOL(tol);

	nmg_vertex_tabulate( &t, &r1->l.magic );

	for( i = NMG_TBL_END(&t)-1; i >= 0; i-- )  {
		register struct vertex	*vi;
		vi = (struct vertex *)NMG_TBL_GET(&t, i);
		NMG_CK_VERTEX(vi);
		if( !vi->vg_p )  continue;

		for( j = i-1; j >= 0; j-- )  {
			register struct vertex	*vj;
			vj = (struct vertex *)NMG_TBL_GET(&t, j);
			NMG_CK_VERTEX(vj);
			if( !vj->vg_p )  continue;
			if( !rt_pt3_pt3_equal( vi->vg_p->coord, vj->vg_p->coord, tol ) )
				continue;
			/* They are the same */
			rt_log("nmg_region_v_unique():  2 verts are the same, within tolerance\n");
			nmg_pr_v( vi, 0 );
			nmg_pr_v( vj, 0 );
			rt_bomb("nmg_region_v_unique()\n");
		}
	}
	nmg_tbl( &t, TBL_FREE, 0 );
}

/*
 *			N M G _ P T B L _ V F U S E
 *
 *  Working from the end to the front, scan for geometric duplications
 *  within a single list of vertex structures.
 *
 *  Exists primarily as a support routine for nmg_model_vertex_fuse().
 */
int
nmg_ptbl_vfuse( t, tol )
struct nmg_ptbl		*t;
CONST struct rt_tol	*tol;
{
	int	count = 0;
	int	i;
	int	j;

	for( i = NMG_TBL_END(t)-1; i >= 0; i-- )  {
		register struct vertex	*vi;
		vi = (struct vertex *)NMG_TBL_GET(t, i);
		NMG_CK_VERTEX(vi);
		if( !vi->vg_p )  continue;

		for( j = i-1; j >= 0; j-- )  {
			register struct vertex	*vj;
			vj = (struct vertex *)NMG_TBL_GET(t, j);
			NMG_CK_VERTEX(vj);
			if( !vj->vg_p )  continue;
			if( !rt_pt3_pt3_equal( vi->vg_p->coord, vj->vg_p->coord, tol ) )
				continue;
			/* They are the same, fuse vi into vj */
			nmg_jv( vj, vi );
			nmg_tbl( t, TBL_RM, (long *)vi );
			count++;
			break;
		}
	}
	return count;
}

/*
 *			N M G _ R E G I O N _ B O T H _ V F U S E
 *
 *  For every element in t1, scan t2 for geometric duplications.
 *
 *  Deleted elements in t2 are marked by a null vertex pointer,
 *  rather than bothering to do a TBL_RM, which will re-copy the
 *  list to compress it.
 *
 *  Exists as a support routine for nmg_two_region_vertex_fuse()
 */
int
nmg_region_both_vfuse( t1, t2, tol )
struct nmg_ptbl		*t1;
struct nmg_ptbl		*t2;
CONST struct rt_tol	*tol;
{
	int	count = 0;
	int	i;
	int	j;

	/* Verify t2 is good to start with */
	for( j = NMG_TBL_END(t2)-1; j >= 0; j-- )  {
		register struct vertex	*vj;
		vj = (struct vertex *)NMG_TBL_GET(t2, j);
		NMG_CK_VERTEX(vj);
	}

	for( i = NMG_TBL_END(t1)-1; i >= 0; i-- )  {
		register struct vertex	*vi;
		vi = (struct vertex *)NMG_TBL_GET(t1, i);
		NMG_CK_VERTEX(vi);
		if( !vi->vg_p )  continue;

		for( j = NMG_TBL_END(t2)-1; j >= 0; j-- )  {
			register struct vertex	*vj;
			vj = (struct vertex *)NMG_TBL_GET(t2, j);
			if(!vj) continue;
			NMG_CK_VERTEX(vj);
			if( !vj->vg_p )  continue;
			if( !rt_pt3_pt3_equal( vi->vg_p->coord, vj->vg_p->coord, tol ) )
				continue;
			/* They are the same, fuse vj into vi */
			nmg_jv( vi, vj );
			NMG_TBL_GET(t2, j) = 0;
			count++;
		}
	}
	return count;
}

#if 0
/*
 *			N M G _ T W O _ R E G I O N _ V E R T E X _ F U S E
 *
 *  This routine should not be used.  Instead, call nmg_model_vertex_fuse().
 */
int
nmg_two_region_vertex_fuse( r1, r2, tol )
struct nmgregion	*r1;
struct nmgregion	*r2;
CONST struct rt_tol	*tol;
{
	struct nmg_ptbl	t1;
	struct nmg_ptbl	t2;
	int		total = 0;

	NMG_CK_REGION(r1);
	NMG_CK_REGION(r2);
	RT_CK_TOL(tol);

	if( r1->m_p != r2->m_p )  rt_bomb("nmg_two_region_vertex_fuse:  regions not in same model\n");

	nmg_vertex_tabulate( &t1, &r1->l.magic );
	nmg_vertex_tabulate( &t2, &r2->l.magic );

	total = nmg_ptbl_vfuse( &t1, tol );
	total += nmg_region_both_vfuse( &t1, &t2, tol );

	nmg_tbl( &t1, TBL_FREE, 0 );
	nmg_tbl( &t2, TBL_FREE, 0 );

	return total;
}
#endif

/*
 *			N M G _ M O D E L _ V E R T E X _ F U S E
 *
 *  Fuse together any vertices in the nmgmodel that are geometricly
 *  identical, within the tolerance.
 */
int
nmg_model_vertex_fuse( m, tol )
struct model		*m;
CONST struct rt_tol	*tol;
{
	struct nmg_ptbl	t1;
	int		total = 0;

	NMG_CK_MODEL(m);
	RT_CK_TOL(tol);

	nmg_vertex_tabulate( &t1, &m->magic );

	total = nmg_ptbl_vfuse( &t1, tol );

	nmg_tbl( &t1, TBL_FREE, 0 );

	if( rt_g.NMG_debug & DEBUG_BASIC && total > 0 )
		rt_log("nmg_model_vertex_fuse() %d\n", total);
	return total;
}

/*
 *			N M G _ M O D E L _ E D G E _ F U S E
 */
int
nmg_model_edge_fuse( m, tol )
struct model		*m;
CONST struct rt_tol	*tol;
{
	struct nmg_ptbl	eutab;
	int		total = 0;
	register int	i,j;

	NMG_CK_MODEL(m);
	RT_CK_TOL(tol);

	/* Make a list of all the edgeuse structs in the model */
again:
	total = 0;
	nmg_edgeuse_tabulate( &eutab, &m->magic );

	for( i = NMG_TBL_END(&eutab)-1; i >= 0; i-- )  {
		struct edgeuse		*eu1;
		struct edge		*e1;
		register struct vertex	*v1a, *v1b;

		eu1 = (struct edgeuse *)NMG_TBL_GET(&eutab, i);
		NMG_CK_EDGEUSE(eu1);
		e1 = eu1->e_p;
		NMG_CK_EDGE(e1);

		v1a = eu1->vu_p->v_p;
		v1b = eu1->eumate_p->vu_p->v_p;
		NMG_CK_VERTEX(v1a);
		NMG_CK_VERTEX(v1b);

		if( v1a == v1b )  {
			/* 0-length edge.  Eliminate. */
			/* These should't happen, but need fixing. */
			if( nmg_keu( eu1 ) )  {
				rt_log("nmg_model_edge_fuse() WARNING:  deletion of 0-length edgeuse=x%x caused parent to go empty.\n", eu1);
			}
			rt_log("nmg_model_edge_fuse() 0-length edgeuse=x%x deleted\n", eu1);
			/* XXX no way to know where edgeuse mate will be */
			nmg_tbl( &eutab, TBL_FREE, 0 );
			goto again;
		}

		/* For performance, don't recheck pointers here */
		for( j = i-1; j >= 0; j-- )  {
			register struct edgeuse	*eu2;
			register struct vertex	*v2a, *v2b;

			eu2 = (struct edgeuse *)NMG_TBL_GET(&eutab,j);

			/* Do this vertex test first, to reduce memory loads */
			v2a = eu2->vu_p->v_p;
			if( v2a != v1a && v2a != v1b )  continue;

			if( e1 == eu2->e_p )  continue;	/* Already shared */
			v2b = eu2->eumate_p->vu_p->v_p;
			if( (v2a == v1a && v2b == v1b) ||
			    (v2b == v1a && v2a == v1b) )  {
				nmg_radial_join_eu(eu1, eu2, tol);
			     	total++;
			 }
		}
	}
	if( rt_g.NMG_debug & DEBUG_BASIC && total > 0 )
		rt_log("nmg_model_edge_fuse(): %d edges fused\n", total);
	nmg_tbl( &eutab, TBL_FREE, 0 );
	return total;
}

/* XXX move to nmg_info.c */
/*
 *			N M G _ 2 E D G E U S E _ G _ C O I N C I D E N T
 *
 *  Given two edgeuses, determine if they share the same edge geometry,
 *  either topologically, or within tolerance.
 *
 *  Returns -
 *	0	two edge geometries are not coincident
 *	1	edges geometries are everywhere coincident.
 *		(For linear edge_g_lseg, the 2 are the same line, within tol.)
 */
int
nmg_2edgeuse_g_coincident( eu1, eu2, tol )
CONST struct edgeuse	*eu1;
CONST struct edgeuse	*eu2;
CONST struct rt_tol	*tol;
{
	struct edge_g_lseg	*eg1;
	struct edge_g_lseg	*eg2;

	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);
	RT_CK_TOL(tol);

	eg1 = eu1->g.lseg_p;
	eg2 = eu2->g.lseg_p;
	NMG_CK_EDGE_G_LSEG(eg1);
	NMG_CK_EDGE_G_LSEG(eg2);

	if( eg1 == eg2 )  return 1;

	/* Ensure direction vectors are generally parallel */
	/* These are not unit vectors */
	/* tol->para and RT_DOT_TOL are too tight a tolerance.  0.1 is 5 degrees */
	if( fabs(VDOT(eg1->e_dir, eg2->e_dir)) <
	    0.9 * MAGNITUDE(eg1->e_dir) * MAGNITUDE(eg2->e_dir)  )  return 0;

	/* Ensure that vertices on edge 2 are within tol of e1 */
	if( rt_distsq_line3_pt3( eg1->e_pt, eg1->e_dir,
	    eu2->vu_p->v_p->vg_p->coord ) > tol->dist_sq )  goto trouble;
	if( rt_distsq_line3_pt3( eg1->e_pt, eg1->e_dir,
	    eu2->eumate_p->vu_p->v_p->vg_p->coord ) > tol->dist_sq )  goto trouble;

	/* Ensure that vertices of both edges are within tol of other eg */
	if( rt_distsq_line3_pt3( eg2->e_pt, eg2->e_dir,
	    eu1->vu_p->v_p->vg_p->coord ) > tol->dist_sq )  goto trouble;
	if( rt_distsq_line3_pt3( eg2->e_pt, eg2->e_dir,
	    eu1->eumate_p->vu_p->v_p->vg_p->coord ) > tol->dist_sq )  goto trouble;

	/* Perhaps check for ultra-short edges (< 10*tol->dist)? */

	/* Do not use rt_isect_line3_line3() -- it's MUCH too strict */

	return 1;
trouble:
	if( !rt_2line3_colinear( eg1->e_pt, eg1->e_dir, eg2->e_pt, eg2->e_dir,
	     1e5, tol ) )
		return 0;

	/* XXX debug */
	nmg_pr_eg( &eg1->magic, 0 );
	nmg_pr_eg( &eg2->magic, 0 );
	rt_log("nmg_2edgeuse_g_coincident() lines colinear, vertex check fails, calling colinear anyway.\n");
	return 1;
}

/*
 *			N M G _ M O D E L _ E D G E _ G _ F U S E
 *
 *  The present algorithm is a consequence of the old edge geom ptr structure.
 *  XXX This might be better formulated by generating a list of all
 *  edge_g structs in the model, and comparing *them* pairwise.
 */
int
nmg_model_edge_g_fuse( m, tol )
struct model		*m;
CONST struct rt_tol	*tol;
{
	struct nmg_ptbl	etab;
	int		total = 0;
	register int	i,j;

	NMG_CK_MODEL(m);
	RT_CK_TOL(tol);

	/* Make a list of all the edge geometry structs in the model */
	nmg_edge_g_tabulate( &etab, &m->magic );

	for( i = NMG_TBL_END(&etab)-1; i >= 0; i-- )  {
		struct edge_g_lseg		*eg1;
		struct edgeuse			*eu1;

		eg1 = (struct edge_g_lseg *)NMG_TBL_GET(&etab, i);
		NMG_CK_EDGE_G_EITHER(eg1);

		/* XXX Need routine to compare two cnurbs geometricly */
		if( eg1->magic == NMG_EDGE_G_CNURB_MAGIC )  {
			continue;
		}

		NMG_CK_EDGE_G_LSEG(eg1);
		eu1 = RT_LIST_MAIN_PTR( edgeuse, RT_LIST_FIRST( rt_list, &eg1->eu_hd2 ), l2 );
		NMG_CK_EDGEUSE(eu1);

		for( j = i-1; j >= 0; j-- )  {
			struct edge_g_lseg	*eg2;
			struct edgeuse		*eu2;

			eg2 = (struct edge_g_lseg *)NMG_TBL_GET(&etab,j);
			NMG_CK_EDGE_G_EITHER(eg2);
			if( eg2->magic == NMG_EDGE_G_CNURB_MAGIC )  continue;
			NMG_CK_EDGE_G_LSEG(eg2);
			eu2 = RT_LIST_MAIN_PTR( edgeuse, RT_LIST_FIRST( rt_list, &eg2->eu_hd2 ), l2 );
			NMG_CK_EDGEUSE(eu2);

			if( eg1 == eg2 )  rt_bomb("nmg_model_edge_g_fuse() edge_g listed twice in ptbl?\n");

			if( !nmg_2edgeuse_g_coincident( eu1, eu2, tol ) )  continue;

			/* Comitted to fusing two edge_g_lseg's.
			 * Make all instances of eg1 become eg2.
			 * XXX really should check ALL edges using eg1
			 * XXX against ALL edges using eg2 for coincidence.
			 */
		     	total++;
			nmg_jeg( eg2, eg1 );
			NMG_TBL_GET(&etab,i) = (long *)NULL;
			break;
		}
	}
	nmg_tbl( &etab, TBL_FREE, (long *)0 );
	if( rt_g.NMG_debug & DEBUG_BASIC && total > 0 )
		rt_log("nmg_model_edge_g_fuse(): %d edge_g_lseg's fused\n", total);
	return total;
}

/*
 *			N M G _ C K _ F U _ V E R T S
 *
 *  Check that all the vertices in fu1 are within tol->dist of fu2's surface.
 *  fu1 and fu2 may be the same face, or different.
 *
 *  This is intended to be a geometric check only, not a topology check.
 *  Topology may have become inappropriately shared.
 *
 *  Returns -
 *	0	All is well, or all verts are within 10*tol->dist of fu2
 *	count	Number of verts *not* on fu2's surface when at least one is
 *		more than 10*tol->dist from fu2.
 *
 *  XXX It would be more efficient to use nmg_vist() for this.
 */
int
nmg_ck_fu_verts( fu1, f2, tol )
struct faceuse	*fu1;
struct face	*f2;
CONST struct rt_tol	*tol;
{
	CONST struct face_g_plane	*fg2;
	struct nmg_ptbl		vtab;
	FAST fastf_t		dist;
	fastf_t			worst = 0;
	int			k;
	int			count = 0;

	NMG_CK_FACEUSE( fu1 );
	NMG_CK_FACE( f2 );
	RT_CK_TOL(tol);

	fg2 = f2->g.plane_p;
	NMG_CK_FACE_G_PLANE(fg2);

	nmg_vertex_tabulate( &vtab, &fu1->l.magic );

	for( k = NMG_TBL_END(&vtab)-1; k >= 0; k-- )  {
		register struct vertex		*v;
		register struct vertex_g	*vg;

		v = (struct vertex *)NMG_TBL_GET(&vtab, k);
		NMG_CK_VERTEX(v);
		vg = v->vg_p;
		if( !vg )  rt_bomb("nmg_ck_fu_verts(): vertex with no geometry?\n");
		NMG_CK_VERTEX_G(vg);

		/* Geometry check */
		dist = DIST_PT_PLANE(vg->coord, fg2->N);
		if( dist > tol->dist || dist < (-tol->dist) )  {
			if (rt_g.NMG_debug & DEBUG_MESH)  {
				rt_log("nmg_ck_fu_verts(x%x, x%x) v x%x off face by %e\n",
					fu1, f2,
					v, dist );
				VPRINT(" pt", vg->coord);
				PLPRINT(" fg2", fg2->N);
			}
			count++;
			if( dist < 0.0 )
				dist = (-dist);
			if( dist > worst )  worst = dist;
		}
	}
	nmg_tbl( &vtab, TBL_FREE, 0 );

	if ( count != 0 || rt_g.NMG_debug & DEBUG_BASIC)  {
		rt_log("nmg_ck_fu_verts(fu1=x%x, f2=x%x, tol=%g) f1=x%x, ret=%d, worst=%gmm (%e)\n",
			fu1, f2, tol->dist, fu1->f_p,
			count, worst, worst );
	}

	if( worst > 10.0*tol->dist )
		return count;
	else
		return( 0 );
}

/*			N M G _ C K _ F G _ V E R T S
 *
 * Similar to nmg_ck_fu_verts, but checks all vertices that use the same
 * face geometry as fu1
 *  fu1 and f2 may be the same face, or different.
 *
 *  This is intended to be a geometric check only, not a topology check.
 *  Topology may have become inappropriately shared.
 *
 *  Returns -
 *	0	All is well.
 *	count	Number of verts *not* on fu2's surface.
 *
 */
int
nmg_ck_fg_verts( fu1 , f2 , tol )
struct faceuse *fu1;
struct face *f2;
CONST struct rt_tol *tol;
{
	struct face_g_plane *fg1;
	struct faceuse *fu;
	struct face *f;
	int count=0;

	NMG_CK_FACEUSE( fu1 );
	NMG_CK_FACE( f2 );
	RT_CK_TOL( tol );

	NMG_CK_FACE( fu1->f_p );
	fg1 = fu1->f_p->g.plane_p;
	NMG_CK_FACE_G_PLANE( fg1 );

	for( RT_LIST_FOR( f , face , &fg1->f_hd ) )
	{
		NMG_CK_FACE( f );

		fu = f->fu_p;
		NMG_CK_FACEUSE( fu );

		count += nmg_ck_fu_verts( fu , f2 , tol );
	}

	return( count );
}

/*
 *			N M G _ T W O _ F A C E _ F U S E
 *
 *  XXX A better algorithm would be to compare loop by loop.
 *  If the two faces share all the verts of at least one loop of 3 or more
 *  vertices, then they should be shared.  Otherwise it will be awkward
 *  having shared loop(s) on non-shared faces!!
 *
 *  Compare the geometry of two faces, and fuse them if they are the
 *  same within tolerance.
 *  First compare the plane equations.  If they are "similar" (within tol),
 *  then check all verts in f2 to make sure that they are within tol->dist
 *  of f1's geometry.  If they are, then fuse the face geometry.
 *
 *  Returns -
 *	0	Faces were not fused.
 *	>0	Faces were successfully fused.
 */
int
nmg_two_face_fuse( f1, f2, tol )
struct face	*f1;
struct face	*f2;
CONST struct rt_tol	*tol;
{
	register struct face_g_plane	*fg1;
	register struct face_g_plane	*fg2;
	int			flip2 = 0;
	int			code;

	NMG_CK_FACE(f1);
	NMG_CK_FACE(f2);
	RT_CK_TOL(tol);

	fg1 = f1->g.plane_p;
	fg2 = f2->g.plane_p;

	if( !fg1 || !fg2 )  {
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			rt_log("nmg_two_face_fuse(x%x, x%x) null fg fg1=x%x, fg2=x%x\n",
				f1, f2, fg1, fg2);
		}
		return 0;
	}

	NMG_CK_FACE_G_PLANE(fg1);
	NMG_CK_FACE_G_PLANE(fg2);

	if( fg1 == fg2 )  {
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			rt_log("nmg_two_face_fuse(x%x, x%x) fg already shared\n",
				f1, f2);
		}
		return 0;	/* Already shared */
	}

	/*
	 *  First, a topology check.
	 *  If the two faces share one entire loop (of at least 3 verts)
	 *  according to topology, then by all rights the faces MUST be
	 *  shared.
	 */

	if( fabs(VDOT(fg1->N, fg2->N)) >= 0.99  &&
	    fabs(fg1->N[3]) - fabs(fg2->N[3]) < 100 * tol->dist  &&
	    nmg_is_common_bigloop( f1, f2 ) )  {
		if( VDOT( fg1->N, fg2->N ) < 0 )  flip2 = 1;
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			rt_log("nmg_two_face_fuse(x%x, x%x) faces have a common loop, they MUST be fused.  flip2=%d\n",
				f1, f2, flip2);
		}
		goto must_fuse;
	}

	/* See if faces are coplanar */
	code = rt_coplanar( fg1->N, fg2->N, tol );
	if( code <= 0 )  {
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			rt_log("nmg_two_face_fuse(x%x, x%x) faces non-coplanar\n",
				f1, f2);
		}
		return 0;
	}
	if( code == 1 )
		flip2 = 0;
	else
		flip2 = 1;

	if (rt_g.NMG_debug & DEBUG_MESH)  {
		rt_log("nmg_two_face_fuse(x%x, x%x) coplanar faces, rt_coplanar code=%d, flip2=%d\n",
			f1, f2, code, flip2);
	}

	/*
	 *  Plane equations match, within tol.
	 *  Before conducting a merge, verify that
	 *  all the verts in f2 are within tol->dist
	 *  of f1's surface.
	 */
	if( nmg_ck_fg_verts( f2->fu_p, f1, tol ) != 0 )  {
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			rt_log("nmg_two_face_fuse: f2 verts not within tol of f1's surface, can't fuse\n");
		}
		return 0;
	}

must_fuse:
	/* All points are on the plane, it's OK to fuse */
	if( flip2 == 0 )  {
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			rt_log("joining face geometry (same dir) f1=x%x, f2=x%x\n", f1, f2);
			PLPRINT(" fg1", fg1->N);
			PLPRINT(" fg2", fg2->N);
		}
		nmg_jfg( f1, f2 );
	} else {
		register struct face	*fn;
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			rt_log("joining face geometry (opposite dirs)\n");

			rt_log(" f1=x%x, flip=%d", f1, f1->flip);
			PLPRINT(" fg1", fg1->N);

			rt_log(" f2=x%x, flip=%d", f2, f2->flip);
			PLPRINT(" fg2", fg2->N);
		}
		/* Flip flags of faces using fg2, first! */
		for( RT_LIST_FOR( fn, face, &fg2->f_hd ) )  {
			NMG_CK_FACE(fn);
			fn->flip = !fn->flip;
			if (rt_g.NMG_debug & DEBUG_MESH)  {
				rt_log("f=x%x, new flip=%d\n", fn, fn->flip);
			}
		}
		nmg_jfg( f1, f2 );
	}
	return 1;
}

/*
 *			N M G _ M O D E L _ F A C E _ F U S E
 *
 *  A routine to find all face geometry structures in an nmg model that
 *  have the same plane equation, and have them share face geometry.
 *  (See also nmg_shell_coplanar_face_merge(), which actually moves
 *  the loops into one face).
 *
 *  The criteria for two face geometry structs being the "same" are:
 *	1) The plane equations must be the same, within tolerance.
 *	2) All the vertices on the 2nd face must lie within the
 *	   distance tolerance of the 1st face's plane equation.
 */
int
nmg_model_face_fuse( m, tol )
struct model		*m;
CONST struct rt_tol	*tol;
{
	struct nmg_ptbl	ftab;
	int		total = 0;
	register int	i,j;

	NMG_CK_MODEL(m);
	RT_CK_TOL(tol);

	/* Make a list of all the face structs in the model */
	nmg_face_tabulate( &ftab, &m->magic );

	for( i = NMG_TBL_END(&ftab)-1; i >= 0; i-- )  {
		register struct face	*f1;
		register struct face_g_plane	*fg1;
		f1 = (struct face *)NMG_TBL_GET(&ftab, i);
		NMG_CK_FACE(f1);
		NMG_CK_FACE_G_EITHER(f1->g.magic_p);

		if( *f1->g.magic_p == NMG_FACE_G_SNURB_MAGIC )  {
			/* XXX Need routine to compare 2 snurbs for equality here */
			continue;
		}

		fg1 = f1->g.plane_p;
		NMG_CK_FACE_G_PLANE(fg1);

		/* Check that all the verts of f1 are within tol of face */
		if( nmg_ck_fu_verts( f1->fu_p, f1, tol ) != 0 )  {
			PLPRINT(" f1", f1->g.plane_p->N);
			nmg_pr_fu_briefly(f1->fu_p, 0);
			rt_bomb("nmg_model_face_fuse(): verts not within tol of containing face\n");
		}

		for( j = i-1; j >= 0; j-- )  {
			register struct face	*f2;
			register struct face_g_plane	*fg2;

			f2 = (struct face *)NMG_TBL_GET(&ftab, j);
			NMG_CK_FACE(f2);
			fg2 = f2->g.plane_p;
			if( !fg2 )  continue;
			NMG_CK_FACE_G_PLANE(fg2);

			if( fg1 == fg2 )  continue;	/* Already shared */

			if( nmg_two_face_fuse( f1, f2, tol ) > 0 )
				total++;
		}
	}
	nmg_tbl( &ftab, TBL_FREE, 0 );
	if( rt_g.NMG_debug & DEBUG_BASIC && total > 0 )
		rt_log("nmg_model_face_fuse: %d faces fused\n", total);
	return total;
}

/*
 *			N M G _ M O D E L _ B R E A K _ E _ O N _ V
 *
 *  As the first step in evaluating a boolean formula,
 *  before starting to do face/face intersections, compare every
 *  edge in the model with every vertex in the model.
 *  If the vertex is within tolerance of the edge, break the edge,
 *  and enrole the new edge on a list of edges still to be processed.
 *
 *  A list of edges and a list of vertices are built, and then processed.
 *
 *  Space partitioning could improve the performance of this algorithm.
 *  For the moment, a brute-force approach is used.
 *
 *  Returns -
 *	Number of edges broken.
 */
int
nmg_model_break_e_on_v( m, tol )
struct model			*m;
CONST struct rt_tol		*tol;
{
	int		count = 0;
	struct nmg_ptbl	verts;
	struct nmg_ptbl	edgeuses;
	struct nmg_ptbl	new_edgeuses;
	register struct edgeuse	**eup;

	NMG_CK_MODEL(m);
	RT_CK_TOL(tol);

	nmg_e_and_v_tabulate( &edgeuses, &verts, &m->magic );

	/* Repeat the process until no new edgeuses are created */
	while( NMG_TBL_LEN( &edgeuses ) > 0 )  {
		(void)nmg_tbl( &new_edgeuses, TBL_INIT, 0 );

		for( eup = (struct edgeuse **)NMG_TBL_LASTADDR(&edgeuses);
		     eup >= (struct edgeuse **)NMG_TBL_BASEADDR(&edgeuses);
		     eup--
		)  {
			register struct edgeuse	*eu;
			register struct vertex	*va;
			register struct vertex	*vb;
			register struct vertex	**vp;

			eu = *eup;
			NMG_CK_EDGEUSE(eu);
			va = eu->vu_p->v_p;
			vb = eu->eumate_p->vu_p->v_p;
			NMG_CK_VERTEX(va);
			NMG_CK_VERTEX(vb);
			for( vp = (struct vertex **)NMG_TBL_LASTADDR(&verts);
			     vp >= (struct vertex **)NMG_TBL_BASEADDR(&verts);
			     vp--
			)  {
				register struct vertex	*v;
				fastf_t			dist;
				int			code;
				struct edgeuse		*new_eu;

				v = *vp;
				NMG_CK_VERTEX(v);
				if( va == v )  continue;
				if( vb == v )  continue;

				/* A good candidate for inline expansion */
				code = rt_isect_pt_lseg( &dist,
					va->vg_p->coord,
					vb->vg_p->coord,
					v->vg_p->coord, tol );
				if( code < 1 )  continue;	/* missed */
				if( code == 1 || code == 2 )  {
					rt_log("nmg_model_break_e_on_v() code=%d, why wasn't this vertex fused?\n", code);
					continue;
				}
				/* Break edge on vertex, but don't fuse yet. */
				new_eu = nmg_ebreak( v, eu );
				/* Put new edge into follow-on list */
				nmg_tbl( &new_edgeuses, TBL_INS, &new_eu->l.magic );
				count++;
			}
		}
		nmg_tbl( &edgeuses, TBL_FREE, 0 );
		edgeuses = new_edgeuses;		/* struct copy */
	}
#if 0
	if (rt_g.NMG_debug & (DEBUG_BOOL|DEBUG_BASIC) )
#endif
		rt_log("nmg_model_break_e_on_v() broke %d edges\n", count);
	return count;
}

/*
 *			N M G _ M O D E L _ F U S E
 *
 *  This is the primary application interface to the geometry fusing support.
 *  Fuse together all data structures that are equal to each other,
 *  within tolerance.
 *
 *  The algorithm is three part:
 *	1)  Fuse together all vertices.
 *	2)  Fuse together all face geometry, where appropriate.
 *	3)  Fuse together all edges.
 *
 *  Edge fusing is handled last, because the difficult part there is
 *  sorting faces radially around the edge.
 *  It is important to know whether faces are shared or not
 *  at that point.
 *
 *  XXX It would be more efficient to build all the ptbl's at once,
 *  XXX with a single traversal of the model.
 */
int
nmg_model_fuse( m, tol )
struct model		*m;
CONST struct rt_tol	*tol;
{
	int	total = 0;

	NMG_CK_MODEL(m);
	RT_CK_TOL(tol);

	/* Step 1 -- the vertices. */
	total += nmg_model_vertex_fuse( m, tol );

	/* Step 2 -- the face geometry */
	total += nmg_model_face_fuse( m, tol );

	/* Step 2.5 -- break edges on vertices, before fusing edges */
	total += nmg_model_break_e_on_v( m, tol );

	/* Step 3 -- edges */
	total += nmg_model_edge_fuse( m, tol );

	/* Step 4 -- edge geometry */
	total += nmg_model_edge_g_fuse( m, tol );

	if( rt_g.NMG_debug & DEBUG_BASIC && total > 0 )
		rt_log("nmg_model_fuse(): %d entities fused\n", total);
	return total;
}


/* -------------------- RADIAL -------------------- */

struct nmg_radial {
	struct rt_list	l;
	struct edgeuse	*eu;
	struct faceuse	*fu;		/* Derrived from eu */
	struct shell	*s;		/* Derrived from eu */
	int		existing_flag;	/* !0 if this eu exists on dest edge */
	int		is_crack;	/* This eu is part of a crack. */
	int		needs_flip;	/* Insert eumate, not eu */
	fastf_t		ang;		/* angle, in radians.  0 to 2pi */
};
#define NMG_RADIAL_MAGIC	0x52614421	/* RaD! */
#define NMG_CK_RADIAL(_p)	NMG_CKMAG(_p, NMG_RADIAL_MAGIC, "nmg_radial")

/*
 *			N M G _ R A D I A L _ S O R T E D _ L I S T _ I N S E R T
 *
 *  Build sorted list, with 'ang' running from zero to 2*pi.
 *  New edgeuses with same angle as an edgeuse already on the list
 *  are added AFTER the existing one.
 *
 *  ??? Should this perhaps build the list in descending order, so
 *  ??? as to match the way that nmg_pr_fu_around_eu_vecs() works?
 */
void
nmg_radial_sorted_list_insert( hd, rad )

struct rt_list		*hd;
struct nmg_radial	*rad;
{
	struct nmg_radial	*cur;
	register fastf_t	rad_ang;

	RT_CK_LIST_HEAD(hd);
	NMG_CK_RADIAL(rad);

	if(RT_LIST_IS_EMPTY(hd))  {
		RT_LIST_APPEND( hd, &rad->l );
		return;
	}
	rad_ang = rad->ang;

	/*  Check for trivial append at end of list.
	 *  This is a very common case, when input list is sorted.
	 */
	cur = RT_LIST_PREV(nmg_radial, hd);
	if( rad_ang >= cur->ang )  {
		RT_LIST_INSERT( hd, &rad->l );
		return;
	}

	/* Brute force search through hd's list, going backwards */
	for( RT_LIST_FOR_BACKWARDS( cur, nmg_radial, hd ) )  {
		if( rad_ang >= cur->ang )  {
			RT_LIST_APPEND( &cur->l, &rad->l );
			return;
		}
	}

	/* Add before first element */
	RT_LIST_APPEND( hd, &rad->l );
}

/*
 *			N M G _ R A D I A L _ B U I L D _ L I S T
 */
void
nmg_radial_build_list( hd, shell_tbl, existing, eu, xvec, yvec, zvec, tol )
struct rt_list		*hd;
struct nmg_ptbl		*shell_tbl;
int			existing;
struct edgeuse		*eu;
CONST vect_t		xvec;
CONST vect_t		yvec;
CONST vect_t		zvec;
CONST struct rt_tol	*tol;		/* for printing */
{
	fastf_t			angle;
	struct edgeuse		*teu;
	struct nmg_radial	*rad;

	RT_CK_LIST_HEAD(hd);
	NMG_CK_PTBL(shell_tbl);
	NMG_CK_EDGEUSE(eu);
	RT_CK_TOL(tol);

	teu = eu;
	for(;;)  {
		GETSTRUCT( rad, nmg_radial );
		rad->l.magic = NMG_RADIAL_MAGIC;
		rad->eu = teu;
		/* We depend on ang being strictly in the range 0..2pi */
		rad->ang = nmg_measure_fu_angle( teu, xvec, yvec, zvec );
		rad->fu = nmg_find_fu_of_eu( teu );
		rad->s = nmg_find_s_of_eu( teu );
		rad->existing_flag = existing;
		rad->needs_flip = 0;	/* not yet determined */
		rad->is_crack = 0;	/* not yet determined */
		nmg_radial_sorted_list_insert( hd, rad );

		/* Build list of all shells involved at this edge */
		nmg_tbl( shell_tbl, TBL_INS_UNIQUE, &(rad->s->l.magic) );

		/* Advance to next edgeuse pair */
		teu = teu->radial_p->eumate_p;
		if( teu == eu )  break;
	}
}

/*
 *			N M G _ R A D I A L _ M E R G E _ L I S T S
 *
 *  Merge all of the src list into the dest list, sorting by angles.
 */
void
nmg_radial_merge_lists( dest, src, tol )
struct rt_list		*dest;
struct rt_list		*src;
CONST struct rt_tol	*tol;
{
	struct nmg_radial	*rad;

	RT_CK_LIST_HEAD(dest);
	RT_CK_LIST_HEAD(src);
	RT_CK_TOL(tol);

	while( RT_LIST_WHILE( rad, nmg_radial, src ) )  {
		RT_LIST_DEQUEUE( &rad->l );
		nmg_radial_sorted_list_insert( dest, rad );
	}
}

/*
 *			N M G _ R A D I A L _ M A R K _ C R A C K S
 *
 *  For every edgeuse, if there are other edgeuses around this edge
 *  from the same face, then mark them all as part of a "crack".
 *
 *  XXX This approach is overly simplistic, as a legal face can have
 *  an even number of crack-pairs, and 0 to 2 non-crack edges, i.e.
 *  edges which border actual surface area.
 *  Telling the difference takes real work.
 *
 *  XXX Worse still, this approach does not distinguish between
 *  an edge cutting across a face, -vs- a pair of crack edges. :-(
 *  Good thing this flag bit isn't used for anything yet!
 */
void
nmg_radial_mark_cracks( hd, tol )
struct rt_list		*hd;
CONST struct rt_tol	*tol;
{
	struct nmg_radial	*rad;
	struct nmg_radial	*other;

	RT_CK_LIST_HEAD(hd);
	RT_CK_TOL(tol);

	for( RT_LIST_FOR( rad, nmg_radial, hd ) )  {
		NMG_CK_RADIAL(rad);
		if( rad->is_crack )  continue;

		other = RT_LIST_PNEXT( nmg_radial, rad );
		while( RT_LIST_NOT_HEAD( other, hd ) )  {
			if( other->fu->f_p == rad->fu->f_p )  {
				rad->is_crack = 1;
				other->is_crack = 1;
				/* And the search continues to end of list */
			}
			other = RT_LIST_PNEXT( nmg_radial, other );
		}
	}

	/* XXX Go back and un-mark any edgeuses which acutally border surface area */
}

/*
 *			N M G _ R A D I A L _ F I N D _ A N _ O R I G I N A L
 *
 *  Returns -
 *	NULL		No edgeuses from indicated shell on this list
 *	nmg_radial*	An original, else first newbie, else a newbie crack.
 */
struct nmg_radial *
nmg_radial_find_an_original( hd, s, tol )
struct rt_list		*hd;
CONST struct shell	*s;
CONST struct rt_tol	*tol;
{
	register struct nmg_radial	*rad;
	struct nmg_radial	*fallback = (struct nmg_radial *)NULL;
	struct nmg_radial	*wire = (struct nmg_radial *)NULL;
	int			seen_shell = 0;

	RT_CK_LIST_HEAD(hd);
	NMG_CK_SHELL(s);

	for( RT_LIST_FOR( rad, nmg_radial, hd ) )  {
		NMG_CK_RADIAL(rad);
		if( rad->s != s )  continue;
		seen_shell++;
		if( rad->is_crack )  continue;	/* skip cracks */
		if( !rad->fu )  continue;	/* skip wires */
		if( rad->existing_flag )  return rad;
	}
	if( !seen_shell )  return (struct nmg_radial *)NULL;	/* No edgeuses from that shell, at all! */

	/* If there were no originals, find first newbie */
	for( RT_LIST_FOR( rad, nmg_radial, hd ) )  {
		if( rad->s != s )  continue;
		if( rad->is_crack )  {
			fallback = rad;
			continue;	/* skip cracks */
		}
		if( !rad->fu )  {
			continue;	/* skip wires */
		}
		return rad;
	}
	/* If no ordinary newbies, provide a newbie crack */
	if(fallback) return fallback;

	/* No ordinary newbiew or newbie cracks, any wires? */
	for( RT_LIST_FOR( rad, nmg_radial, hd ) )  {
		if( rad->s != s )  continue;
		if( rad->is_crack )  {
			continue;	/* skip cracks */
		}
		if( !rad->fu )  {
			fallback = rad;
			continue;	/* skip wires */
		}
		return rad;
	}
	if(fallback) return fallback;

	rt_log("nmg_radial_find_an_original() shell=x%x\n", s);
	nmg_pr_radial_list( hd, tol );
	rt_bomb("nmg_radial_find_an_original() No entries from indicated shell\n");
}

/*
 *			N M G _ R A D I A L _ M A R K _ F L I P S
 *
 *  For each shell, find an original edgeuse from that shell,
 *  and then mark parity violators with a "flip" flag.
 */
void
nmg_radial_mark_flips( hd, shells, tol )
struct rt_list		*hd;
CONST struct nmg_ptbl	*shells;
CONST struct rt_tol	*tol;
{
	struct nmg_radial	*rad;
	struct shell		**sp;
	struct nmg_radial	*orig;
	register int		expected_ot;

	RT_CK_LIST_HEAD(hd);
	NMG_CK_PTBL(shells);
	RT_CK_TOL(tol);

	for( sp = (struct shell **)NMG_TBL_LASTADDR(shells);
 	     sp >= (struct shell **)NMG_TBL_BASEADDR(shells); sp-- 
	)  {
		int	count = 1;

		NMG_CK_SHELL(*sp);
		orig = nmg_radial_find_an_original( hd, *sp, tol );
		NMG_CK_RADIAL(orig);
		if( !orig->existing_flag )  {
			/* There were no originals.  Do something sensible to check the newbies */
			if( !orig->fu )  continue;	/* Nothing but wires */
#if 0
			/*  Given that there were no existing edgeuses
			 *  from this shell, make our first addition
			 *  oppose the previous radial faceuse, just to be nice.
			 */
			rad = RT_LIST_PPREV_CIRC( nmg_radial, orig );
			/* Hopefully it isn't a wire */
			if( rad->eu->vu_p->v_p == orig->eu->vu_p->v_p )  {
				/* Flip everything, for general compatibility */
				for( RT_LIST_FOR( rad, nmg_radial, hd ) )  {
					if( rad->s != *sp )  continue;
					if( !rad->fu )  continue;	/* skip wires */
					rad->needs_flip = !rad->needs_flip;
				}
			}
#endif
		}
		expected_ot = !(orig->fu->orientation == OT_SAME);

		for( RT_LIST_FOR_CIRC( rad, nmg_radial, orig ) )  {
			if( rad->s != *sp )  continue;
			if( !rad->fu )  continue;	/* skip wires */
			count++;
			if( expected_ot == (rad->fu->orientation == OT_SAME) )  {
				expected_ot = !expected_ot;
				continue;
			}
			/* Mis-match detected */
			rt_log("nmg_radial_mark_flips() Mis-match detected, setting flip flag eu=x%x\n", rad->eu);
			rad->needs_flip = !rad->needs_flip;
			/* With this one flipped, set expectation for next */
			expected_ot = !expected_ot;
		}
		if( expected_ot == (orig->fu->orientation == OT_SAME) )
			continue;
		rt_log("nmg_radial_mark_flips() unable to establish proper orientation parity.\n  eu count=%d, shell=x%x, expectation=%d\n",
			count, *sp, expected_ot);
		rt_bomb("nmg_radial_mark_flips() unable to establish proper orientation parity.\n");
 	}
}

/*
 *			N M G _ R A D I A L _ C H E C K _ P A R I T Y
 *
 *  For each shell, check orientation parity of edgeuses within that shell.
 */
int
nmg_radial_check_parity( hd, shells, tol )
CONST struct rt_list	*hd;
CONST struct nmg_ptbl	*shells;
CONST struct rt_tol	*tol;
{
	struct nmg_radial	*rad;
	struct nmg_radial	*other;
	struct shell		**sp;
	struct nmg_radial	*orig;
	register int		expected_ot;
	int			count = 0;

	RT_CK_LIST_HEAD(hd);
	NMG_CK_PTBL(shells);
	RT_CK_TOL(tol);

	for( sp = (struct shell **)NMG_TBL_LASTADDR(shells);
 	     sp >= (struct shell **)NMG_TBL_BASEADDR(shells); sp-- 
	)  {

		NMG_CK_SHELL(*sp);
		orig = nmg_radial_find_an_original( hd, *sp, tol );
		if( !orig )  continue;
		NMG_CK_RADIAL(orig);
		if( !orig->existing_flag )  {
			/* There were no originals.  Do something sensible to check the newbies */
			if( !orig->fu )  continue;	/* Nothing but wires */
		}
		expected_ot = !(orig->fu->orientation == OT_SAME);

		for( RT_LIST_FOR_CIRC( rad, nmg_radial, orig ) )  {
			if( rad->s != *sp )  continue;
			if( !rad->fu )  continue;	/* skip wires */
			if( expected_ot == (rad->fu->orientation == OT_SAME) )  {
				expected_ot = !expected_ot;
				continue;
			}
			/* Mis-match detected */
			rt_log("nmg_radial_check_parity() bad parity eu=x%x, s=x%x\n",
				rad->eu, *sp);
			count++;
			/* Set expectation for next */
			expected_ot = !expected_ot;
		}
		if( expected_ot == (orig->fu->orientation == OT_SAME) )
			continue;
		rt_log("nmg_radial_check_parity() bad parity at END eu=x%x, s=x%x\n",
			rad->eu, *sp);
		count++;
 	}
	return count;
}

/*
 *			N M G _ R A D I A L _ I M P L E M E N T _ D E C I S I O N S
 *
 *  For all non-original edgeuses in the list, place them in the proper
 *  place around the destination edge.
 */
void
nmg_radial_implement_decisions( hd, tol )
struct rt_list		*hd;
CONST struct rt_tol	*tol;		/* for printing */
{
	struct nmg_radial	*rad;
	struct nmg_radial	*prev;
	int			skipped;

	RT_CK_LIST_HEAD(hd);
	RT_CK_TOL(tol);

again:
	skipped = 0;
	for( RT_LIST_FOR( rad, nmg_radial, hd ) )  {
		if( rad->existing_flag )  continue;
		prev = RT_LIST_PPREV_CIRC( nmg_radial, rad );
		if( !prev->existing_flag )  {
			/* Previous eu isn't in place yet, can't do this one until next pass. */
			skipped++;
			continue;
		}
		/*
		 *  Make rad radial to prev, inserting between
		 *  prev->eu->eumate_p and prev->eu->eumate_p->radial_p.
		 */
		if( rad->needs_flip )  {
			nmg_je( prev->eu->eumate_p, rad->eu->eumate_p );
			rad->eu = rad->eu->eumate_p;
			rad->fu = nmg_find_fu_of_eu( rad->eu );
			rad->needs_flip = 0;
		} else {
			nmg_je( prev->eu->eumate_p, rad->eu );
		}
		rad->existing_flag = 1;
	}
	if( skipped )  goto again;
}

/*
 *			N M G _ P R _ R A D I A L _ L I S T
 *
 *  Patterned after nmg_pr_fu_around_eu_vecs(), with similar format.
 */
void
nmg_pr_radial_list( hd, tol )
CONST struct rt_list	*hd;
CONST struct rt_tol	*tol;		/* for printing */
{
	struct nmg_radial	*rad;

	RT_CK_LIST_HEAD(hd);
	RT_CK_TOL(tol);

	rt_log("nmg_pr_radial_list( hd=x%x )\n", hd);

	for( RT_LIST_FOR( rad, nmg_radial, hd ) )  {
		NMG_CK_RADIAL(rad);
		rt_log(" %8.8x, f=%8.8x, fu=%8.8x=%c, s=%8.8x %s %c%c %g deg\n",
			rad->eu, rad->fu->f_p, rad->fu,
			nmg_orientation(rad->fu->orientation)[3],
			rad->s,
			rad->existing_flag ? "old" : "new",
			rad->is_crack ? 'C' : '/',
			rad->needs_flip ? 'F' : '/',
			rad->ang * rt_radtodeg
		);
	}
}

/*
 *			N M G _ R A D I A L _ J O I N _ E U
 *
 *  A new routine, that uses "global information" about the edge
 *  to plan the operations to be performed.
 */
void
nmg_radial_join_eu_NEW(eu1, eu2, tol)
struct edgeuse		*eu1;
struct edgeuse		*eu2;
CONST struct rt_tol	*tol;
{
	struct edge_g_lseg	*best_eg;
	struct faceuse		*fu1;
	struct faceuse		*fu2;
	struct nmg_radial	*rad;
	vect_t			xvec, yvec, zvec;
	struct rt_list		list1;
	struct rt_list		list2;
	struct nmg_ptbl		shell_tbl;
	int			count1, count2;

	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);
	RT_CK_TOL(tol);

	if( eu1->e_p == eu2->e_p )  return;

	if( !NMG_ARE_EUS_ADJACENT(eu1, eu2) )
		rt_bomb("nmg_radial_join_eu_NEW() edgeuses don't share vertices.\n");

	if( eu1->vu_p->v_p == eu1->eumate_p->vu_p->v_p )  rt_bomb("nmg_radial_join_eu_NEW(): 0 length edge (topology)\n");

	if( rt_pt3_pt3_equal( eu1->vu_p->v_p->vg_p->coord,
	    eu1->eumate_p->vu_p->v_p->vg_p->coord, tol ) )
		rt_bomb("nmg_radial_join_eu_NEW(): 0 length edge (geometry)\n");

	/* Ensure faces are of same orientation, if both eu's have faces */
	fu1 = nmg_find_fu_of_eu(eu1);
	fu2 = nmg_find_fu_of_eu(eu2);
	if( fu1 && fu2 )  {
		if( fu1->orientation != fu2->orientation ){
			eu2 = eu2->eumate_p;
			fu2 = nmg_find_fu_of_eu(eu2);
			if( fu1->orientation != fu2->orientation )
				rt_bomb( "nmg_radial_join_eu_NEW(): Cannot find matching orientations for faceuses\n" );
		}
	}

	/* XXX This angle-based algorithm can't yet handle snurb faces! */
	if( fu1 && fu1->f_p->g.magic_p && *fu1->f_p->g.magic_p == NMG_FACE_G_SNURB_MAGIC )  return;
	if( fu2 && fu2->f_p->g.magic_p && *fu2->f_p->g.magic_p == NMG_FACE_G_SNURB_MAGIC )  return;

	/*  Construct local coordinate system for this edge,
	 *  so all angles can be measured relative to a common reference.
	 */
	nmg_eu_2vecs_perp( xvec, yvec, zvec, eu1, tol );

	/* Build the primary lists that describe the situation */
	RT_LIST_INIT( &list1 );
	RT_LIST_INIT( &list2 );
	nmg_tbl( &shell_tbl, TBL_INIT, 0 );

	nmg_radial_build_list( &list1, &shell_tbl, 1, eu1, xvec, yvec, zvec, tol );
	nmg_radial_build_list( &list2, &shell_tbl, 0, eu2, xvec, yvec, zvec, tol );

#if 0
	/* OK so far? */
	nmg_pr_radial_list( &list1, tol );
	nmg_pr_radial_list( &list2, tol );
	nmg_pr_ptbl( "Participating shells", &shell_tbl, 1 );
#endif

	if (rt_g.NMG_debug & DEBUG_MESH_EU ) {
		rt_log("nmg_radial_join_eu_NEW(eu1=x%x, eu2=x%x) e1=x%x, e2=x%x\n",
			eu1, eu2,
			eu1->e_p, eu2->e_p);
		nmg_euprint("\tJoining", eu1);
		nmg_euprint("\t     to", eu2);
		rt_log( "Faces around eu1:\n" );
		nmg_pr_fu_around_eu_vecs( eu1, xvec, yvec, zvec, tol );
		nmg_pr_radial_list( &list1, tol );

		rt_log( "Faces around eu2:\n" );
		nmg_pr_fu_around_eu_vecs( eu2, xvec, yvec, zvec, tol );
		nmg_pr_radial_list( &list2, tol );
		nmg_pr_ptbl( "Participating shells", &shell_tbl, 1 );
	}

	count1 = nmg_radial_check_parity( &list1, &shell_tbl, tol );
	count2 = nmg_radial_check_parity( &list2, &shell_tbl, tol );
	if( count1 || count2 ) rt_log("nmg_radial_join_eu_NEW() bad parity at the outset, %d, %d\n", count1, count2);

	best_eg = nmg_pick_best_edge_g( eu1, eu2, tol );

	/* Merge the two lists, sorting by angles */
	nmg_radial_merge_lists( &list1, &list2, tol );
	nmg_radial_mark_cracks( &list1, tol );

	nmg_radial_mark_flips( &list1, &shell_tbl, tol );
#if 0
rt_log("marked list:\n");
rt_log("  edge: %g %g %g -> %g %g %g\n",
	V3ARGS(eu1->vu_p->v_p->vg_p->coord), V3ARGS(eu1->eumate_p->vu_p->v_p->vg_p->coord) );
nmg_pr_radial_list( &list1, tol );
#endif

	nmg_radial_implement_decisions( &list1, tol );

#if 0
	/* How did it come out? */
	nmg_pr_radial_list( &list1, tol );
	nmg_pr_fu_around_eu_vecs( eu1, xvec, yvec, zvec, tol );
#endif
	count1 = nmg_radial_check_parity( &list1, &shell_tbl, tol );
	if( count1 )  rt_log("nmg_radial_join_eu_NEW() bad parity at completion, %d\n", count1);

	/* Ensure that all edgeuses are using the "best_eg" line */
	for( RT_LIST_FOR( rad, nmg_radial, &list1 ) )  {
		if( rad->eu->g.lseg_p != best_eg )  {
			nmg_use_edge_g( rad->eu, &best_eg->magic );
		}
	}

	/* Clean up */
	nmg_tbl( &shell_tbl, TBL_FREE, 0 );
	while( RT_LIST_WHILE( rad, nmg_radial, &list1 ) )  {
		RT_LIST_DEQUEUE( &rad->l );
		rt_free( (char *)rad, "nmg_radial" );
	}
}
