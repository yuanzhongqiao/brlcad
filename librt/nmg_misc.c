/*
 *			N M G _ M I S C . C
 *
 *	As the name implies, these are miscellaneous routines that work with
 *	the NMG structures.
 *
 *
 *  Authors -
 *	John R. Anderson
 *	Lee A. Butler
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

#include <stdio.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif
#include "machine.h"
#include "vmath.h"
#include "externs.h"
#include "nmg.h"
#include "raytrace.h"

#include "db.h"		/* for debugging stuff at bottom */

/*	N M G _ L O O P _ P L A N E _ A R E A
 *
 *  Calculates a plane equation and the area of a loop
 *
 *  returns:
 *	the area of the loop
 *	less tha zero indicates an error
 *
 *  pl is assigned the plane equation for the loop
 *
 */

fastf_t
nmg_loop_plane_area( lu , pl )
CONST struct loopuse *lu;
plane_t pl;
{
	fastf_t area;
	fastf_t vect_mag;
	fastf_t vect_mag_inv;
	fastf_t pt_dot_plane=0.0;
	fastf_t pt_count=0.0;
	plane_t plane;
	struct edgeuse *eu;
	vect_t trans;

	NMG_CK_LOOPUSE( lu );

	/* make sure we have a loop of edges */
	if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		return( (fastf_t)(-1.0) );

	/* check if this loop is a crack */
	if( nmg_loop_is_a_crack( lu ) )
		return( (fastf_t)(-1.0) );

	/* calculate a translation to put one vertex at the origin
	 * not necessary, but good for accuracy.
	 * Also, origin must be in plane of loop for this
	 * method to work
	 */
	eu = RT_LIST_FIRST( edgeuse , &lu->down_hd );
	NMG_CK_VERTEXUSE( eu->vu_p );
	NMG_CK_VERTEX( eu->vu_p->v_p );
	NMG_CK_VERTEX_G( eu->vu_p->v_p->vg_p );
	VMOVE( trans , eu->vu_p->v_p->vg_p->coord );

	VSET( plane , 0.0 , 0.0 , 0.0 );

	/* Calculate area and plane normal.
	 * Cross product of each pair of vertices gives twice
	 * the area of the triangle formed by the origin and
	 * the two vertices. (positive if counter-clockwise,
	 * negative if clockwise). In counter_clockwise case,
	 * sum of all cross products around loop adds area for
	 * edges away from origin and subtracts area for edges
	 * near origin, leaving twice the area of the polygon.
	 */
	for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
	{
		struct edgeuse *next_eu;
		struct vertex *vp,*next_vp;
		vect_t cross;
		point_t p1,p2;

		vp = eu->vu_p->v_p;

		next_eu = RT_LIST_PNEXT_CIRC( edgeuse , &eu->l );
		NMG_CK_EDGEUSE( next_eu );
		NMG_CK_VERTEXUSE( next_eu->vu_p );

		next_vp = next_eu->vu_p->v_p;
		NMG_CK_VERTEX( next_vp );
		NMG_CK_VERTEX_G( next_vp->vg_p );

		VSUB2( p1 , vp->vg_p->coord , trans );
		VSUB2( p2 , next_vp->vg_p->coord , trans );
		VCROSS( cross , p1 , p2 );
		VADD2( plane , plane , cross );
	}

	vect_mag = MAGNITUDE( plane );

	/* Error if the area is too small to unitize the normal vector */
	if( vect_mag < SMALL_FASTF )
		return( (fastf_t)(-1.0) );

	area = 0.5 * vect_mag;
	vect_mag_inv = 1.0/vect_mag;

	VSCALE( plane , plane , vect_mag_inv );

	/* calculate plane[3] as average distance to plane */
	for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
	{
		pt_dot_plane += VDOT( plane , eu->vu_p->v_p->vg_p->coord );
		pt_count++;
	}

	/* Error if we don't have at least 3 vertices in this loop */
	if( pt_count < 3 )
		return( (fastf_t)(-1.0) );

	plane[3] = pt_dot_plane/pt_count;
	HMOVE( pl , plane );

	return( area );
}

/*	R T _ D I S T _ L I N E 3 _ L I N E 3
 *
 *  Calculate closest approach of two lines
 *
 *  returns:
 *	-2 -> lines are parallel and do not intersect
 *	-1 -> lines are parallel and collinear
 *	 0 -> lines intersect
 *	 1 -> lines do not intersect
 *  For return values less than zero, dist is not set.
 *  For return valuse of 0 or 1, dist[0] is the distance
 *  from p1 in the d1 direction to the point of closest
 *  approach for that line.  Similar for the second line.
 *  d1 and d2 must be unit direction vectors.
 */

int
rt_dist_line3_line3( dist , p1 , d1 , p2 , d2 , tol )
fastf_t dist[2];
CONST point_t p1,p2;
CONST vect_t d1,d2;
CONST struct rt_tol *tol;
{
	fastf_t d1_d2;
	point_t a1,a2;
	vect_t a1_to_a2;
	vect_t p2_to_p1;
	fastf_t min_dist;

	RT_CK_TOL( tol );

	d1_d2 = VDOT( d1 , d2 );

	if( RT_VECT_ARE_PARALLEL( d1_d2 , tol ) )
	{
		if( rt_dist_line_point( p1 , d1 , p2 ) > tol->dist )
			return( -2 ); /* parallel, but not collinear */
		else
			return( -1 ); /* parallel and collinear */
	}

	VSUB2( p2_to_p1 , p1 , p2 );
	dist[0] = (d1_d2 * VDOT( p2_to_p1 , d2 ) - VDOT( p2_to_p1 , d1 ))/(1.0 - d1_d2 * d1_d2 );
	dist[1] = dist[0] * d1_d2 + VDOT( p2_to_p1 , d2 );

	VJOIN1( a1 , p1 , dist[0] , d1 );
	VJOIN1( a2 , p2 , dist[1] , d2 );

	VSUB2( a1_to_a2 , a2 , a1 );
	min_dist = MAGNITUDE( a1_to_a2 );
	if( min_dist < tol->dist )
		return( 0 );
	else
		return( 1 );
}

/*	R T _ D I S T _ L I N E 3 _ L S E G 3
 *
 *  calculate intersection or closest approach of
 *  a line and a line segement.
 *
 *  returns:
 *	-2 -> line and line segment are parallel and collinear.
 *	-1 -> line and line segment are parallel, not collinear.
 *	 0 -> intersection between points a and b.
 *	 1 -> intersection outside a and b.
 *	 2 -> closest approach is between a and b.
 *	 3 -> closest approach is outside a and b.
 *
 *  dist[0] is actual distance from p in d direction to
 *  closest portion of segment.
 *  dist[1] is ratio of distance from a to b (0.0 at a, and 1.0 at b),
 *  dist[1] may be less than 0 or greater than 1.
 *  For return values less than 0, closest approach is defined as
 *  closest to point p.
 *  Direction vector, d, must be unit length.
 */

int
rt_dist_line3_lseg3( dist , p , d , a , b , tol )
fastf_t dist[2];
CONST point_t p;
CONST vect_t d;
CONST point_t a,b;
CONST struct rt_tol *tol;
{
	vect_t a_to_b;
	vect_t a_dir;
	fastf_t len_ab;
	int outside_segment;
	int ret;

	RT_CK_TOL( tol );

	VSUB2( a_to_b , b , a );
	len_ab = MAGNITUDE( a_to_b );
	VSCALE( a_dir , a_to_b , (1.0/len_ab) );

	ret = rt_dist_line3_line3( dist , p , d , a , a_dir , tol );

	if( ret < 0 )
	{
		vect_t to_a,to_b;
		fastf_t dist_to_a,dist_to_b;

		VSUB2( to_a , a , p );
		VSUB2( to_b , b , p );
		dist_to_a = VDOT( to_a , d );
		dist_to_b = VDOT( to_b , d );

		if( dist_to_a <= dist_to_b )
		{
			dist[0] = dist_to_a;
			dist[1] = 0.0;
		}
		else
		{
			dist[0] = dist_to_b;
			dist[1] = 1.0;
		}
		return( ret );
	}

	if( dist[1] >= (-tol->dist) && dist[1] <= len_ab + tol->dist )
	{
		/* intersect or closest approach between a and b */
		outside_segment = 0;
		dist[1] = dist[1]/len_ab;
		if( dist[1] < 0.0 )
			dist[1] = 0.0;
		if( dist[1] > 1.0 )
			dist[1] = 1.0;
	}
	else
	{
		outside_segment = 1;
		dist[1] = dist[1]/len_ab;
	}

	return( 2*ret + outside_segment );
}

/*	N M G _ T B L
 *	maintain a table of pointers (to magic numbers/structs)
 */
int
nmg_tbl(b, func, p)
struct nmg_ptbl *b;
int func;
long *p;
{
	if (func == TBL_INIT) {
		b->magic = NMG_PTBL_MAGIC;
		b->blen=64;
		b->buffer = (long **)rt_calloc(b->blen, sizeof(long *),
			"nmg_ptbl.buffer[]");
		b->end = 0;
		if (rt_g.NMG_debug & DEBUG_INS)
			rt_log("nmg_tbl(%8x) TBL_INIT\n", b);
		return(0);
	} else if (func == TBL_RST) {
		NMG_CK_PTBL(b);
		b->end = 0;
		if (rt_g.NMG_debug & DEBUG_INS)
			rt_log("nmg_tbl(%8x) TBL_RST\n", b);
		return(0);
	} else if (func == TBL_INS) {
		register int i;
		union {
			struct loopuse *lu;
			struct edgeuse *eu;
			struct vertexuse *vu;
			long *l;
		} pp;

		if (rt_g.NMG_debug & DEBUG_INS)
			rt_log("nmg_tbl(%8x) TBL_INS %8x\n", b, p);

		NMG_CK_PTBL(b);
		pp.l = p;

		if (b->blen == 0) (void)nmg_tbl(b, TBL_INIT, p);
		if (b->end >= b->blen)
			b->buffer = (long **)rt_realloc( (char *)b->buffer,
			    sizeof(p)*(b->blen *= 4),
			    "pointer table" );

		b->buffer[i=b->end++] = p;
		return(i);
	} else if (func == TBL_LOC) {
		/* we do this a great deal, so make it go as fast as possible.
		 * this is the biggest argument I can make for changing to an
		 * ordered list.  Someday....
		 */
		register int	k;
		register long	**pp = b->buffer;

		NMG_CK_PTBL(b);
#		include "noalias.h"
		for( k = b->end-1; k >= 0; k-- )
			if (pp[k] == p) return(k);

		return(-1);
	} else if (func == TBL_INS_UNIQUE) {
		/* we do this a great deal, so make it go as fast as possible.
		 * this is the biggest argument I can make for changing to an
		 * ordered list.  Someday....
		 */
		register int	k;
		register long	**pp = b->buffer;

		NMG_CK_PTBL(b);
#		include "noalias.h"
		for( k = b->end-1; k >= 0; k-- )
			if (pp[k] == p) return(k);

		if (b->blen <= 0 || b->end >= b->blen)  {
			/* Table needs to grow */
			return( nmg_tbl( b, TBL_INS, p ) );
		}

		if (rt_g.NMG_debug & DEBUG_INS)
			rt_log("nmg_tbl(%8x) TBL_INS_UNIQUE %8x\n", b, p);

		b->buffer[k=b->end++] = p;
		return(-1);		/* To signal that it was added */
	} else if (func == TBL_RM) {
		/* we go backwards down the list looking for occurrences
		 * of p to delete.  We do it backwards to reduce the amount
		 * of data moved when there is more than one occurrence of p
		 * in the list.  A pittance savings, unless you're doing a
		 * lot of it.
		 */
		register int end = b->end, j, k, l;
		register long **pp = b->buffer;

		NMG_CK_PTBL(b);
		if (rt_g.NMG_debug & DEBUG_INS)
			rt_log("nmg_tbl(%8x) TBL_RM %8x\n", b, p);
		for (l = b->end-1 ; l >= 0 ; --l)
			if (pp[l] == p){
				/* delete occurrence(s) of p */

				j=l+1;
				while (pp[l-1] == p) --l;

				end -= j - l;
#				include "noalias.h"
				for(k=l ; j < b->end ;)
					b->buffer[k++] = b->buffer[j++];
				b->end = end;
			}
		return(0);
	} else if (func == TBL_CAT) {
		union {
			long *l;
			struct nmg_ptbl *t;
		} d;

		NMG_CK_PTBL(b);
		d.l = p;
		NMG_CK_PTBL(d.t);
		if (rt_g.NMG_debug & DEBUG_INS)
			rt_log("nmg_tbl(%8x) TBL_CAT %8x\n", b, p);

		if ((b->blen - b->end) < d.t->end) {
			
			b->buffer = (long **)rt_realloc( (char *)b->buffer,
				sizeof(p)*(b->blen += d.t->blen),
				"pointer table (CAT)");
		}
		bcopy(d.t->buffer, &b->buffer[b->end], d.t->end*sizeof(p));
		return(0);
	} else if (func == TBL_FREE) {
		NMG_CK_PTBL(b);
		bzero((char *)b->buffer, b->blen * sizeof(p));
		rt_free((char *)b->buffer, "pointer table");
		bzero(b, sizeof(struct nmg_ptbl));
		if (rt_g.NMG_debug & DEBUG_INS)
			rt_log("nmg_tbl(%8x) TBL_FREE\n", b);
		return (0);
	} else {
		NMG_CK_PTBL(b);
		rt_log("nmg_tbl(%8x) Unknown table function %d\n", b, func);
		rt_bomb("nmg_tbl");
	}
	return(-1);/* this is here to keep lint happy */
}




/* N M G _ P U R G E _ U N W A N T E D _ I N T E R S E C T I O N _ P O I N T S
 *
 *	Make sure that the list of intersection points doesn't contain
 *	any vertexuses from loops whose bounding boxes don;t overlap the
 *	bounding box of a loop in the given faceuse.
 *
 *	This is really a special purpose routine to help the intersection
 *	operations of the boolean process.  The only reason it's here instead
 *	of nmg_inter.c is that it knows too much about the format and contents
 *	of an nmg_ptbl structure.
 */
void
nmg_purge_unwanted_intersection_points(vert_list, fu, tol)
struct nmg_ptbl		*vert_list;
CONST struct faceuse	*fu;
CONST struct rt_tol	*tol;
{
	int			i;
	int			j;
	struct vertexuse	*vu;
	struct loopuse		*lu;
	CONST struct loop_g	*lg;
	CONST struct loopuse	*fu2lu;
	CONST struct loop_g	*fu2lg = (CONST struct loop_g *)NULL;
	int			overlap = 0;

	NMG_CK_FACEUSE(fu);
	RT_CK_TOL(tol);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_purge_unwanted_intersection_points(0x%08x, 0x%08x)\n", vert_list, fu);

	for (i=0 ; i < vert_list->end ; i++) {
		vu = (struct vertexuse *)vert_list->buffer[i];
		NMG_CK_VERTEXUSE(vu);
		lu = nmg_find_lu_of_vu( vu );
		NMG_CK_LOOPUSE(lu);
		lg = lu->l_p->lg_p;
		NMG_CK_LOOP_G(lg);

		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			rt_log("vu[%d]: 0x%08x (%g %g %g) lu: 0x%08x %s\n",
				i, vu, V3ARGS(vu->v_p->vg_p->coord),
				lu, nmg_orientation(lu->orientation) );
			rt_log("\tlu BBox: (%g %g %g) (%g %g %g)\n",
				V3ARGS(lg->min_pt), V3ARGS(lg->max_pt) );
		}
		if (lu->up.fu_p->f_p == fu->f_p)
			rt_log("I'm checking against my own face?\n");

		/* If the bounding box of a loop doesn't intersect the
		 * bounding box of a loop in the other face, it shouldn't
		 * be on the list of intersecting loops.
		 */
		overlap = 0;
		for (RT_LIST_FOR(fu2lu, loopuse, &fu->lu_hd )){
			NMG_CK_LOOPUSE(fu2lu);
			NMG_CK_LOOP(fu2lu->l_p);

			switch(fu2lu->orientation)  {
			case OT_BOOLPLACE:
				/*  If this loop is destined for removal
				 *  by sanitize(), skip it.
				 */
				continue;
			case OT_UNSPEC:
				/* If this is a loop of a lone vertex,
				 * it was deposited into the
				 * other loop as part of an intersection
				 * operation, and is quite important.
				 */
				if( RT_LIST_FIRST_MAGIC(&fu2lu->down_hd) != NMG_VERTEXUSE_MAGIC )
					rt_log("nmg_purge_unwanted_intersection_points() non self-loop OT_UNSPEC vertexuse in fu2?\n");
				break;
			case OT_SAME:
			case OT_OPPOSITE:
				break;
			default:
				rt_log("nmg_purge_unwanted_intersection_points encountered %s loop in fu2\n",
					nmg_orientation(fu2lu->orientation));
				/* Process it anyway */
				break;
			}

			fu2lg = fu2lu->l_p->lg_p;
			NMG_CK_LOOP_G(fu2lg);

			if (rt_g.NMG_debug & DEBUG_POLYSECT) {
				rt_log("\tfu2lu BBox: (%g %g %g)  (%g %g %g) %s\n",
					V3ARGS(fu2lg->min_pt), V3ARGS(fu2lg->max_pt),
					nmg_orientation(fu2lu->orientation) );
			}

			if (V3RPP_OVERLAP_TOL(fu2lg->min_pt, fu2lg->max_pt,
			    lg->min_pt, lg->max_pt, tol)) {
				overlap = 1;
				break;
			}
		}
		if (!overlap) {
			/* why is this vertexuse in the list? */
			if (rt_g.NMG_debug & DEBUG_POLYSECT) {
				rt_log("nmg_purge_unwanted_intersection_points This little bugger slipped in somehow.  Deleting it from the list.\n");
				nmg_pr_vu_briefly(vu, (char *)NULL);
			}
			if( RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC &&
			    lu->orientation == OT_UNSPEC )  {
				/* Drop this loop of a single vertex in sanitize() */
				if (rt_g.NMG_debug & DEBUG_POLYSECT)
					rt_log("nmg_purge_unwanted_intersection_points() remarking as OT_BOOLPLACE\n");
				lu->orientation =
				  lu->lumate_p->orientation = OT_BOOLPLACE;
			}

			/* delete the entry from the vertex list */
			for (j=i ; j < vert_list->end ; j++)
				vert_list->buffer[j] = vert_list->buffer[j+1];

			--(vert_list->end);
			vert_list->buffer[vert_list->end] = (long *)NULL;
			--i;
		}
	}
}


/*				N M G _ I N _ O R _ R E F
 *
 *	if the given vertexuse "vu" is in the table given by "b" or if "vu"
 *	references a vertex which is refernced by a vertexuse in the table,
 *	then we return 1.  Otherwise, we return 0.
 */
int 
nmg_in_or_ref(vu, b)
struct vertexuse *vu;
struct nmg_ptbl *b;
{
	union {
		struct vertexuse **vu;
		long **magic_p;
	} p;
	int i;

	p.magic_p = b->buffer;
	for (i=0 ; i < b->end ; ++i) {
		if (p.vu[i] && *p.magic_p[i] == NMG_VERTEXUSE_MAGIC &&
		    (p.vu[i] == vu || p.vu[i]->v_p == vu->v_p))
			return(1);
	}
	return(0);
}

/*
 *			N M G _ R E B O U N D
 *
 *  Re-compute all the bounding boxes in the NMG model.
 *  Bounding boxes are presently found in these structures:
 *	loop_g
 *	face_g
 *	shell_a
 *	nmgregion_a
 *  The re-bounding must be performed in a bottom-up manner,
 *  computing the loops first, and working up to the nmgregions.
 */
void
nmg_rebound( m, tol )
struct model		*m;
CONST struct rt_tol	*tol;
{
	struct nmgregion	*r;
	struct shell		*s;
	struct faceuse		*fu;
	struct face		*f;
	struct loopuse		*lu;
	struct loop		*l;
	register int		*flags;

	NMG_CK_MODEL(m);
	RT_CK_TOL(tol);

	flags = (int *)rt_calloc( m->maxindex, sizeof(int), "rebound flags[]" );

	for( RT_LIST_FOR( r, nmgregion, &m->r_hd ) )  {
		NMG_CK_REGION(r);
		for( RT_LIST_FOR( s, shell, &r->s_hd ) )  {
			NMG_CK_SHELL(s);

			/* Loops in faces in shell */
			for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
				NMG_CK_FACEUSE(fu);
				/* Loops in face */
				for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
					NMG_CK_LOOPUSE(lu);
					l = lu->l_p;
					NMG_CK_LOOP(l);
					if( NMG_INDEX_FIRST_TIME(flags,l) )
						nmg_loop_g(l, tol);
				}
			}
			/* Faces in shell */
			for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
				NMG_CK_FACEUSE(fu);
				f = fu->f_p;
				NMG_CK_FACE(f);

				/* Rebound the face */
				if( NMG_INDEX_FIRST_TIME(flags,f) )
					nmg_face_bb( f, tol );
			}

			/* Wire loops in shell */
			for( RT_LIST_FOR( lu, loopuse, &s->lu_hd ) )  {
				NMG_CK_LOOPUSE(lu);
				l = lu->l_p;
				NMG_CK_LOOP(l);
				if( NMG_INDEX_FIRST_TIME(flags,l) )
					nmg_loop_g(l, tol);
			}

			/*
			 *  Rebound the shell.
			 *  This routine handles wire edges and lone vertices.
			 */
			if( NMG_INDEX_FIRST_TIME(flags,s) )
				nmg_shell_a( s, tol );
		}

		/* Rebound the nmgregion */
		nmg_region_a( r, tol );
	}

	rt_free( (char *)flags, "rebound flags[]" );
}

/*
 *			N M G _ C O U N T _ S H E L L _ K I D S
 */
void
nmg_count_shell_kids(m, total_faces, total_wires, total_points)
CONST struct model *m;
unsigned long *total_wires;
unsigned long *total_faces;
unsigned long *total_points;
{
	short *tbl;

	CONST struct nmgregion *r;
	CONST struct shell *s;
	CONST struct faceuse *fu;
	CONST struct loopuse *lu;
	CONST struct edgeuse *eu;

	NMG_CK_MODEL(m);

	tbl = (short *)rt_calloc(m->maxindex+1, sizeof(char),
		"face/wire/point counted table");

	*total_faces = *total_wires = *total_points = 0;
	for (RT_LIST_FOR(r, nmgregion, &m->r_hd)) {
		for (RT_LIST_FOR(s, shell, &r->s_hd)) {
			if (s->vu_p) {
				total_points++;
				continue;
			}
			for (RT_LIST_FOR(fu, faceuse, &s->fu_hd)) {
				if (NMG_INDEX_TEST_AND_SET(tbl, fu->f_p))
					(*total_faces)++;
			}
			for (RT_LIST_FOR(lu, loopuse, &s->lu_hd)) {
				if (NMG_INDEX_TEST_AND_SET(tbl, lu->l_p))
					(*total_wires)++;
			}
			for (RT_LIST_FOR(eu, edgeuse, &s->eu_hd)) {
				if (NMG_INDEX_TEST_AND_SET(tbl, eu->e_p))
					(*total_wires)++;
			}
		}
	}

	rt_free((char *)tbl, "face/wire/point counted table");
}

/*
 *	O R D E R _ T B L
 *
 *	private support routine for nmg_close_shell
 *	creates an array of indices into a table of edgeuses, ordered
 *	to create a loop.
 *
 *	Arguments:
 *	tbl is the table (provided by caller)
 *	index is the array of indices created by order_tbl
 *	tbl_size is the size of the table (provided by caller)
 *	loop_size is the number of edgeuses in the loop (calculated by order_tbl)
 */
static void
order_tbl( tbl , index , tbl_size , loop_size )
struct nmg_ptbl *tbl;
int **index;
int tbl_size;
int *loop_size;
{
	int i,j;
	int found;
	struct edgeuse *eu,*eu1;

	/* create an index into the table, ordered to create a loop */
	if( *index == NULL )
		(*index) = (int *)rt_calloc( tbl_size , sizeof( int ) , "Table index" );

	for( i=0 ; i<tbl_size ; i++ )
		(*index)[i] = (-1);

	/* start the loop at index = 0 */
	(*index)[0] = 0;
	*loop_size = 1;
	eu = (struct edgeuse *)NMG_TBL_GET( tbl , 0 );
	found = 1;
	i = 0;
	while( found )
	{
		found = 0;

		/* Look for edgeuse that starts where "eu" ends */
		for( j=1 ; j<tbl_size ; j++ )
		{
			eu1 = (struct edgeuse *)NMG_TBL_GET( tbl , j );
			if( eu1->vu_p->v_p == eu->eumate_p->vu_p->v_p )
			{
				/* Found it */
				found = 1;
				(*index)[++i] = j;
				(*loop_size)++;
				eu = eu1;
				break;
			}
		}
	}
}

/*
 *	N M G _ C L O S E _ S H E L L
 *
 *	Examines the passed shell and, if there are holes, closes them
 *	note that not much care is taken as to how the holes are closed
 *	so the results are not entirely predictable.
 *	A list of free edges is created (edges bounding only one face).
 *	New faces are constructed by taking two consecutive edges
 *	and making a face. The newly created edge is added to the list
 *	of free edges and the two used ones are removed.
 *
 */
void
nmg_close_shell( s , tol )
struct shell *s;
struct rt_tol *tol;
{
	struct nmg_ptbl eu_tbl;		/* table of free edgeuses from shell */
	struct nmg_ptbl vert_tbl;	/* table of vertices for use in nmg_cface */
	int *index;			/* array of indices into eu_tbl, ordered to form a loop */
	int loop_size;			/* number of edgeueses in loop */
	struct faceuse **fu_list;	/* array of pointers to faceuses, for use in nmg_gluefaces */
	int fu_counter=0;		/* number of faceuses in above array */
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu,*eu1,*eu2,*eu3,*eu_new;
	int i,j;
	int found;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_close_shell: s = x%x\n" , s );

	NMG_CK_SHELL( s );

	index = NULL;

	/* construct a table of free edges */
	(void)nmg_tbl( &eu_tbl , TBL_INIT , NULL );

	/* loop through all the faces in the shell */
	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		/* only look at OT_SAME faces */
		if( fu->orientation == OT_SAME )
		{
			/* count 'em */
			fu_counter++;
			/* loop through each loopuse in the face */
			for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
				NMG_CK_LOOPUSE( lu );
				/* ignore loops that are just a vertex */
				if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) ==
					NMG_VERTEXUSE_MAGIC )
						continue;

				/* loop through all the edgeuses in the loop */
				for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					NMG_CK_EDGEUSE( eu );
					/* if this edgeuse is a free edge, add its mate to the list */
					if( eu->radial_p == eu->eumate_p )
						(void)nmg_tbl( &eu_tbl , TBL_INS , (long *) eu->eumate_p );
				}
			}
		}
	}

	/* if there is nothing in our list of free edges, the shell is already closed */
	if( NMG_TBL_END( &eu_tbl ) == 0 )
	{
		nmg_tbl( &eu_tbl , TBL_FREE , NULL );
		return;
	}

	/* put all the existing faces in a list (needed later for "nmg_gluefaces") */
	fu_list = (struct faceuse **)rt_calloc( fu_counter + NMG_TBL_END( &eu_tbl ) - 2 , sizeof( struct faceuse *) , "face use list " );
	fu_counter = 0;
	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		if( fu->orientation == OT_SAME )
			fu_list[fu_counter++] = fu;
	}

	/* create a table of vertices */
	(void)nmg_tbl( &vert_tbl , TBL_INIT , NULL );

	while( NMG_TBL_END( &eu_tbl ) )
	{
		vect_t normal,v1,v2,tmp_norm;
		int give_up_on_face=0;

		/* Create an index into the table that orders the edgeuses into a loop */
		order_tbl( &eu_tbl , &index , NMG_TBL_END( &eu_tbl ) , &loop_size );

		/* Create new faces to close the shell */
		while( loop_size > 3 )
		{
			vect_t inside;			/* vector pointing to left of edge (inside face) */
			struct edgeuse **eu_used;	/* array of edgueses used, for deletion */
			vect_t v1,v2;			/* edge vectors */
			int edges_used;			/* number of edges used in making a face */
			int found_face=0;		/* flag - indicates that a face with the correct normal will be created */
			int start_index,end_index;	/* start and stop index for loop */
			int coplanar;			/* flag - indicates entire loop is coplanar */
			plane_t pl1,pl2;		/* planes for checking coplanarity of loop */
			point_t pt[3];			/* points for calculating planes */

			/* Look for an easy way out, maybe this loop is planar */
			/* first, calculate a plane from the first three non-collinear vertices */
			start_index = 0;
			end_index = start_index + 3;
			
			for( i=start_index ; i<end_index ; i++ )
			{
				eu = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );
				VMOVE( pt[i-start_index] , eu->vu_p->v_p->vg_p->coord );
			}
			while( rt_mk_plane_3pts( pl1 , pt[0] , pt[1] , pt[2] , tol ) && end_index<loop_size )
			{
				start_index++;
				end_index++;
				for( i=start_index ; i<end_index ; i++ )
				{
					eu = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );
					VMOVE( pt[i-start_index] , eu->vu_p->v_p->vg_p->coord );
				}
			}
			if( end_index == loop_size )
			{
				/* Could not even make a plane, this is some serious screw-up */
				rt_bomb( "nmg_close_shell: cannot make any planes from loop\n" );
			}

			/* now we have one plane, let's check the others */
			coplanar = 1;
			while( end_index < loop_size && coplanar )
			{
				end_index +=3;
				if( end_index > loop_size )
					end_index = loop_size;
				start_index = end_index - 3;

				for( i=start_index ; i<end_index ; i++ )
				{
					eu = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );
					VMOVE( pt[i-start_index] , eu->vu_p->v_p->vg_p->coord );
				}

				/* if these three points make a plane, is it coplanar with
				 * our first one??? */
				if( !rt_mk_plane_3pts( pl2 , pt[0] , pt[1] , pt[2] , tol ) )
				{
					if( rt_coplanar( pl1 , pl2 , tol ) < 1 )
						coplanar = 0;
				}
			}

			if( coplanar )	/* excellent! - just make one big face */
			{
				/* put vertices in table */
				nmg_tbl( &vert_tbl , TBL_RST , NULL );
				for( i=0 ; i<loop_size ; i++ )
				{
					eu = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );
					nmg_tbl( &vert_tbl , TBL_INS , (long *)eu->vu_p->v_p );
				}

				/* make face */
				fu = nmg_cface( s , (struct vertex **)NMG_TBL_BASEADDR(&vert_tbl) , loop_size );

				/* already have face geometry, so don't need to call nmg_fu_planeeqn */
				nmg_face_g( fu , pl1 );

				/* add this face to list for glueing */
				fu_list[fu_counter++] = fu;

				/* now eliminate loop from table */
				eu_used = (struct edgeuse **)rt_calloc( loop_size , sizeof( struct edguse *) , "edges used list" );
				for( i=0 ; i<loop_size ; i++ )
					eu_used[i] = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );

				for( i=0 ; i<loop_size ; i++ )
					nmg_tbl( &eu_tbl , TBL_RM , (long *)eu_used[i] );

				rt_free( (char *)eu_used , "edge used list" );

				/* set some flags to get us back to start of loop */
				found_face = 1;
				give_up_on_face = 1;
			}

			/* OK, so we have to do this one little-by-little */
			start_index = 0;
			while( !found_face )
			{
				vect_t	norm;

				/* refresh the vertex list */
				(void)nmg_tbl( &vert_tbl , TBL_RST , NULL );

				end_index = start_index + 1;
				if( end_index == loop_size )
					end_index = 0;

				/* Get two edgeuses from the loop */
				eu1 = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[start_index] );
				nmg_tbl( &vert_tbl , TBL_INS , (long *)eu1->vu_p->v_p );

				VSUB2( v1 , eu1->eumate_p->vu_p->v_p->vg_p->coord , eu1->vu_p->v_p->vg_p->coord );
				VCROSS( inside , normal , v1 );

				eu2 = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[end_index] );
				nmg_tbl( &vert_tbl , TBL_INS , (long *)eu2->vu_p->v_p );

				edges_used = 2;	
				/* if the edges are collinear, we can't make a face */
				while( rt_3pts_collinear(
					eu1->vu_p->v_p->vg_p->coord,
					eu2->vu_p->v_p->vg_p->coord,
					eu2->eumate_p->vu_p->v_p->vg_p->coord,
					tol ) && edges_used < loop_size )
				{
					/* So, add another edge */
					end_index++;
					if( end_index == loop_size )
						end_index = 0;
					eu1 = eu2;
					eu2 = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[end_index]);
					nmg_tbl( &vert_tbl , TBL_INS , (long *)eu2->vu_p->v_p );
					edges_used++;
				}

				found_face = 1;
				VSUB2( v2 , eu2->eumate_p->vu_p->v_p->vg_p->coord , eu2->vu_p->v_p->vg_p->coord );
				fu = nmg_find_fu_of_eu( eu1 );
				NMG_GET_FU_NORMAL( norm, fu );
				VCROSS( inside , norm , v1 );
				if( VDOT( inside , v2 ) > 0.0 )
				{
					/* this face normal would be in the wrong direction */
					found_face = 0;

					/* move along the loop by one edge and try again */
					start_index++;
					if( start_index > loop_size-2 )
					{
						/* can't make a face from this loop, so delete it */
						eu_used = (struct edgeuse **)rt_calloc( loop_size , sizeof( struct edguse *) , "edges used list" );
						for( i=0 ; i<loop_size ; i++ )
							eu_used[i] = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );
						for( i=0 ; i<loop_size ; i++ )
							nmg_tbl( &eu_tbl , TBL_RM , (long *)eu_used[i] );

						rt_free( (char *)eu_used , "edge used list" );

						give_up_on_face = 1;
						break;
					}
				}
			}

			if( give_up_on_face )
				break;			

			/* add last vertex to table */
			nmg_tbl( &vert_tbl , TBL_INS , (long *)eu2->eumate_p->vu_p->v_p );

			/* save list of used edges to be removed later */
			eu_used = (struct edgeuse **)rt_calloc( edges_used , sizeof( struct edguse *) , "edges used list" );
			for( i=0 ; i<edges_used ; i++ )
				eu_used[i] = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );

			/* make a face */
			fu = nmg_cface( s , (struct vertex **)NMG_TBL_BASEADDR(&vert_tbl) , edges_used+1 );
			if( nmg_fu_planeeqn( fu , tol ) )
				rt_log( "Failed planeeq\n" );

			/* add new face to the list of faces */
			fu_list[fu_counter++] = fu;

			/* find the newly created edgeuse */
			found = 0;
			for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
				NMG_CK_LOOPUSE( lu );
				if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) ==
					NMG_VERTEXUSE_MAGIC )
						continue;
				for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					NMG_CK_EDGEUSE( eu );
					if( eu->vu_p->v_p == (struct vertex *)NMG_TBL_GET( &vert_tbl , 0 )
					&& eu->eumate_p->vu_p->v_p == (struct vertex *)NMG_TBL_GET( &vert_tbl , edges_used) )
					{
						eu_new = eu;
						found = 1;
						break;
					}
					else if( eu->vu_p->v_p == (struct vertex *)NMG_TBL_GET( &vert_tbl , edges_used)
					&& eu->eumate_p->vu_p->v_p == (struct vertex *)NMG_TBL_GET( &vert_tbl , 0 ) )
					{
						eu_new = eu->eumate_p;
						found = 1;
						break;
					}

				}
				if( found )
					break;
			}

			/* out with the old, in with the new */
			for( i=0 ; i<edges_used ; i++ )
				nmg_tbl( &eu_tbl , TBL_RM , (long *)eu_used[i] );
			nmg_tbl( &eu_tbl , TBL_INS , (long *)eu_new );

			rt_free( (char *)eu_used , "edge used list" );

			/* re-order loop */
			order_tbl( &eu_tbl , &index , NMG_TBL_END( &eu_tbl ) , &loop_size );
		}

		if( give_up_on_face )
			continue;

		if( loop_size != 3 )
		{
			rt_log( "Error, loop size should be 3\n" );
			nmg_tbl( &eu_tbl , TBL_FREE , NULL );
			nmg_tbl( &vert_tbl , TBL_FREE , NULL );
			rt_free( (char *)index , "index" );
			rt_free( (char *)fu_list , "faceuse list " );
			return;
		}

		/* if the last 3 vertices are collinear, then don't make the last face */
		nmg_tbl( &vert_tbl , TBL_RST , NULL );
		for( i=0 ; i<3 ; i++ )
		{
			eu = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );
			(void)nmg_tbl( &vert_tbl , TBL_INS , (long *)eu->vu_p->v_p);
		}

		if( !rt_3pts_collinear(
			((struct vertex *)NMG_TBL_GET( &vert_tbl , 0 ))->vg_p->coord,
			((struct vertex *)NMG_TBL_GET( &vert_tbl , 1 ))->vg_p->coord,
			((struct vertex *)NMG_TBL_GET( &vert_tbl , 2 ))->vg_p->coord,
			tol ) )
		{
		
			/* Create last face from remaining 3 edges */
			fu = nmg_cface( s , (struct vertex **)NMG_TBL_BASEADDR(&vert_tbl) , 3 );
			if( nmg_fu_planeeqn( fu , tol ) )
				rt_log( "Failed planeeq\n" );

			/* and add it to the list */
			fu_list[fu_counter++] = fu;

		}

		/* remove the last three edges from the table */
		eu1 = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[0] );
		eu2 = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[1] );
		eu3 = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[2] );
		nmg_tbl( &eu_tbl , TBL_RM , (long *)eu1 );
		nmg_tbl( &eu_tbl , TBL_RM , (long *)eu2 );
		nmg_tbl( &eu_tbl , TBL_RM , (long *)eu3 );
	}

	/* finally, glue it all together */
	nmg_gluefaces( fu_list , fu_counter );

	/* Free up all the memory */
	rt_free( (char *)index , "index" );
	rt_free( (char *)fu_list , "faceuse list " );
	nmg_tbl( &eu_tbl , TBL_FREE , NULL );
	nmg_tbl( &vert_tbl , TBL_FREE , NULL );

	/* we may have constructed some coplanar faces */
	nmg_shell_coplanar_face_merge( s , tol , 1 );
	if( nmg_simplify_shell( s ) )
	{
		rt_log( "nmg_close_shell(): Simplified shell is empty" );
		return;
	}
}

/*
 *	N M G _ D U P _ S H E L L
 *
 *	Duplicate a shell and return the new copy. New shell is
 *	in the same region.
 *
 *  The vertex geometry is copied from the source faces into topologically
 *  distinct (new) vertex and vertex_g structs.
 *  They will start out being geometricly coincident, but it is anticipated
 *  that the caller will modify the geometry, e.g. as in an extrude operation.
 *
 *  NOTE: This routine creates a translation table that gives the
 *  correspondence between old and new structures, the caller is responsible
 *  for freeing this memory. Warning - NOT EVERY structure is assigned a
 *  correspondence.
 */
struct shell *
nmg_dup_shell( s , trans_tbl )
struct shell *s;
long ***trans_tbl;
{
	struct model *m;
	struct shell *new_s;
	struct faceuse *fu;
	struct loopuse *lu,*new_lu;
	struct edgeuse *eu;
	struct faceuse *new_fu;
	struct nmg_ptbl faces;

	if( rt_g.NMG_debug & DEBUG_BASIC) 
		rt_log( "nmg_dup_shell( s = x%x , trans_tbl = x%x )\n" , s , trans_tbl );

	NMG_CK_SHELL( s );

	m = nmg_find_model( (long *)s );

	/* create translation table double size to accomodate both copies */
	(*trans_tbl) = (long **)rt_calloc(m->maxindex*2, sizeof(long *),
		"nmg_dup_shell trans_tbl" );

	nmg_tbl( &faces , TBL_INIT , NULL );

	new_s = nmg_msv( s->r_p );
	NMG_INDEX_ASSIGN( (*trans_tbl) , s , (long *)new_s );

	/* copy face uses */
	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		if( fu->orientation == OT_SAME )
		{
			new_fu = (struct faceuse *)NULL;
			for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
				NMG_CK_LOOPUSE( lu );
				if( new_fu )
				{
					new_lu = nmg_dup_loop( lu , &new_fu->l.magic , (*trans_tbl) );
					NMG_INDEX_ASSIGN( (*trans_tbl) , lu , (long *)new_lu );
				}
				else
				{
					new_lu = nmg_dup_loop( lu , &new_s->l.magic , (*trans_tbl) );
					NMG_INDEX_ASSIGN( (*trans_tbl) , lu , (long *)new_lu );
					new_fu = nmg_mf( new_lu );
					NMG_INDEX_ASSIGN( (*trans_tbl) , fu , (long *)new_fu );
					NMG_INDEX_ASSIGN( (*trans_tbl) , fu->fumate_p , (long *)new_fu->fumate_p );
					NMG_INDEX_ASSIGN( (*trans_tbl) , fu->f_p , (long *)new_fu->f_p );
				}
			}
			if (fu->f_p->fg_p)
			{
#if 1
				/* Do it this way if you expect to change the normals */
				plane_t	n;
				NMG_GET_FU_PLANE( n, fu );
				nmg_face_g(new_fu, n);
#else
				/* Do it this way to share fu's geometry struct */
				nmg_jfg( fu, new_fu );
#endif

				/* XXX Perhaps this should be new_fu->f_p->fg_p ? */
				NMG_INDEX_ASSIGN( (*trans_tbl) , fu->f_p->fg_p , (long *)new_fu->f_p->fg_p );
			}
			new_fu->orientation = fu->orientation;
			new_fu->fumate_p->orientation = fu->fumate_p->orientation;
			nmg_tbl( &faces , TBL_INS , (long *)new_fu );
		}
	}

	/* glue new faces */
	nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ) );
	nmg_tbl( &faces , TBL_FREE , NULL );

	/* copy wire loops */
	for( RT_LIST_FOR( lu , loopuse , &s->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu );
		new_lu = nmg_dup_loop( lu , &new_s->l.magic , (*trans_tbl) );
		NMG_INDEX_ASSIGN( (*trans_tbl) , lu , (long *)new_lu );
	}

	/* copy wire edges */
	for( RT_LIST_FOR( eu , edgeuse , &s->eu_hd ) )
	{
		struct vertex *old_v1,*old_v2,*new_v1,*new_v2;
		struct edgeuse *new_eu;

		NMG_CK_EDGEUSE( eu );
		NMG_CK_VERTEXUSE( eu->vu_p );
		NMG_CK_VERTEX( eu->vu_p->v_p );
		NMG_CK_EDGEUSE( eu->eumate_p );
		NMG_CK_VERTEXUSE( eu->eumate_p->vu_p );
		NMG_CK_VERTEX( eu->eumate_p->vu_p->v_p );

		old_v1 = eu->vu_p->v_p;
		new_v1 = NMG_INDEX_GETP(vertex, (*trans_tbl), old_v1);
		old_v2 = eu->eumate_p->vu_p->v_p;
		new_v2 = NMG_INDEX_GETP(vertex, (*trans_tbl), old_v2);

		/* make the wire edge */
		new_eu = nmg_me( new_v1 , new_v2 , new_s );
		NMG_INDEX_ASSIGN( (*trans_tbl) , eu , (long *)new_eu );

		new_v1 = new_eu->vu_p->v_p;
		NMG_INDEX_ASSIGN( (*trans_tbl) , old_v1 , (long *)new_v1 );
		if( !new_v1->vg_p )
		{
			nmg_vertex_gv( new_v1 , old_v1->vg_p->coord );
			NMG_INDEX_ASSIGN( (*trans_tbl) , old_v1->vg_p , (long *)new_v1->vg_p );
		}

		new_v2 = new_eu->eumate_p->vu_p->v_p;
		NMG_INDEX_ASSIGN( (*trans_tbl) , old_v2 , (long *)new_v2 );
		if( !new_v2->vg_p )
		{
			nmg_vertex_gv( new_v2 , old_v2->vg_p->coord );
			NMG_INDEX_ASSIGN( (*trans_tbl) , old_v2->vg_p , (long *)new_v2->vg_p );
		}

	}

#if 0
	/* XXX for this to work nmg_mvu and nmg_mvvu must not be private
	 *     perhaps there is another way???? */
	/* copy vertex use
	 * This must be done last, since other routines may steal it */
	if( s->vu_p )
	{
		old_vu = s->vu_p;
		NMG_CK_VERTEXUSE( old_vu );
		old_v = old_vu->v_p;
		NMG_CK_VERTEX( old_v );
		new_v = NMG_INDEX_GETP(vertex, (*trans_tbl), old_v);
		if( new_v )
		{
			/* already copied vertex, just need a use */
			if( new_s->vu_p )
				(void )nmg_kvu( new_s->vu_p );
			new_s->vu_p = nmg_mvu( new_v , (long *)new_s , m );
		}
		else
		{
			/* make a new vertex and use */
			new_s->vu_p = nmg_mvvu( (long *)new_s , m );
			new_v = new_s->vu_p->v_p;

			/* put entry in table */
			NMG_INDEX_ASSIGN( (*trans_tbl) , old_v , (long *)new_v );

			/* assign the same geometry as the old copy */
			nmg_vertex_gv( new_v , old_v->vg_p->coord );
			NMG_INDEX_ASSIGN( (*trans_tbl) , old_v->vg_p , (long *)new_v->vg_p );
		}
	}
#endif
	
	return( new_s );
}

/*	Routines to use the nmg_ptbl structures as a stack of edgeuse structures */

#define	NMG_PUSH( _ptr , _stack )	nmg_tbl( _stack , TBL_INS , (long *) _ptr );

struct edgeuse
*nmg_pop_eu( stack )
struct nmg_ptbl *stack;
{
	struct edgeuse *eu;

	/* return a NULL if stack is empty */
	if( NMG_TBL_END( stack ) == 0 )
		return( (struct edgeuse *)NULL );

	/* get last edgeuse on the stack */
	eu = (struct edgeuse *)NMG_TBL_GET( stack , NMG_TBL_END( stack )-1 );

	/* remove that edgeuse from the stack */
	nmg_tbl( stack , TBL_RM , (long *)eu );

	return( eu );
}

/*	N M G _ R E V E R S E _ F A C E _ A N D _ R A D I A L S
 *
 *	This routine calls "nmg_reverse_face" and also makes the radial
 *	pointers connect faces of like orientation (i.e., OT_SAME to OT_SAME and
 *	OT_OPPOSITE to OT_OPPOSITE).
 */

void
nmg_reverse_face_and_radials( fu , tol )
struct faceuse *fu;
CONST struct rt_tol *tol;
{
	struct loopuse *lu;
	struct edgeuse *eu;
	struct faceuse *fu2;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_reverse_face_and_radials( fu = x%x )\n" , fu );

	NMG_CK_FACEUSE( fu );
	RT_CK_TOL( tol );

	/* reverse face */
	nmg_reverse_face( fu );

	(void)nmg_face_fix_radial_parity( fu , tol );
}

/*
 *	N M G _ F I N D _ T O P _ F A C E
 *
 *	Finds the topmost face in a shell (in z-direction).
 *	Expects to have a translation table (variable "flags") for
 *	the model, and will ignore face structures that have their
 *	flag set in the table.
 */

struct face *
nmg_find_top_face( s , flags )
struct shell *s;
long *flags;
{
	fastf_t max_z=(-MAX_FASTF);
	fastf_t max_slope=(-MAX_FASTF);
	vect_t edge;
	struct face *f_top=(struct face *)NULL;
	struct edge *e_top=(struct edge *)NULL;
	struct vertex *vp_top=(struct vertex *)NULL;
	struct loopuse *lu;
	struct faceuse *fu;
	struct edgeuse *eu,*eu1,*eu2;
	struct vertexuse *vu;
	int done;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_find_top_face( s = x%x , flags = x%x )\n" , s , flags );

	NMG_CK_SHELL( s );

	/* find vertex with greatest z coordinate */
	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		if( NMG_INDEX_TEST( flags , fu->f_p ) )
			continue;
		for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			NMG_CK_LOOPUSE( lu );
			if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_EDGEUSE_MAGIC )
			{
				for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					NMG_CK_EDGEUSE( eu );
					if( eu->vu_p->v_p->vg_p->coord[Z] > max_z )
					{
						max_z = eu->vu_p->v_p->vg_p->coord[Z];
						vp_top = eu->vu_p->v_p;
					}
				}
			}
		}
	}
	if( vp_top == (struct vertex *)NULL )
	{
		rt_log( "Fix_normals: Could not find uppermost vertex" );
		return( (struct face *)NULL );
	}

	/* find edge from vp_top with largest slope in +z direction */
	for( RT_LIST_FOR( vu , vertexuse , &vp_top->vu_hd ) )
	{
		NMG_CK_VERTEXUSE( vu );
		if( *vu->up.magic_p == NMG_EDGEUSE_MAGIC )
		{
			struct vertexuse *vu1;

			eu = vu->up.eu_p;
			NMG_CK_EDGEUSE( eu );

			/* skip wire edges */
			if( *eu->up.magic_p != NMG_LOOPUSE_MAGIC )
				continue;

			/* skip wire loops */
			if( *eu->up.lu_p->up.magic_p != NMG_FACEUSE_MAGIC )
				continue;

			/* skip finished faces */
			if( NMG_INDEX_TEST( flags , eu->up.lu_p->up.fu_p->f_p ) )
				continue;

			/* skip edges from other shells */
			if( nmg_find_s_of_eu( eu ) != s )
				continue;

			/* get vertex at other end of this edge */
			vu1 = eu->eumate_p->vu_p;
			NMG_CK_VERTEXUSE( vu1 );

			/* make a unit vector in direction of edgeuse */
			VSUB2( edge , vu1->v_p->vg_p->coord , vu->v_p->vg_p->coord );
			VUNITIZE( edge );

			/* check against current maximum slope */
			if( edge[Z] > max_slope )
			{
				max_slope = edge[Z];
				e_top = eu->e_p;
			}
		}
	}
	if( e_top == (struct edge *)NULL )
	{
		rt_log( "Fix_normals: Could not find uppermost edge" );
		return( (struct face *)NULL );
	}

	/* now find the face containing e_top with "left-pointing vector" having the greatest slope */
	max_slope = (-MAX_FASTF);
	eu = e_top->eu_p;
	eu1 = eu;
	done = 0;
	while( !done )
	{
		/* don't bother with anything but faces */
		if( *eu1->up.magic_p == NMG_LOOPUSE_MAGIC )
		{
			lu = eu1->up.lu_p;
			NMG_CK_LOOPUSE( lu );
			if( *lu->up.magic_p == NMG_FACEUSE_MAGIC )
			{
				vect_t left;
				vect_t edge_dir;
				vect_t normal;

				/* fu is a faceuse containing "eu1" */
				fu = lu->up.fu_p;
				NMG_CK_FACEUSE( fu );

				/* skip faces from other shells */
				if( fu->s_p != s )
				{
					/* go on to next radial face */
					eu1 = eu1->eumate_p->radial_p;

					/* check if we are back where we started */
					if( eu1 == eu )
						done = 1;

					continue;
				}

				/* make a vector in the direction of "eu1" */
				VSUB2( edge_dir , eu1->vu_p->v_p->vg_p->coord , eu1->eumate_p->vu_p->v_p->vg_p->coord );

				/* find the normal for this faceuse */
				NMG_GET_FU_NORMAL( normal, fu );

				/* edge direction cross normal gives vetor in face */
				VCROSS( left , edge_dir , normal );

				/* unitize to get slope */
				VUNITIZE( left );

				/* check against current max slope */
				if( left[Z] > max_slope )
				{
					max_slope = left[Z];
					f_top = fu->f_p;
				}
			}
		}
		/* go on to next radial face */
		eu1 = eu1->eumate_p->radial_p;

		/* check if we are back where we started */
		if( eu1 == eu )
			done = 1;
	}

	if( f_top == (struct face *)NULL )
	{
		rt_log( "Fix_normals: Could not find uppermost face" );
		return( (struct face *)NULL );
	}

	return( f_top );
}


/*	N M G _ P R O P A G A T E _ N O R M A L S
 *
 *	This routine expects "fu_in" to have a correctly oriented normal.
 *	It then checks all faceuses in the same shell it can reach via radial structures, and
 *	reverses faces and modifies radial structures as needed to result in
 *	a consistent NMG shell structure. The "flags" variable is a translation table
 *	for the model, and as each face is checked, its flag is set. Faces with flags
 *	that have already been set will not be checked by this routine.
 */

void
nmg_propagate_normals( fu_in , flags , tol )
struct faceuse *fu_in;
long *flags;
CONST struct rt_tol *tol;
{
	struct nmg_ptbl stack;
	struct loopuse *lu;
	struct edgeuse *eu,*eu1;
	struct faceuse *fu;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_propagate_normals( fu_in = x%x , flags = x%x )\n" , fu_in , flags );

	NMG_CK_FACEUSE( fu_in );
	RT_CK_TOL( tol );

	fu = fu_in;
	if( fu->orientation != OT_SAME )
		fu = fu->fumate_p;
	if( fu->orientation != OT_SAME )
	{
		rt_log( "nmg_propagate_normals: Could not find OT_SAME orientation of faceuse x%x\n" , fu_in );
		return;
	}

	/* set flag for this face since we know this one is OK */
	NMG_INDEX_SET( flags , fu->f_p );

	/* Use the ptbl structure as a stack */
	nmg_tbl( &stack , TBL_INIT , NULL );

	/* push all edgeuses of "fu" onto the stack */
	for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu );
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;
		for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
		{
			/* don't push free edges on the stack */
			if( eu->radial_p->eumate_p != eu )
				NMG_PUSH( eu , &stack );
		}
	}

	/* now pop edgeuses from stack, go to radial face, fix its normal,
	 * and push its edgeuses onto the stack */

	while( (eu1 = nmg_pop_eu( &stack )) != (struct edgeuse *)NULL )
	{
		/* eu1 is an edgeuse on an OT_SAME face, so its radial
		 * should be in an OT_SAME also */

		NMG_CK_EDGEUSE( eu1 );

		/* go to the radial */
		eu = eu1->radial_p;
		NMG_CK_EDGEUSE( eu );

		/* make sure we are still on the same shell */
		while( nmg_find_s_of_eu( eu ) != fu_in->s_p && eu != eu1 && eu != eu1->eumate_p )
			eu = eu->eumate_p->radial_p;

		if( eu == eu1 || eu == eu1->eumate_p )
			continue;

		/* find the face that contains this edgeuse */
		if( *eu->up.magic_p != NMG_LOOPUSE_MAGIC )
			continue;

		lu = eu->up.lu_p;
		NMG_CK_LOOPUSE( lu );

		if( *lu->up.magic_p != NMG_FACEUSE_MAGIC )
			continue;

		fu = lu->up.fu_p;
		NMG_CK_FACEUSE( fu );

		/* if this face has already been processed, skip it */
		if( NMG_INDEX_TEST_AND_SET( flags , fu->f_p ) )
		{
			/* if orientation is wrong, or if the radial edges are in the same direction
			 * then reverse the face and fix the radials */
			if( fu->orientation != OT_SAME ||
				( eu1->vu_p->v_p == eu->vu_p->v_p &&
				 eu1->eumate_p->vu_p->v_p == eu->eumate_p->vu_p->v_p ))
			{
				nmg_reverse_face_and_radials( fu , tol );
			}

			/* make sure we are dealing with an OT_SAME faceuse */
			if( fu->orientation != OT_SAME )
				fu = fu->fumate_p;

			/* push all edgeuses of "fu" onto the stack */
			for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
				NMG_CK_LOOPUSE( lu );
				if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
					continue;
				for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					/* don't push free edges on the stack */
					if( eu->radial_p->eumate_p != eu )
						NMG_PUSH( eu , &stack );
				}
			}
		}
	}

	/* free the stack */
	nmg_tbl( &stack , TBL_FREE , NULL );
}

/*	N M G _ F I X _ N O R M A L S
 *
 *	Finds the topmost face in the shell, assumes its normal must have
 *	a positive z-component, and makes it so if not.  Then propagates this
 *	orientation though the radial structures.  Disjoint shells are handled
 *	by flagging checked faces, and calling "nmg_find_top_face" again.
 *
 *	XXX - known bug: shells that are intended to describe void spaces (i.e., should
 *			 have inward pointing normals), will end up with outward pointing
 *			 normals.
 *
 */

void
nmg_fix_normals( s , tol )
struct shell *s;
CONST struct rt_tol *tol;
{
	struct model *m;
	struct face *f_top;
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu,*eu1,*eu2;
	struct vertexuse *vu;
	long *flags;
	int missed_faces=1;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_fix_normals( s = x%x )\n" , s );

	NMG_CK_SHELL( s );
	RT_CK_TOL( tol );

	/* Make an index table to insure we visit each face once and only once */
	m = s->r_p->m_p;
	flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "nmg_fix_normals: flags" );

	/* loop to catch disjoint shells */
	while( missed_faces )
	{
		FAST fastf_t	z;

		/* find the top face */
		f_top = nmg_find_top_face( s , flags );	
		NMG_CK_FACE( f_top );
		if( f_top->flip )
			z = - f_top->fg_p->N[Z];
		else
			z =   f_top->fg_p->N[Z];

		/* f_top is the topmost face (in the +z direction), so its OT_SAME use should have a
		 * normal with a positive z component */
		if( z < 0.0 )
			nmg_reverse_face_and_radials( f_top->fu_p , tol );

		/* get OT_SAME use of top face */
		fu = f_top->fu_p;
		if( fu->orientation != OT_SAME )
			fu = fu->fumate_p;

		NMG_CK_FACEUSE( fu );

		/* fu is now known to be a correctly oriented faceuse,
		 * propagate this throughout the shell, face by face, by
		 * traversing the shell using the radial edge structure */

		nmg_propagate_normals( fu , flags , tol );

		/* check if all the faces have been processed */
		missed_faces = 0;
		for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
		{
			NMG_CK_FACEUSE( fu );
			if( fu->orientation == OT_SAME )
			{
				if( !NMG_INDEX_TEST( flags , fu->f_p ) )
					missed_faces++;
			}
		}
	}

	/* free some memory */
	rt_free( (char *)flags , "nmg_fix_normals: flags" );
}

/*	N M G _ B R E A K _ L O N G _ E D G E S
 *
 *	This codes looks for situations as illustrated:
 *
 *    *------->*-------->*--------->*
 *    *<----------------------------*
 *
 *	where one long edgeuse (the bottom one above) and two or more
 *	shorter edgeusess (the tops ones) are collinear and have the same
 *	start and end vertices.  The code breaks the longer edgeuse into
 *	ones that can be radials of the shorter ones.
 *	Returns the number of splits performed.
 */

int
nmg_break_long_edges( s , tol )
struct shell *s;
struct rt_tol *tol;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	int split_count=0;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_break_long_edges( s = x%x )\n" , s );

	NMG_CK_SHELL( s );
	RT_CK_TOL( tol );

	/* look at every edgeuse in the shell */
	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );

		for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			NMG_CK_LOOPUSE( lu );

			/* skip loops of a single vertex */
			if( RT_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
				continue;

			for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
			{
				struct vertexuse *vu;

				NMG_CK_EDGEUSE( eu );

				/* look for an edgeuse that terminates on this vertex */
				for( RT_LIST_FOR( vu , vertexuse , &eu->vu_p->v_p->vu_hd ) )
				{
					struct edgeuse *eu1;

					NMG_CK_VERTEXUSE( vu );

					/* skip vertexuses that are not part of an edge */
					if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
						continue;

					eu1 = vu->up.eu_p;

					/* don't consider the edge we already found!!! */
					if( eu1 == eu )
						continue;

					/* if it terminates at the same vertex as eu, skip it */
					if( eu1->eumate_p->vu_p->v_p == eu->eumate_p->vu_p->v_p )
						continue;

					/* get the mate (so it terminates at "vu") */
					eu1 = eu1->eumate_p;

					/* make sure it is collinear with "eu" */
					if( rt_3pts_collinear( eu->vu_p->v_p->vg_p->coord ,
						eu->eumate_p->vu_p->v_p->vg_p->coord ,
						eu1->vu_p->v_p->vg_p->coord , tol ) )
					{
						/* make sure we break the longer edge
						 * and that the edges are in opposite directions */
						vect_t v0,v1;
						struct edgeuse *tmp_eu;

						VSUB2( v0 , eu->eumate_p->vu_p->v_p->vg_p->coord , eu->vu_p->v_p->vg_p->coord );
						VSUB2( v1 , eu1->eumate_p->vu_p->v_p->vg_p->coord , eu1->vu_p->v_p->vg_p->coord );

						if (MAGSQ( v0 ) > MAGSQ( v1 ) && VDOT( v0 , v1 ) < 0.0 )
						{
							if( rt_g.NMG_debug & DEBUG_BASIC )
							{
								rt_log( "Breaking edge from ( %f %f %f ) to ( %f %f %f ) at ( %f %f %f )\n",
									V3ARGS( eu->vu_p->v_p->vg_p->coord ),
									V3ARGS( eu->eumate_p->vu_p->v_p->vg_p->coord ),
									V3ARGS( eu1->vu_p->v_p->vg_p->coord ) );
							}
							tmp_eu = nmg_esplit(eu1->vu_p->v_p, eu);
							split_count++;
						}
						else if( rt_g.NMG_debug & DEBUG_BASIC )
						{
							rt_log( "Not splitting collinear edges x%x and x%x:\n", eu , eu1 );
							rt_log( "\t( %f %f %f ) -> ( %f %f %f )\n",
								V3ARGS( eu->vu_p->v_p->vg_p->coord ),
								V3ARGS( eu->eumate_p->vu_p->v_p->vg_p->coord ) );
							rt_log( "\t( %f %f %f ) -> ( %f %f %f )\n",
								V3ARGS( eu1->vu_p->v_p->vg_p->coord ),
								V3ARGS( eu1->eumate_p->vu_p->v_p->vg_p->coord ) );
						}
					}
				}
			}
		}
	}
	return( split_count );
}

/*	N M G _ M K _ N E W _ F A C E _ F R O M _ L O O P
 *
 *  Remove a loopuse from an existing face and construct a new face
 *  from that loop
 *
 *  Returns new faceuse as built by nmg_mf()
 *
 */
struct faceuse *
nmg_mk_new_face_from_loop( lu )
struct loopuse *lu;
{
	struct shell *s;
	struct faceuse *fu;
	struct loopuse *lu1;
	struct loopuse *lu_mate;
	int ot_same_loops=0;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_mk_new_face_from_loop( lu = x%x )\n" , lu );

	NMG_CK_LOOPUSE( lu );

	if( *lu->up.magic_p != NMG_FACEUSE_MAGIC )
	{
		rt_log( "nmg_mk_new_face_from_loop: loopuse is not in a faceuse\n" );
		return( (struct faceuse *)NULL );
	}

	fu = lu->up.fu_p;
	NMG_CK_FACEUSE( fu );

	s = fu->s_p;
	NMG_CK_SHELL( s );

	/* Count the number of exterior loops in this faceuse */
	for( RT_LIST_FOR( lu1 , loopuse , &fu->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu1 );
		if( lu1->orientation == OT_SAME )
			ot_same_loops++;
	}

	if( ot_same_loops == 1 && lu->orientation == OT_SAME )
	{
		rt_log( "nmg_mk_new_face_from_loop: cannot remove only exterior loop from faceuse\n" );
		return( (struct faceuse *)NULL );
	}

	lu_mate = lu->lumate_p;

	/* remove loopuse from faceuse */
	RT_LIST_DEQUEUE( &lu->l );

	/* remove its mate from faceuse mate */
	RT_LIST_DEQUEUE( &lu_mate->l );

	/* insert these loops in the shells list of wire loops
	 * put the original loopuse at the head of the list
	 * so that it will end up as the returned faceuse from "nmg_mf"
	 */
	RT_LIST_INSERT( &s->lu_hd , &lu_mate->l );
	RT_LIST_INSERT( &s->lu_hd , &lu->l );

	/* set the "up" pointers to the shell */
	lu->up.s_p = s;
	lu_mate->up.s_p = s;

	/* Now make the new face */
	return( nmg_mf( lu ) );
}

/* state for nmg_split_loops_into_faces */
struct nmg_split_loops_state
{
	long		*flags;		/* index based array of flags for model */
	struct rt_tol	*tol;
	int		split;		/* count of faces split */
};
/* state for nmg_unbreak_edge */

void
nmg_split_loops_handler( fu_p , sl_state , after )
long *fu_p;
genptr_t sl_state;
int after;
{
	struct faceuse *fu;
	struct nmg_split_loops_state *state;
	struct loopuse *lu;
	struct rt_tol *tol;
	int otsame_loops=0;
	int otopp_loops=0;

	fu = (struct faceuse *)fu_p;
	NMG_CK_FACEUSE( fu );

	state = (struct nmg_split_loops_state *)sl_state;
	tol = state->tol;

	if( fu->orientation != OT_SAME )
		return;

	if( !NMG_INDEX_TEST_AND_SET( state->flags , fu ) )  return;

	NMG_INDEX_SET( state->flags , fu->fumate_p );

	for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu );

		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		if( lu->orientation == OT_SAME )
			otsame_loops++;
		else if( lu->orientation == OT_OPPOSITE )
			otopp_loops++;
		else
		{
			rt_log( "nmg_split_loops_into_faces: facuse (x%x) with %s loopuse (x%x)\n",
				fu , nmg_orientation( lu->orientation ) , lu );
			return;
		}
	}

	if( otopp_loops && otsame_loops )
	{
		struct faceuse *new_fu;
		struct loopuse *lu1;
		int first=1;

		lu = RT_LIST_FIRST( loopuse , &fu->lu_hd );
		while( RT_LIST_NOT_HEAD( lu , &fu->lu_hd ) )
		{
			struct loopuse *next_lu;

			next_lu = RT_LIST_PNEXT( loopuse , &lu->l );
			if( lu->orientation == OT_OPPOSITE )
			{
				struct loopuse *lu1;
				int inside=0;

				/* check if this loop is within another loop */
				for( RT_LIST_FOR( lu1 , loopuse , &fu->lu_hd ) )
				{
					int class;

					if( lu == lu1 || lu1->orientation != OT_SAME )
						continue;

					class = nmg_classify_lu_lu( lu , lu1 , tol );
					if( class == NMG_CLASS_AinB )
					{
						inside = 1;
						break;
					}
				}

				if( !inside )
				{
					/* this loop is not a hole inside another loop
					 * so make it a face of its own */
					plane_t plane;

					NMG_GET_FU_PLANE( plane , fu->fumate_p );
					new_fu = nmg_mk_new_face_from_loop( lu );
					nmg_face_g( new_fu , plane );
					nmg_lu_reorient( lu , tol );
				}
			}
			else
			{
				plane_t plane;

				if( first )
					first = 0;
				else
				{
					NMG_GET_FU_PLANE( plane , fu );
					new_fu = nmg_mk_new_face_from_loop( lu );
					nmg_face_g( new_fu , plane );
				}
			}
			lu = next_lu;
		}
	}
	else
	{
		/* all loops are of the same orientation, just make a face for every loop */
		int first=1;
		struct faceuse *new_fu;

		lu = RT_LIST_FIRST( loopuse , &fu->lu_hd );
		while( RT_LIST_NOT_HEAD( lu , &fu->lu_hd ) )
		{
			struct loopuse *next_lu;

			next_lu = RT_LIST_PNEXT( loopuse , &lu->l );

			if( first )
				first = 0;
			else
			{
				plane_t plane;

				if( lu->orientation == OT_SAME )
				{
					NMG_GET_FU_PLANE( plane , fu );
				}
				else
				{
					NMG_GET_FU_PLANE( plane , fu->fumate_p );
				}
				new_fu = nmg_mk_new_face_from_loop( lu );
				nmg_face_g( new_fu , plane );
			}

			lu = next_lu;
		}
	}

	/* XXXX Need code for faces with OT_OPPOSITE loops */
}

/*	N M G _ S P L I T _ L O O P S _ I N T O _ F A C E S
 *
 *	Visits each faceuse and splits disjoint loops into
 *	seperate faces.
 *
 *	Returns the number of faces modified.
 */
int
nmg_split_loops_into_faces( magic_p , tol )
long		*magic_p;
struct rt_tol	*tol;
{
	struct model *m;
	struct nmg_visit_handlers htab;
	struct nmg_split_loops_state sl_state;
	long *flags;
	int count;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_split_loops_into_faces( magic_p = x%x )\n" , magic_p );

	RT_CK_TOL( tol );

	m = nmg_find_model( magic_p );
	NMG_CK_MODEL( m );

	RT_CK_TOL( tol );

	htab = nmg_visit_handlers_null;		/* struct copy */
	htab.aft_faceuse = nmg_split_loops_handler;

	sl_state.split = 0;
	sl_state.flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "nmg_split_loops_into_faces: flags" );
	sl_state.tol = tol;

	nmg_visit( magic_p , &htab , (genptr_t *)&sl_state );

	count = sl_state.split;

	rt_free( (char *)sl_state.flags , "nmg_unbreak_region_edges: flags" );

	return( count );
}

/*	N M G _ D E C O M P O S E _ S H E L L
 *
 *	Accepts one shell and breaks it to the minimum number
 *	of disjoint shells.
 *
 *	explicit returns:
 *		# of resulting shells ( 1 indicates that nothing was done )
 *
 *	implicit returns:
 *		additional shells in the passed in shell's region.
 */
int
nmg_decompose_shell( s , tol )
struct shell *s;
struct rt_tol *tol;
{
	int missed_faces;
	int no_of_shells=1;
	int shell_no=1;
	int i,j;
	struct model *m;
	struct nmgregion *r;
	struct shell *new_s;
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct edgeuse *eu1;
	struct faceuse *missed_fu;
	struct nmg_ptbl stack;
	struct nmg_ptbl shared_edges;
	long *flags;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_decompose_shell( s = x%x )\n" , s );

	NMG_CK_SHELL( s );

	RT_CK_TOL( tol );

	(void)nmg_split_loops_into_faces( &s->l.magic , tol );

	/* Make an index table to insure we visit each face once and only once */
	r = s->r_p;
	NMG_CK_REGION( r );
	m = r->m_p;
	NMG_CK_MODEL( m );
	flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "nmg_decompose_shell: flags" );

	nmg_tbl( &stack , TBL_INIT , NULL );
	nmg_tbl( &shared_edges , TBL_INIT , NULL );

	/* get first faceuse from shell */
	fu = RT_LIST_FIRST( faceuse , &s->fu_hd );
	NMG_CK_FACEUSE( fu );
	if( fu->orientation != OT_SAME )
		fu = fu->fumate_p;
	if( fu->orientation != OT_SAME )
		rt_bomb( "First face in shell has no OT_SAME uses!!!!\n" );

	/* put all edguses of first faceuse on the stack */
	for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu );
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )	
			continue;

		for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
		{
			int face_count=1;

			NMG_CK_EDGEUSE( eu );

			/* count radial faces on this edge */
			eu1 = eu->eumate_p->radial_p;
			while( eu1 != eu && eu1 != eu->eumate_p )
			{
				/* ignore other shells */
				if( nmg_find_s_of_eu( eu1 ) == s )
					face_count++;
				eu1 = eu1->eumate_p->radial_p;
			}

			/* build two lists, one of winged edges, the other not */
			if( face_count > 2 )
				nmg_tbl( &shared_edges , TBL_INS , (long *)eu );
			else
				nmg_tbl( &stack , TBL_INS , (long *)eu );
		}
	}

	/* Mark first faceuse with shell number */
	NMG_INDEX_ASSIGN( flags , fu , shell_no );

	/* now pop edgeuse of the stack and visit faces radial to edgeuse */
	while( (eu1 = nmg_pop_eu( &stack )) != (struct edgeuse *)NULL )
	{
		NMG_CK_EDGEUSE( eu1 );

		/* move to the radial */
		eu = eu1->radial_p;
		NMG_CK_EDGEUSE( eu );

		/* make sure we stay within the intended shell */
		while( nmg_find_s_of_eu( eu ) != s && eu != eu1 && eu != eu1->eumate_p )
			eu = eu->eumate_p->radial_p;

		if( eu == eu1 || eu == eu1->eumate_p )
			continue;

		fu = nmg_find_fu_of_eu( eu );
		NMG_CK_FACEUSE( fu );

		if( fu->orientation != OT_SAME )
			fu = fu->fumate_p;

		/* if this faceuse has already been visited, skip it */
		if( !NMG_INDEX_TEST( flags , fu ) )
		{
                        /* push all "winged" edgeuses of "fu" onto the stack */
                        for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
                        {
                                NMG_CK_LOOPUSE( lu );

                                if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
                                        continue;

                                for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
                                {
                                	int face_count=1;

                                	NMG_CK_EDGEUSE( eu );

					/* count radial faces on this edge */
					eu1 = eu->eumate_p->radial_p;
					while( eu1 != eu && eu1 != eu->eumate_p )
					{
						/* ignore other shells */
						if( nmg_find_s_of_eu( eu1 ) == s )
							face_count++;
						eu1 = eu1->eumate_p->radial_p;
					}

					/* build two lists, one of winged edges, the other not */
					if( face_count > 2 )
						nmg_tbl( &shared_edges , TBL_INS , (long *)eu );
					else
						nmg_tbl( &stack , TBL_INS , (long *)eu );
                                }
                        }
			/* Mark this faceuse and its mate with a shell number */
			NMG_INDEX_ASSIGN( flags , fu , shell_no );
			NMG_INDEX_ASSIGN( flags , fu->fumate_p , shell_no );
		}
	}

	/* count number of faces that were not visited */
	missed_faces = 0;
	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		if( fu->orientation == OT_SAME )
		{
			if( !NMG_INDEX_TEST( flags , fu ) )
			{
				missed_faces++;
				missed_fu = fu;
			}
		}
	}

	if( !missed_faces )	/* nothing to do, just one shell */
	{
		rt_free( (char *)flags , "nmg_decompose_shell: flags " );
		nmg_tbl( &stack , TBL_FREE , NULL );
		nmg_tbl( &shared_edges , TBL_FREE , NULL );
		return( no_of_shells );
	}

	while( missed_faces )
	{
		struct edgeuse *unassigned_eu;
		int *shells_at_edge;
		int new_shell_no=0;

		nmg_tbl( &stack , TBL_RST , NULL );

		/* Look at the list of shared edges to see if anything can be deduced */
		shells_at_edge = (int *)rt_calloc( no_of_shells+1 , sizeof( int ) , "nmg_decompose_shell: shells_at_edge" );

		for( i=0 ; i<NMG_TBL_END( &shared_edges ) ; i++ )
		{
			int faces_at_edge=0;

			unassigned_eu = NULL;

			eu = (struct edgeuse *)NMG_TBL_GET( &shared_edges , i );
			NMG_CK_EDGEUSE( eu );

			eu1 = eu;
			do
			{
				faces_at_edge++;
				fu = nmg_find_fu_of_eu( eu1 );
				if( !NMG_INDEX_TEST( flags , fu ) )
					unassigned_eu = eu1;
				shells_at_edge[ NMG_INDEX_GET( flags , fu ) ]++;
				eu1 = eu1->eumate_p->radial_p;
			}
			while( eu1 != eu );

			if( shells_at_edge[0] == 1 && unassigned_eu )
			{
				/* Only one face at this edge is unassigned, should be
				 * able to determine which shell it belongs in */

				/* Make sure everything is O.K. so far */
				if( faces_at_edge & 1 )
					rt_bomb( "nmg_decompose_shell: Odd number of faces at edge\n" );

				for( j=1 ; j<no_of_shells ; j++ )
				{
					if( shells_at_edge[j] == 1 )
					{
						/* the unassigned face could belong to shell number j */
						if( new_shell_no )
						{
							/* more than one shell has only one face here
							 * cannot determine which to choose */
							new_shell_no = 0;
							break;
						}
						else
							new_shell_no = j;
					}
				}
			}
		}

		rt_free( (char *)shells_at_edge , "nmg_decompose_shell: shells_at_edge" );

		/* make a new shell number */
		if( new_shell_no )
		{
			shell_no = new_shell_no;
			fu = nmg_find_fu_of_eu( unassigned_eu );
		}
		else
		{
			shell_no = (++no_of_shells);
			NMG_CK_FACEUSE( missed_fu );
			fu = missed_fu;
		}

		if( fu->orientation != OT_SAME )
			fu = fu->fumate_p;

		if( !NMG_INDEX_TEST( flags , fu ) )
		{
			/* move this missed face to the new shell */

                        /* push all edgeuses of "fu" onto the stack */
                        for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
                        {
                                NMG_CK_LOOPUSE( lu );

                                if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
                                        continue;

                                for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
                                {
                                	int face_count = 1;

                                	NMG_CK_EDGEUSE( eu );

					/* count radial faces on this edge */
					eu1 = eu->eumate_p->radial_p;
					while( eu1 != eu && eu1 != eu->eumate_p )
					{
						/* ignore other shells */
						if( nmg_find_s_of_eu( eu1 ) == s )
							face_count++;
						eu1 = eu1->eumate_p->radial_p;
					}

					/* build two lists, one of winged edges, the other not */
					if( face_count > 2 )
						nmg_tbl( &shared_edges , TBL_INS , (long *)eu );
					else
						nmg_tbl( &stack , TBL_INS , (long *)eu );
                                }
                        }

			/* Mark this faceuse with a shell number */
			NMG_INDEX_ASSIGN( flags , fu , shell_no );
			NMG_INDEX_ASSIGN( flags , fu->fumate_p , shell_no );

		}
		else
			rt_bomb( "nmg_decompose_shell: Missed face wasn't missed???\n" );

		/* now pop edgeuse of the stack and visit faces radial to edgeuse */
		while( (eu1 = nmg_pop_eu( &stack )) != (struct edgeuse *)NULL )
		{
			NMG_CK_EDGEUSE( eu1 );

			/* move to the radial */
			eu = eu1->radial_p;
			NMG_CK_EDGEUSE( eu );

			/* stay within the original shell */
			while( nmg_find_s_of_eu( eu ) != s && eu != eu1 && eu != eu1->eumate_p )
				eu = eu->eumate_p->radial_p;

			if( eu == eu1 || eu == eu1->eumate_p )
				continue;

			fu = nmg_find_fu_of_eu( eu );
			NMG_CK_FACEUSE( fu );

			/* if this face has already been visited, skip it */
			if( !NMG_INDEX_TEST( flags , fu ) )
			{
	                        /* push all edgeuses of "fu" onto the stack */
	                        for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	                        {
	                                NMG_CK_LOOPUSE( lu );
	                                if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
	                                        continue;
	                                for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
	                                {
	                                	int face_count = 1;
	                                	NMG_CK_EDGEUSE( eu );

						/* count radial faces on this edge */
						eu1 = eu->eumate_p->radial_p;
						while( eu1 != eu && eu1 != eu->eumate_p )
						{
							/* ignore other shells */
							if( nmg_find_s_of_eu( eu1 ) == s )
								face_count++;
							eu1 = eu1->eumate_p->radial_p;
						}

						/* build two lists, one of winged edges, the other not */
						if( face_count > 2 )
							nmg_tbl( &shared_edges , TBL_INS , (long *)eu );
						else
							nmg_tbl( &stack , TBL_INS , (long *)eu );
	                                }
	                        }

				/* Mark this faceuse with a shell number */
				NMG_INDEX_ASSIGN( flags , fu , shell_no );
				NMG_INDEX_ASSIGN( flags , fu->fumate_p , shell_no );

			}
		}

		/* count number of faces that were not visited */
		missed_faces = 0;
		for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
		{
			NMG_CK_FACEUSE( fu );
			if( fu->orientation == OT_SAME )
			{
				if( !NMG_INDEX_TEST( flags , fu ) )
				{
					missed_faces++;
					missed_fu = fu;
				}
			}
		}
	}

	/* Now build the new shells */
	for( shell_no=2 ; shell_no<=no_of_shells ; shell_no++ )
	{

		/* Make a shell */
		new_s = nmg_msv( r );

		/* Move faces marked with this shell_no to this shell */
		fu = RT_LIST_FIRST( faceuse , &s->fu_hd );
		while( RT_LIST_NOT_HEAD( fu , &s->fu_hd ) )
		{
			struct faceuse *next_fu;

			next_fu = RT_LIST_NEXT( faceuse , &fu->l );
			while( RT_LIST_NOT_HEAD( next_fu , &s->fu_hd ) && next_fu->orientation != OT_SAME )
				next_fu = RT_LIST_NEXT( faceuse , &next_fu->l );

			if( fu->orientation != OT_SAME )
			{
				fu = next_fu;
				continue;
			}

			if( NMG_INDEX_GET( flags , fu ) == shell_no )
				nmg_mv_fu_between_shells( new_s , s , fu );

			fu = next_fu;
		}
		if( nmg_kvu( new_s->vu_p ) )
			rt_bomb( "nmg_decompose_shell: killed shell vertex, emptied shell!!\n" );
	}
	rt_free( (char *)flags , "nmg_decompose_shell: flags " );
	nmg_tbl( &stack , TBL_FREE , NULL );
	nmg_tbl( &shared_edges , TBL_FREE , NULL );
	return( no_of_shells );
}

/*
 *			N M G _ S T A S H _ M O D E L _ T O _ F I L E
 *
 *  Store an NMG model as a separate .g file, for later examination.
 *  Don't free the model, as the caller may still have uses for it.
 */
void
nmg_stash_model_to_file( filename, m, title )
CONST char		*filename;
CONST struct model	*m;
CONST char		*title;
{
	FILE	*fp;
	struct rt_external	ext;
	struct rt_db_internal	intern;
	union record		rec;

	rt_log("nmg_stash_model_to_file('%s', x%x, %s)\n", filename, m, title);

	NMG_CK_MODEL(m);

	if( (fp = fopen(filename, "w")) == NULL )  {
		perror(filename);
		return;
	}

	RT_INIT_DB_INTERNAL(&intern);
	intern.idb_type = ID_NMG;
	intern.idb_ptr = (genptr_t)m;
	RT_INIT_EXTERNAL( &ext );

	/* Scale change on export is 1.0 -- no change */
	if( rt_functab[ID_NMG].ft_export( &ext, &intern, 1.0 ) < 0 )  {
		rt_log("nmg_stash_model_to_file: solid export failure\n");
		db_free_external( &ext );
		rt_bomb("nmg_stash_model_to_file() ft_export() error\n");
	}
	NAMEMOVE( "error", ((union record *)ext.ext_buf)->s.s_name );

	bzero( (char *)&rec, sizeof(rec) );
	rec.u_id = ID_IDENT;
	strcpy( rec.i.i_version, ID_VERSION );
	strncpy( rec.i.i_title, title, sizeof(rec.i.i_title)-1 );
	fwrite( (char *)&rec, sizeof(rec), 1, fp );
	fwrite( ext.ext_buf, ext.ext_nbytes, 1, fp );
	fclose(fp);
	db_free_external( &ext );
	rt_log("nmg_stash_model_to_file(): wrote '%s' in %d bytes\n", filename, ext.ext_nbytes);
}

/* state for nmg_unbreak_edge */
struct nmg_unbreak_state
{
	long		*flags;		/* index based array of flags for model */
	int		unbroken;	/* count of edges mended */
};

/*
 *			N M G _ U N B R E A K _ H A N D L E R
 *
 *	edge visit routine for nmg_unbreak_region_edges.
 *
 *	checks if edge "e" is a candidate to unbroken,
 *	i.e., if it was brokem earlier by nmg_ebreak and
 *	now may be mended.
 *	looks for two consectutive edgeuses sharing the same
 *	edge geometry. Checks that the middle vertex has no
 *	other uses, and,  if so, kills the second edge.
 *	Also moves the vu of the first edgeuse mate to the vu
 *	of the killed edgeuse mate.
 */
void
nmg_unbreak_handler( ep , state , after )
long	*ep;
genptr_t state;
int	after;
{
	struct edgeuse *eu1,*eu2;
	struct edge *e;
	struct edge_g *eg;
	struct nmg_unbreak_state *ub_state;
	struct vertex	*vb;
	struct vertexuse *vu;

	e = (struct edge *)ep;
	NMG_CK_EDGE( e );

	ub_state = (struct nmg_unbreak_state *)state;

	/* make sure we only visit this edge once */
	if( !NMG_INDEX_TEST_AND_SET( ub_state->flags , e ) )  return;

	eg = e->eg_p;
	if( !eg )  {
		rt_log( "nmg_unbreak_handler: no geomtry for edge x%x\n" , e );
		return;
	}


	/* if the edge geometry doesn't have at least two uses, this
	 * is not a candidate for unbreaking */		
	if( eg->usage < 2 )  {
		/* rt_log("nmg_unbreak_handler: usage < 2\n"); */
		return;
	}

	/* Check for two consecutive uses, by looking forward. */
	eu1 = e->eu_p;
	NMG_CK_EDGEUSE( eu1 );
	eu2 = RT_LIST_PNEXT_CIRC( edgeuse , eu1 );
	if( eu2->e_p->eg_p != eg )
	{
		/* Can't look backward here, or nmg_unbreak_edge()
		 * will be asked to kill *this* edgeuse, which
		 * will blow our caller's mind.
		 */
		/* rt_log("nmg_unbreak_handler: edge geom not shared\n"); */
		return;
	}
	vb = eu2->vu_p->v_p;
	NMG_CK_VERTEX(vb);

	/* at this point, the situation is:

		     eu1          eu2
		*----------->*----------->*
		A------------B------------C
		*<-----------*<-----------*
		    eu1mate      eu2mate
	*/
	if( nmg_unbreak_edge( eu1 ) != 0 )  return;

	/* keep a count of unbroken edges */
	ub_state->unbroken++;

#if 0
	/* See if vertex "B" survived, meaning it has other uses */
	if( vb->l.magic != NMG_VERTEX_MAGIC )  return;

	/* It's really unclear how to proceed here. No context info. */
#endif
}

/*
 *			N M G _ U N B R E A K _ R E G I O N _ E D G E S
 *
 *	Uses the visit handler to call nmg_unbreak_handler for
 *	each edge below the region (or any other NMG element).
 *
 *	returns the number of edges mended
 */
int
nmg_unbreak_region_edges( magic_p )
long		*magic_p;
{
	struct model *m;
	struct nmg_visit_handlers htab;
	struct nmg_unbreak_state ub_state;
	long *flags;
	int count;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_unbreak_region_edges( magic_p = x%x )\n" , magic_p );

	m = nmg_find_model( magic_p );
	NMG_CK_MODEL( m );	

	htab = nmg_visit_handlers_null;		/* struct copy */
	htab.vis_edge = nmg_unbreak_handler;

	ub_state.unbroken = 0;
	ub_state.flags = (long *)rt_calloc( m->maxindex+2 , sizeof( long ) , "nmg_unbreak_region_edges: flags" );

	nmg_visit( magic_p , &htab , (genptr_t *)&ub_state );

	count = ub_state.unbroken;

	rt_free( (char *)ub_state.flags , "nmg_unbreak_region_edges: flags" );

	return( count );
}

/*
 *			R T _ D I S T _ P T 3 _ L I N E 3
 *
 *  Find the distance from a point P to a line described
 *  by the endpoint A and direction dir, and the point of closest approach (PCA).
 *
 *			P
 *		       *
 *		      /.
 *		     / .
 *		    /  .
 *		   /   . (dist)
 *		  /    .
 *		 /     .
 *		*------*-------->
 *		A      PCA	dir
 *
 *  There are three distinct cases, with these return codes -
 *	0	P is within tolerance of point A.  *dist = 0, pca=A.
 *	1	P is within tolerance of line.  *dist = 0, pca=computed.
 *	2	P is "above/below" line.  *dist=|PCA-P|, pca=computed.
 *
 *
 * XXX For efficiency, a version of this routine that provides the
 * XXX distance squared would be faster.
 */
int
rt_dist_pt3_line3( dist, pca, a, dir, p, tol )
fastf_t		*dist;
point_t		pca;
CONST point_t	a, p;
CONST vect_t	dir;
CONST struct rt_tol *tol;
{
	vect_t	AtoP;		/* P-A */
	vect_t	unit_dir;	/* unitized dir vector */
	fastf_t	A_P_sq;		/* |P-A|**2 */
	fastf_t	t;		/* distance along ray of projection of P */
	fastf_t	dsq;		/* sqaure of distance from p to line */

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "rt_dist_pt3_line3( a=( %f %f %f ), dir=( %f %f %f ), p=( %f %f %f )\n" ,
			V3ARGS( a ) , V3ARGS( dir ) , V3ARGS( p ) );

	RT_CK_TOL(tol);

	/* Check proximity to endpoint A */
	VSUB2(AtoP, p, a);
	if( (A_P_sq = MAGSQ(AtoP)) < tol->dist_sq )  {
		/* P is within the tol->dist radius circle around A */
		VMOVE( pca, a );
		*dist = 0.0;
		return( 0 );
	}

	VMOVE( unit_dir , dir );
	VUNITIZE( unit_dir );

	/* compute distance (in actual units) along line to PROJECTION of
	 * point p onto the line: point pca
	 */
	t = VDOT(AtoP, unit_dir);

	VJOIN1( pca , a , t , unit_dir );
	if( (dsq = A_P_sq - t*t) < tol->dist_sq )
	{
		/* P is within tolerance of the line */
		*dist = 0.0;
		return( 1 );
	}
	else
	{
		/* P is off line */
		*dist = sqrt( dsq );
		return( 2 );
	}
}

/*
 *	N M G _ M V _ S H E L L _ T O _ R E G I O N
 *
 *  Move a shell from one nmgregion to another.
 *  Will bomb if shell and region aren't in the same model.
 *
 *  returns:
 *	0 - all is well
 *	1 - nmgregion that gave up the shell is now empty!!!!
 *
 */
int
nmg_mv_shell_to_region( s , r )
struct shell *s;
struct nmgregion *r;
{
	int ret_val;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_mv_shell_to_region( s=x%x , r=x%x )\n" , s , r );

	NMG_CK_SHELL( s );
	NMG_CK_REGION( r );

	if( s->r_p == r )
	{
		rt_log( "nmg_mv_shell_to_region: Attempt to move shell to region it is already in\n" );
		return( 0 );
	}

	if( nmg_find_model( &s->l.magic ) != nmg_find_model( &r->l.magic ) )
		rt_bomb( "nmg_mv_shell_to_region: Cannot move shell to a different model\n" );

	RT_LIST_DEQUEUE( &s->l );
	if( RT_LIST_IS_EMPTY( &s->r_p->s_hd ) )
		ret_val = 1;
	else
		ret_val = 0;

	RT_LIST_APPEND( &r->s_hd , &s->l );

	s->r_p = r;

	return( ret_val );
}

/*	N M G _ F I N D _ I S E C T _ F A C E S
 *
 *	Find all faces that contain vertex "new_v"
 *	Put them in a nmg_ptbl "faces"
 *
 *	returns:
 *		the number of faces that contain the vertex
 *
 *	and fills in the table with the faces.
 *	Places a count of edges at this vertex where radial is mate (free_edges)
 */
int
nmg_find_isect_faces( new_v , faces , free_edges , tol )
CONST struct vertex *new_v;
struct nmg_ptbl *faces;
int *free_edges;
CONST struct rt_tol *tol;
{
	struct faceuse *fu;
	struct face_g *fg;
	struct edgeuse *eu;
	struct vertexuse *vu;
	int i;
	int unique;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_find_isect_faces( new_v=x%x , faces=x%x )\n" , new_v , faces );

	NMG_CK_VERTEX( new_v );
	RT_CK_TOL( tol );

	/* loop through vertex's vu list */
	for( RT_LIST_FOR( vu , vertexuse , &new_v->vu_hd ) )
	{
		NMG_CK_VERTEXUSE( vu );
		fu = nmg_find_fu_of_vu( vu );
		if( fu->orientation != OT_SAME )
			continue;;

		NMG_CK_FACEUSE( fu );
		fg = fu->f_p->fg_p;

		/* check if this face is different from the ones on list */
		unique = 1;
		for( i=0 ; i<NMG_TBL_END( faces ) ; i++ )
		{
			struct face *fp;

			fp = (struct face *)NMG_TBL_GET( faces , i );
			if( fp->fg_p == fg || rt_coplanar( fg->N , fp->fg_p->N , tol ) > 0 )
			{
				unique = 0;
				break;
			}
		}

		/* if it is not already on the list, add it */
		if( unique )
		{
			struct edgeuse *eu1;

			nmg_tbl( faces , TBL_INS , (long *)fu->f_p );
			/* Count the number of free edges containing new_v */

			if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
				continue;

			eu1 = vu->up.eu_p;
			if( eu1->eumate_p == eu1->radial_p )
				(*free_edges)++;
			else
			{
				eu1 = RT_LIST_PLAST_CIRC( edgeuse , eu1 );
				if( eu1->eumate_p == eu1->radial_p )
					(*free_edges)++;
			}
		}
	}
	return( NMG_TBL_END( faces ) );
}

/*	N M G _ S I M P L E _ V E R T E X _ S O L V E
 *
 *	given a vertex and a list of faces that should intersect at the
 *	vertex, calculate a new location for the vertex.
 *
 *	returns:
 *		0 - if everything is OK
 *		1 - failure
 *
 *	and modifies the geometry of the vertex to the new location
 */
int
nmg_simple_vertex_solve( new_v , faces , tol )
struct vertex *new_v;
CONST struct nmg_ptbl *faces;
CONST struct rt_tol *tol;
{
	struct vertex_g *vg;
	struct rt_tol tol_tmp;
	int failed=0;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_simple_vertex_solve( new_v=x%x , %d faces )\n" , new_v , NMG_TBL_END( faces ));

	NMG_CK_VERTEX( new_v );
	RT_CK_TOL( tol );

	vg = new_v->vg_p;
	NMG_CK_VERTEX_G( vg );

	switch( NMG_TBL_END( faces ) )
	{
		struct face *fp1,*fp2,*fp3;
		plane_t pl1;
		fastf_t	vert_move_len;

		case 0:
			rt_log( "nmg_simple_vertex_solve: vertex not in any face planes!!!\n" );
			failed = 1;
			break;
		case 1:
			fp1 = (struct face *)NMG_TBL_GET( faces , 0 );
			vert_move_len = DIST_PT_PLANE( vg->coord , fp1->fg_p->N );
			VJOIN1( vg->coord , vg->coord , -vert_move_len , fp1->fg_p->N );
			break;
		case 2:
			fp1 = (struct face *)NMG_TBL_GET( faces , 0 );
			fp2 = (struct face *)NMG_TBL_GET( faces , 1 );

			VCROSS( pl1 , fp1->fg_p->N , fp2->fg_p->N );
			pl1[3] = VDOT( vg->coord , pl1 );
			if( rt_mkpoint_3planes( vg->coord , fp1->fg_p->N , fp2->fg_p->N , pl1 ) )
			{
				rt_log( "nmg_simple_vertex_solve: Cannot find new coords for two planes\n" );
				failed = 1;
				break;
			}
			break;
		case 3:
			fp1 = (struct face *)NMG_TBL_GET( faces , 0 );
			fp2 = (struct face *)NMG_TBL_GET( faces , 1 );
			fp3 = (struct face *)NMG_TBL_GET( faces , 2 );
			if( rt_mkpoint_3planes( vg->coord , fp1->fg_p->N , fp2->fg_p->N , fp3->fg_p->N ) )
			{
				rt_log( "nmg_simple_vertex_solve: failed for  3 planes\n" );
				failed = 1;
				break;
			}
			break;
		default:
			failed = 1;
			rt_log( "nmg_simple_vertex_solve: Called for a complex vertex\n" );
			break;
	}

	return( failed );
}

/*	N M G _ C K _ V E R T _ O N _ F U S
 *
 *  Check all uses of a vertex to see if it lies within tolerance
 *  of all faces where it is used
 *
 * returns:
 *	0 - All is well
 *	1 - vertex is off face plane by at least tol->dist for at least one face
 */
int
nmg_ck_vert_on_fus( v , tol )
CONST struct vertex *v;
CONST struct rt_tol *tol;
{
	struct vertexuse *vu;
	fastf_t max_dist=0.0;
	int ret_val=0;

	NMG_CK_VERTEX( v );
	RT_CK_TOL( tol );

	NMG_CK_VERTEX_G( v->vg_p );

	for( RT_LIST_FOR( vu , vertexuse , &v->vu_hd ) )
	{
		struct faceuse *fu;
		fastf_t dist;

		fu = nmg_find_fu_of_vu( vu );
		if( !fu )
			continue;

		NMG_CK_FACEUSE( fu );
		NMG_CK_FACE( fu->f_p );
		NMG_CK_FACE_G( fu->f_p->fg_p );
		dist = DIST_PT_PLANE( v->vg_p->coord , fu->f_p->fg_p->N );
		dist = (dist < 0.0 ? (-dist) : dist);
		if( dist > tol->dist )
		{
			ret_val = 1;

			if( dist > max_dist )
				max_dist = dist;

			rt_log( "nmg_ck_vert_on_fus: v=x%x vu=x%x ( %f %f %f ) is %g from\n\tfaceuse x%x f x%x\n" , v , vu , V3ARGS( v->vg_p->coord ) , dist , fu , fu->f_p );
		}
	}

	if( ret_val )
		rt_log( "nmg_ck_vert_on_fus: v=x%x ( %f %f %f ) max distance of %g from faceuses\n" , v , V3ARGS( v->vg_p->coord ) , max_dist );

	return( ret_val );
}

/* struct used by nmg_complex_vertex_solve
 * to store info about one edge
 * that contains the vertex in question
 */
struct intersect_fus
{
	struct faceuse *fu[2];	/* fu's that intersect at this edge */
	struct edgeuse *eu;	/* edgeuse in fu[0] that emanates from vertex */
	point_t start;		/* calculated start point of edge line */
	vect_t dir;		/* calculated direction of edge line */
	point_t pt;		/* a point on the edge a small distance from the vertex */
	int free_edge;		/* flag indicating that this is a free edge */
	struct vertex *vp;	/* a vertex pointer for above point */
};

static void
nmg_pr_inter( new_v , int_faces )
CONST struct vertex *new_v;
CONST struct nmg_ptbl *int_faces;
{
	int i;
	struct rt_tol tol;

	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	rt_log( "\nint_faces at vertex x%x ( %f %f %f )\n" , new_v , V3ARGS( new_v->vg_p->coord ) );
	for( i=0 ; i<NMG_TBL_END( int_faces ) ; i++ )
	{
		struct intersect_fus *i_fus;
		struct face *fp1,*fp2;

		i_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , i );

		rt_log( "edge number %d, x%x\n" , i , i_fus );
		if( i_fus->fu[0] )
			fp1 = i_fus->fu[0]->f_p;
		else
			fp1 = NULL;
		if( i_fus->fu[1] )
			fp2 = i_fus->fu[1]->f_p;
		else
			fp2 = NULL;

		rt_log( "\tfu1 = x%x (face=x%x), fu2 = x%x (face=x%x)\n" , i_fus->fu[0] , fp1 , i_fus->fu[1] , fp2 );

		if( i_fus->eu == NULL )
			rt_log( "\teu = NULL\n" );
		else if( i_fus->eu->vu_p == NULL )
			rt_log( "\teu = x%x , vu_p = NULL\n" , i_fus->eu );
		else
		{
			struct faceuse *fu;

			rt_log( "\teu = x%x, from x%x ( %f %f %f ) to x%x ( %f %f %f )\n" , i_fus->eu,
				i_fus->eu->vu_p->v_p , V3ARGS( i_fus->eu->vu_p->v_p->vg_p->coord ),
				i_fus->eu->eumate_p->vu_p->v_p , V3ARGS( i_fus->eu->eumate_p->vu_p->v_p->vg_p->coord ) );
			if( i_fus->fu[0] )
			{
				fu = nmg_find_fu_of_eu( i_fus->eu );
				if( fu != i_fus->fu[0] )
					rt_log( "****ERROR**** eu is not in fu1 it's in x%x\n" , fu );
			}
			else
			{
				fu = nmg_find_fu_of_eu( i_fus->eu );
				if( fu != i_fus->fu[1] )
					rt_log( "****ERROR**** eu is not in fu2, it's in x%x\n" , fu );
			}
			nmg_pr_fu_around_eu( i_fus->eu , &tol );
		}

		rt_log( "\tstart = ( %f %f %f ) , dir = ( %f %f %f )\n" , V3ARGS( i_fus->start ) , V3ARGS( i_fus->dir ) );
		rt_log( "\tpt = ( %f %f %f )\n" , V3ARGS( i_fus->pt ) );
		rt_log( "\tfree_edge = %d\n" , i_fus->free_edge );
		if( i_fus->eu )
		{
			if( i_fus->eu->eumate_p != i_fus->eu->radial_p &&
			    i_fus->free_edge )
				rt_log( "****ERROR**** this is NOT a free edge\n" );
			if( i_fus->eu->eumate_p == i_fus->eu->radial_p &&
			    !i_fus->free_edge )
				rt_log( "****ERROR**** This is a free edge\n" );
		}
		if( i_fus->vp )
			rt_log( "\tvp = x%x ( %f %f %f )\n" , i_fus->vp , V3ARGS( i_fus->vp->vg_p->coord ) );
		else
			rt_log( "\tvp = NULL\n" );
	}
}

static void
nmg_plot_inter( new_v , int_faces )
CONST struct vertex *new_v;
CONST struct nmg_ptbl *int_faces;
{
	int i,j;
	static int num=0;
	char name[24];
	FILE *fp;

	sprintf( name , "inter%d.pl" , num );
	fp = fopen( name , "w" );
	if( fp == NULL )
	{
		rt_log( "Cannot open %s\n" , name );
		perror( "nmg_plot_inter" );
		return;
	}

	rt_log( "Plotting to %s\n" , name );

	for( i=0 ; i<NMG_TBL_END( int_faces ) ; i++ )
	{
		struct intersect_fus *i_fus,*j_fus;

		j = i+1;
		if( j == NMG_TBL_END( int_faces ) )
			j = 0;

		i_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , i );
		j_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , j );

		pdv_3line( fp , new_v->vg_p->coord , i_fus->vp->vg_p->coord );
		pdv_3line( fp , j_fus->vp->vg_p->coord , i_fus->vp->vg_p->coord );
	}

	fclose( fp );
	num++;
}

/*	N M G _ G E T _ E D G E _ L I N E S
 *
 * Fill in the intersect_fus structures for edges around
 * new_v. Does not fill in "pt" or "vp".
 *
 *	returns:
 *		0 - All is well
 *		1 - Failure
 */

static int
nmg_get_edge_lines( new_v , int_faces , tol )
struct vertex *new_v;
struct nmg_ptbl *int_faces;
CONST struct rt_tol *tol;
{
	struct vertex_g *vg;
	struct vertexuse *vu;
	struct edgeuse *eu,*eu1;
	struct faceuse *fu;
	struct rt_tol tol_tmp;
	int done=0;
	int edge_no;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_get_edge_lines( new_v=x%x , int_faces=x%x )\n" , new_v , int_faces );

	NMG_CK_VERTEX( new_v );
	vg = new_v->vg_p;
	NMG_CK_VERTEX_G( vg );
	RT_CK_TOL( tol );

	/* A temporary tolerance struct for times when I don't want tolerancing */
	tol_tmp.magic = RT_TOL_MAGIC;
	tol_tmp.dist = 0.0;
	tol_tmp.dist_sq = 0.0;
	tol_tmp.perp = 0.0;
	tol_tmp.para = 1.0;

	eu1 = (struct edgeuse *)NULL;
	for( RT_LIST_FOR( vu , vertexuse , &new_v->vu_hd ) )
	{
		if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
			continue;

		eu = vu->up.eu_p->eumate_p;
		fu = nmg_find_fu_of_eu( eu );
		if( !fu )
			continue;

		if( fu->orientation != OT_SAME )
			continue;

		if( eu->eumate_p == eu->radial_p )
		{
			vect_t tmp_vec,eu_dir;
			plane_t pl;
			struct intersect_fus *i_fus;

			i_fus = (struct intersect_fus *)rt_malloc( sizeof( struct intersect_fus ) , "nmg_get_edge_lines: i_fus" );
			i_fus->fu[0] = NULL;
			i_fus->fu[1] = fu;
			i_fus->eu = eu;
			i_fus->vp = (struct vertex *)NULL;
			VSET( i_fus->pt , 0.0 , 0.0 , 0.0 );
			i_fus->free_edge = 1;
			eu1 = RT_LIST_PNEXT_CIRC( edgeuse , &eu->l );

			VSUB2( i_fus->dir , eu->vu_p->v_p->vg_p->coord , eu->eumate_p->vu_p->v_p->vg_p->coord );
			VUNITIZE( i_fus->dir );
			NMG_GET_FU_PLANE( pl , fu );
			VJOIN1( i_fus->start , vg->coord , (-DIST_PT_PLANE( vg->coord , pl )) , pl );

			/* Save this info in the int_faces table */
			nmg_tbl( int_faces , TBL_INS , (long *)i_fus );

			break;
		}
	}

	if( !eu1 )
	{
		/* get an edgeuse emanating from new_v */
		vu = RT_LIST_FIRST( vertexuse , &new_v->vu_hd );
		NMG_CK_VERTEXUSE( vu );
		eu1 = vu->up.eu_p;
		NMG_CK_EDGEUSE( eu1 );
		fu = nmg_find_fu_of_eu( eu1 );
		if( fu->orientation != OT_SAME )
			eu1 = eu1->eumate_p;
	}

	eu = eu1;

	/* loop through all the edges emanating from new_v */
	while( !done )
	{
		fastf_t dist;
		vect_t normal1;
		point_t start;
		vect_t dir;
		vect_t eu_dir;
		int ret_val;
		struct intersect_fus *i_fus;
		struct faceuse *fu1,*fu2;

		NMG_CK_EDGEUSE( eu );

		/* get the direction of the original edge (away from the vertex) */
		VSUB2( eu_dir , eu->eumate_p->vu_p->v_p->vg_p->coord , eu->vu_p->v_p->vg_p->coord );

		/* get the two faces that intersect along this edge */
		fu1 = nmg_find_fu_of_eu( eu );
		fu2 = nmg_find_fu_of_eu( eu->radial_p );


		/* initialize the intersect structure for this edge */
		i_fus = (struct intersect_fus *)rt_malloc( sizeof( struct intersect_fus ) , "nmg_inside_vert: intersection list" );
		i_fus->fu[0] = fu1;
		if( eu->radial_p == eu->eumate_p )
		{
			i_fus->fu[1] = (struct faceuse *)NULL;
			i_fus->free_edge = 1;
			done = 1;
		}
		else
		{
			i_fus->fu[1] = fu2;
			i_fus->free_edge = 0;
		}
		i_fus->eu = eu;
		i_fus->vp = (struct vertex *)NULL;
		VSET( i_fus->pt , 0.0 , 0.0 , 0.0 );
		VSET( i_fus->start , 0.0 , 0.0 , 0.0 );
		VSET( i_fus->dir , 0.0 , 0.0 , 0.0 );

		/* if edge is between loops of same face , don't calculate an edge line */
		if( fu1->f_p != fu2->f_p )
		{
			/* find the new edge line at the intersection of these two faces
			 * the line is defined by start and dir */

			NMG_GET_FU_NORMAL( normal1 , fu1 );
			if( ret_val=rt_isect_2planes( start , dir , fu1->f_p->fg_p->N , fu2->f_p->fg_p->N , new_v->vg_p->coord , &tol_tmp ) )
			{
				rt_log( "nmg_inside_vert: Cannot find new edge between two planes\n" );
				rt_log( "return from rt_isect_2planes is %d\n" , ret_val );
				rt_log( "\tplanes are ( %f %f %f %f ) and ( %f %f %f %f )\n" ,
					V4ARGS( fu1->f_p->fg_p->N ),
					V4ARGS( fu2->f_p->fg_p->N ) );
				rt_log( "\tfus x%x and x%x, faces x%x and x%x\n" ,
					fu1, fu2, fu1->f_p, fu2->f_p );
				rt_bomb( "Can't find plane intersection\n" );
			}
			/* Make the start point at closest approach to old vertex */
			(void)rt_dist_pt3_line3( &dist , start , start , dir , new_v->vg_p->coord , tol );

			/* Make sure the calculated direction is away from the vertex */
			if( VDOT( eu_dir , dir ) < 0.0 )
				VREVERSE( dir , dir );
			VMOVE( i_fus->start , start );
			VMOVE( i_fus->dir , dir );
		}
		else if( i_fus->free_edge )
		{
			vect_t tmp_vec;
			plane_t pl;

			NMG_GET_FU_PLANE( pl , fu1 );

			VMOVE( i_fus->dir , eu_dir );
			VUNITIZE( i_fus->dir );

			VJOIN1( i_fus->start , vg->coord , (-DIST_PT_PLANE( vg->coord , pl )) , pl );

		}

		/* Save this info in the int_faces table */
		nmg_tbl( int_faces , TBL_INS , (long *)i_fus );

		if( !done )
		{
			/* move on to the next edge emanating from new_v */
			eu = eu->radial_p;
			eu = RT_LIST_PNEXT_CIRC( edgeuse , eu );
			if( eu == eu1 )
				done = 1;
		}
	}
	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "After getting edge lines:\n" );
		nmg_pr_inter( new_v , int_faces );
	}

	return( 0 );
}

/*	N M G _ G E T _ M A X _ E D G E _ I N T E R S
 *
 * Fill in the "pt" portion of the "intersect_fus" structure
 * for edges around new_v by calculating the intersection with neighboring
 * faces and selecting the furthest one from new_v.
 * Estimate a value for new_v coords as the average of the
 * selected intersection points.
 */
void
nmg_get_max_edge_inters( new_v , int_faces , faces , tol )
struct vertex *new_v;
struct nmg_ptbl *int_faces;
CONST struct nmg_ptbl *faces;
CONST struct rt_tol *tol;
{
	int edge_no;
	point_t ave_pt;
	fastf_t edge_count=0.0;
	int free_edges=0;
	struct intersect_fus *free_fus[2];

	if( rt_g.NMG_debug & DEBUG_BASIC ) 
		rt_log( "nmg_get_max_edge_inters( new_v = x%x , %d intersect_fus structs , %d faces )\n" , new_v , NMG_TBL_END( int_faces ) , NMG_TBL_END( faces ) );

	NMG_CK_VERTEX( new_v );
	RT_CK_TOL( tol );

	VSET( ave_pt , 0.0 , 0.0 , 0.0 );

	/* loop through edges departing from new_v */
	for( edge_no=0 ; edge_no<NMG_TBL_END( int_faces ) ; edge_no++ )
	{
		struct intersect_fus *edge_fus,*other_fus[2];
		fastf_t max_dist;
		int next_edge_no,prev_edge_no;
		int other_index;
		int found1,found2;

		edge_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , edge_no );

		/* don't calculate intersect point for edge between two loops of same face */
		if( edge_fus->fu[0] && edge_fus->fu[1] &&
			 edge_fus->fu[0]->f_p == edge_fus->fu[1]->f_p )
				continue;

		/* Find intersections with other faces and keep the one
		 * furthest up the edge
		 */
		max_dist = (-MAX_FASTF);
		for( other_index=0 ; other_index < NMG_TBL_END( faces ) ; other_index++ )
		{
			struct face *f;
			fastf_t dist;
			int ret_val;

			f = (struct face *)NMG_TBL_GET( faces , other_index );

			if( !f )
				continue;

			/* don't intersect with planes used to calculate edge line */
			if( edge_fus->fu[0] && f == edge_fus->fu[0]->f_p )
				continue;

			if( edge_fus->fu[1] && f == edge_fus->fu[1]->f_p )
				continue;

			/* don't intersect with planes parallel to edge line */
			if( RT_VECT_ARE_PERP( VDOT( edge_fus->dir , f->fg_p->N ) , tol ) )
				continue;

			if( (ret_val=rt_isect_line3_plane( &dist , edge_fus->start , edge_fus->dir , f->fg_p->N , tol )) < 1 )
			{
				if( rt_g.NMG_debug & DEBUG_BASIC )
				{
					rt_log( "return from rt_isect_line3_plane is %d\n" , ret_val );
					rt_log( "\tfor line ( %f %f %f ) -> ( %f %f %f )\n",
						V3ARGS( edge_fus->start ) , V3ARGS( edge_fus->dir ) );
					rt_log( "\tand plane ( %f %f %f %f )\n" , V4ARGS( f->fg_p->N ) );
				}
				continue;
			}
			else if( rt_g.NMG_debug & DEBUG_BASIC )
			{
				rt_log( "return from rt_isect_line3_plane is %d\n" , ret_val );
				rt_log( "\tfor line ( %f %f %f ) -> ( %f %f %f )\n",
					V3ARGS( edge_fus->start ) , V3ARGS( edge_fus->dir ) );
				rt_log( "\tand plane ( %f %f %f %f )\n" , V4ARGS( f->fg_p->N ) );
				rt_log( "\tpl dot dir = %g, tol->perp=%g\n" , VDOT( edge_fus->dir, f->fg_p->N ) , tol->perp );
				rt_log( "\tdist = %f\n" , dist );
			}

			if( dist > max_dist )
				max_dist = dist;
		}

		if( max_dist <= 0.0 )
		{
			VMOVE( edge_fus->pt , edge_fus->start );
		}
		else
		{
			VJOIN1( edge_fus->pt , edge_fus->start , max_dist , edge_fus->dir );
		}

		if( edge_fus->free_edge )
		{
			free_fus[free_edges++] = edge_fus;
			if( free_edges > 2 )
				rt_log( "nmg_get_max_edge_inters: Too may free edges!!\n" );
		}

		VADD2( ave_pt , ave_pt , edge_fus->pt );
		edge_count++;
	}

	if( free_edges == 2 )
	{
		VBLEND2( new_v->vg_p->coord , 0.5 , free_fus[0]->pt , 0.5 , free_fus[1]->pt );
	}
	else
	{
		VSCALE( ave_pt , ave_pt , 1.0/edge_count );
		VMOVE( new_v->vg_p->coord , ave_pt );
	}
}

static void
nmg_fix_folded_inters( new_v , int_faces , tol )
CONST struct vertex *new_v;
struct nmg_ptbl *int_faces;
CONST struct rt_tol *tol;
{
	int edge_no;
	struct vertex_g *vg;

	NMG_CK_VERTEX( new_v );
	vg = new_v->vg_p;
	NMG_CK_VERTEX_G( vg );
	RT_CK_TOL( tol );

	for( edge_no=0 ; edge_no<NMG_TBL_END( int_faces ) ; edge_no++ )
	{
		struct intersect_fus *edge_fus,*next_fus,*prev_fus;
		fastf_t dist[2];
		int next_edge_no,prev_edge_no;
		int ret;

		edge_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , edge_no );

		next_edge_no = edge_no + 1;
		if( next_edge_no == NMG_TBL_END( int_faces ) )
			next_edge_no = 0;

		next_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , next_edge_no );
		ret = rt_dist_line3_lseg3( dist , edge_fus->pt , edge_fus->dir , vg->coord , next_fus->pt , tol );
		if( ret != 1 && ret != 3 && dist[0] > 0.0 )
		{
			VJOIN1( edge_fus->pt , edge_fus->pt , dist[0] , edge_fus->dir );
			continue;
		}

		prev_edge_no = edge_no - 1;
		if( prev_edge_no == (-1) )
			prev_edge_no = NMG_TBL_END( int_faces ) - 1;

		prev_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , prev_edge_no );
		ret = rt_dist_line3_lseg3( dist , edge_fus->pt , edge_fus->dir , vg->coord , prev_fus->pt , tol );
		if( ret != 1 && ret != 3 && dist[0] > 0.0 )
		{
			VJOIN1( edge_fus->pt , edge_fus->pt , dist[0] , edge_fus->dir );
			continue;
		}
	}
}

/*	N M G _ S P L I T _ E D G E S _ A T _ P T S
 *
 * Using the info in the table of intersect_fus structs,
 * split the edgeuse (eu) in each struct at the point (pt)
 * store the new vertices in the structure (vp) and assign
 * the geometry. Any points within tolerance will end up
 * with the same vertex pointer and geometry
 *
 */
static void
nmg_split_edges_at_pts( new_v , int_faces , tol )
CONST struct vertex *new_v;
struct nmg_ptbl *int_faces;
CONST struct rt_tol *tol;
{
	int edge_no;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_split_edges_at_pts( new_v = x%x , %d intersect_fus structs)\n" , new_v , NMG_TBL_END( int_faces ) );

	RT_CK_TOL( tol );
	NMG_CK_VERTEX( new_v );

	/* loop through all edges departing from new_v */
	for( edge_no=0 ; edge_no < NMG_TBL_END( int_faces ) ; edge_no++ )
	{
		struct intersect_fus *i_fus;
		struct edgeuse *new_eu;

		i_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , edge_no );

		/* skip edges between two loops of same face, for now */
		if( i_fus->fu[0] && i_fus->fu[1] && i_fus->fu[0]->f_p == i_fus->fu[1]->f_p )
			continue;

		if( rt_pt3_pt3_equal( new_v->vg_p->coord , i_fus->pt , tol ) )
		{
			vect_t eu_dir;

			/* if pt is within tolerance of new_v, don't split the edge */
			i_fus->vp = (struct vertex *)NULL;
			VMOVE( i_fus->pt , new_v->vg_p->coord );
			VMOVE( i_fus->start , new_v->vg_p->coord );
			VSUB2( i_fus->dir , i_fus->eu->eumate_p->vu_p->v_p->vg_p->coord , i_fus->eu->vu_p->v_p->vg_p->coord );
			VUNITIZE( i_fus->dir );
			continue;
		}
		new_eu = nmg_esplit( i_fus->vp , i_fus->eu );
		i_fus->vp = new_eu->vu_p->v_p;

		/* Need to keep track of correct eu in this case */
		if( i_fus->free_edge && !i_fus->fu[0] )
			i_fus->eu = new_eu;

		/* Assign geometry to the new vertex */
		if( i_fus && !i_fus->vp->vg_p )
			nmg_vertex_gv( i_fus->vp , i_fus->pt );
	}
	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "After splitting edges:\n" );
		nmg_pr_inter( new_v , int_faces );
	}

	/* Now take care of edges between two loops of same face */
	edge_no = 0;
	while( edge_no < NMG_TBL_END( int_faces ) )
	{
		int next_edge_no;
		struct intersect_fus *i_fus,*j_fus;

		next_edge_no = edge_no + 1;
		if( next_edge_no == NMG_TBL_END( int_faces ) )
			next_edge_no = 0;

		i_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , edge_no );
		j_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , next_edge_no );

		while( j_fus->fu[0] && j_fus->fu[1] &&
		       j_fus->fu[0]->f_p == j_fus->fu[1]->f_p &&
		       j_fus != i_fus )
		{
			struct edgeuse *new_eu;
			struct edgeuse *prev_eu;
			struct edgeuse *radial_eu;


			if( i_fus->free_edge && j_fus->free_edge )
				break;

			if( !i_fus->vp )
				break;

			new_eu = nmg_esplit( i_fus->vp , j_fus->eu );
			prev_eu = RT_LIST_PPREV_CIRC( edgeuse , &j_fus->eu->l );
			radial_eu = j_fus->eu->radial_p;

			if( EDGESADJ( prev_eu , j_fus->eu ) )
			{
				nmg_keu( prev_eu );
				nmg_keu( j_fus->eu );
			}
			else
				rt_log( "nmg_split_edges_at_vertex: ERROR - can't find adjacent edges to kill\n" );

			i_fus->fu[1] = j_fus->fu[1];

			if( i_fus->fu[0] && j_fus->fu[1] )
			{
				if( rt_g.NMG_debug & DEBUG_BASIC )
				{
					rt_log( "radial join of eu's x%x and x%x\n" , i_fus->eu , radial_eu );
					rt_log( "\tx%x to x%x and x%x to x%x\n" ,
						i_fus->eu->vu_p->v_p, i_fus->eu->eumate_p->vu_p->v_p,
						radial_eu->vu_p->v_p, radial_eu->eumate_p->vu_p->v_p );
				}
				nmg_radial_join_eu( i_fus->eu , radial_eu , tol );
			}

			if( !i_fus->fu[0] )
				i_fus->eu = radial_eu;
			NMG_CK_EDGEUSE( i_fus->eu );

			if( j_fus->free_edge )
				i_fus->free_edge = 1;

			nmg_tbl( int_faces , TBL_RM , (long *)j_fus );

			if( next_edge_no == NMG_TBL_END( int_faces ) )
				next_edge_no = 0;

			rt_free( (char *)j_fus , "nmg_split_edges_at_pts: j_fus " );

			j_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , next_edge_no );

		}
		edge_no++;
	}
	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "After loops of same face\n" );
		nmg_pr_inter( new_v , int_faces );
	}
}

static void
nmg_fuse_inters( i_fus , j_fus , int_faces , tol )
struct intersect_fus *i_fus;
struct intersect_fus *j_fus;
struct nmg_ptbl *int_faces;
CONST struct rt_tol *tol;
{
	struct edgeuse *radial_eu;
	struct edgeuse *prev_eu;

	radial_eu = j_fus->eu->radial_p;
	prev_eu = RT_LIST_PPREV_CIRC( edgeuse , &j_fus->eu->l );

	if( EDGESADJ( prev_eu , j_fus->eu ) )
	{
		nmg_keu( prev_eu );
		nmg_keu( j_fus->eu );
	}
	else
		rt_log( "nmg_fuse_inter_verts: ERROR: can't find adjacent edges to kill\n" );

	i_fus->fu[1] = j_fus->fu[1];

	if( i_fus->fu[0] && j_fus->fu[1] )
	{
		if( rt_g.NMG_debug & DEBUG_BASIC )
		{
			rt_log( "radial join of eu's x%x and x%x\n" , i_fus->eu , radial_eu );
			rt_log( "\tx%x to x%x and x%x to x%x\n" ,
				i_fus->eu->vu_p->v_p, i_fus->eu->eumate_p->vu_p->v_p,
				radial_eu->vu_p->v_p, radial_eu->eumate_p->vu_p->v_p );
		}
		nmg_radial_join_eu( i_fus->eu , radial_eu , tol );
	}

	if( !i_fus->fu[0] )
		i_fus->eu = radial_eu;
	NMG_CK_EDGEUSE( i_fus->eu );

	if( j_fus->free_edge )
		i_fus->free_edge = 1;

	nmg_tbl( int_faces , TBL_RM , (long *)j_fus );
	rt_free( (char *)j_fus , "nmg_split_edges_at_pts: j_fus " );

}

#if 0
static void
nmg_fuse_inter_verts( new_v , int_faces , tol )
CONST struct vertex *new_v;
struct nmg_ptbl *int_faces;
CONST struct rt_tol *tol;
{
	int edge_no=0;

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "nmg_fuse_inter_verts( new_v=x%x ( %f %f %f ), int_faces=x%x)\n",
			new_v, V3ARGS( new_v->vg_p->coord ) , int_faces );
	}

	NMG_CK_VERTEX( new_v );
	RT_CK_TOL( tol );

	while( edge_no < NMG_TBL_END( int_faces ) )
	{
		struct intersect_fus *edge_fus,*next_fus;
		struct edgeuse *radial_eu;
		struct edgeuse *prev_eu;
		int next_edge_no;

		next_edge_no = edge_no + 1;
		if( next_edge_no == NMG_TBL_END( int_faces ) )
			next_edge_no = 0;

		edge_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , edge_no );
		next_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , next_edge_no );

		if( next_edge_no == 0 && ( edge_fus->free_edge || next_fus->free_edge ) )
		{
			edge_no++;
			continue;
		}

		if( !edge_fus->vp || !next_fus->vp )
		{
			edge_no++;
			continue;
		}

		if( !rt_pt3_pt3_equal( edge_fus->vp->vg_p->coord , next_fus->vp->vg_p->coord , tol ) )
		{
			edge_no++;
			continue;
		}

		radial_eu = next_fus->eu->radial_p;
		prev_eu = RT_LIST_PPREV_CIRC( edgeuse , &next_fus->eu->l );

		if( rt_g.NMG_debug & DEBUG_BASIC )
			rt_log( "\tJoining vertices x%x and x%x\n" , edge_fus->vp , next_fus->vp );

		nmg_jv( edge_fus->vp , next_fus->vp );

		if( EDGESADJ( prev_eu , next_fus->eu ) )
		{
			nmg_keu( prev_eu );
			nmg_keu( next_fus->eu );
		}
		else
			rt_log( "nmg_fuse_inter_verts: ERROR: can't find adjacent edges to kill\n" );

		edge_fus->fu[1] = next_fus->fu[1];

		if( edge_fus->fu[0] && next_fus->fu[1] )
		{
			if( rt_g.NMG_debug & DEBUG_BASIC )
			{
				rt_log( "radial join of eu's x%x and x%x\n" , edge_fus->eu , radial_eu );
				rt_log( "\tx%x to x%x and x%x to x%x\n" ,
					edge_fus->eu->vu_p->v_p, edge_fus->eu->eumate_p->vu_p->v_p,
					radial_eu->vu_p->v_p, radial_eu->eumate_p->vu_p->v_p );
			}
			nmg_radial_join_eu( edge_fus->eu , radial_eu , tol );
		}

		if( !edge_fus->fu[0] )
			edge_fus->eu = radial_eu;
		NMG_CK_EDGEUSE( edge_fus->eu );

		if( next_fus->free_edge )
			edge_fus->free_edge = 1;

		nmg_tbl( int_faces , TBL_RM , (long *)next_fus );
		rt_free( (char *)next_fus , "nmg_split_edges_at_pts: next_fus " );
	}
	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "After vertex fusion:\n" );
		nmg_pr_inter( new_v , int_faces );
	}
}
#endif

/*	N M G _ F U S E _ C O L L I N E A R _ E D G E S
 *
 * Eliminates adjacent intersect_fus structs with collinear edges
 *
 */
static void
nmg_simplify_inter( new_v , int_faces , tol )
CONST struct vertex *new_v;
struct nmg_ptbl *int_faces;
CONST struct rt_tol *tol;
{
	int edge_no=0;
	int next_edge_no;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_fuse_collinear_edges( new_v=x%x ( %f %f %f ), int_faces=x%x)\n",
			new_v, V3ARGS( new_v->vg_p->coord ) , int_faces );

	NMG_CK_VERTEX( new_v );
	RT_CK_TOL( tol );

	while( edge_no < NMG_TBL_END( int_faces ) )
	{
		struct intersect_fus *i_fus;
		struct intersect_fus *j_fus;
		struct vertexuse *vu1,*vu2;
		struct edgeuse *eu;
		struct loopuse *lu;
		struct loopuse *new_lu;
		struct loopuse *dup_lu;
		struct faceuse *new_fu;
		struct faceuse *fu;
		long **trans_tbl;

		/* get two consectutive structures */
		i_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , edge_no );
		next_edge_no = edge_no+1;
		if( next_edge_no == NMG_TBL_END( int_faces ) )
			 next_edge_no = 0;
		j_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , next_edge_no );

		/* skip open space */
		if( (i_fus->free_edge || j_fus->free_edge) && next_edge_no == 0 )
		{
			edge_no++;
			continue;
		}

		/* Don't fuse free edges together */
		if( i_fus->free_edge && j_fus->free_edge )
		{
			edge_no++;
			continue;
		}

		/* if either vertex or edgeuse is null, skip */
		if( i_fus->vp == NULL || j_fus->vp == NULL ||
		    i_fus->eu == NULL || j_fus->eu == NULL )
		{
			edge_no++;
			continue;
		}

		/* If either vertex is new_v, skip */
		if( i_fus->vp == new_v || j_fus->vp == new_v )
		{
			edge_no++;
			continue;
		}

		/* the two vertices should never be the same */
		if( i_fus->vp == j_fus->vp )
			rt_bomb( "nmg_kill_collinear_edges: Two vertices are the same\n" );

		NMG_CK_VERTEX( i_fus->vp );
		NMG_CK_VERTEX( j_fus->vp );
		NMG_CK_EDGEUSE( i_fus->eu );
		NMG_CK_EDGEUSE( j_fus->eu );

		/* if the two vertices are within tolerance,
		 * fuse them
		 */
		if( rt_pt3_pt3_equal( i_fus->vp->vg_p->coord , j_fus->vp->vg_p->coord , tol ) )
		{
			nmg_jv( i_fus->vp , j_fus->vp );
			nmg_fuse_inters( i_fus , j_fus , int_faces , tol );
			continue;
		}
		else if( rt_3pts_collinear( i_fus->vp->vg_p->coord , j_fus->vp->vg_p->coord , new_v->vg_p->coord , tol ) )
		{
			fastf_t i_dist,j_dist;
			vect_t i_dist_to_new_v,j_dist_to_new_v;

			/* all three points are collinear,
			 * may need to split edges
			 */

			VSUB2( i_dist_to_new_v , new_v->vg_p->coord , i_fus->vp->vg_p->coord );
			VSUB2( j_dist_to_new_v , new_v->vg_p->coord , j_fus->vp->vg_p->coord );

			if( VDOT( i_dist_to_new_v , j_dist_to_new_v ) < 0.0 )
			{
				/* points are collinear with new_v, but in opoosit directions */
				edge_no++;
				continue;
			}

			i_dist = MAGSQ( i_dist_to_new_v );
			j_dist = MAGSQ( j_dist_to_new_v );

			if( i_dist < tol->dist_sq || j_dist < tol->dist_sq )
				rt_bomb( "nmg_collinear_edges_kill: vertex within tolerance of new_v\n" );

			if( rt_g.NMG_debug & DEBUG_BASIC )
				rt_log( "\tCollinear vertices x%x, x%x, and x%x\n",
						new_v , i_fus->vp , j_fus->vp );

			if( i_dist > j_dist && j_dist > tol->dist_sq )
			{
				/* j point is closer to new_v than i point
				 * split edge at j point
				 */

				if( rt_g.NMG_debug & DEBUG_BASIC )
					rt_log( "\tSplitting i_fus->eu x%x at vertex x%x\n" , i_fus->eu , j_fus->vp );

				(void)nmg_esplit( j_fus->vp , i_fus->eu );
				i_fus->vp = j_fus->vp;
				nmg_fuse_inters( i_fus , j_fus , int_faces , tol );

				continue;
			}
			else if( j_dist > i_dist && i_dist > tol->dist_sq )
			{
				/* i point is closer to new_v than j point
				 * split edge at i point
				 */

				if( rt_g.NMG_debug & DEBUG_BASIC )
					rt_log( "\tSplitting j_fus->eu x%x at vertex x%x\n" , j_fus->eu , i_fus->vp );

				(void)nmg_esplit( i_fus->vp , j_fus->eu );
				nmg_fuse_inters( i_fus , j_fus , int_faces , tol );
				continue;
			}
		}
		edge_no++;
	}
	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "\nAfter fusing collinear edges:\n" );
		nmg_pr_inter( new_v , int_faces );
	}
}

/*	N M G _ M A K E _ F A C E S _ A T _ V E R T
 *
 * Make new faces around vertex new_v using info in
 * the table of intersect_fu structures. Each structure
 * contains a vertex on an edge departing new_v.
 * Vertices from two consecutive edges are combined with
 * new_v to form triangular faces around new_v
 *
 */
void
nmg_make_faces_at_vert( new_v , int_faces , tol )
struct vertex *new_v;
struct nmg_ptbl *int_faces;
CONST struct rt_tol *tol;
{
	struct model *m;
	struct loopuse *old_lu;
	int edge_no=0,next_edge_no,j;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_make_faces_at_vert( x%x , %d intersect_fus structs)\n" , new_v , NMG_TBL_END( int_faces ) );

	NMG_CK_VERTEX( new_v );
	RT_CK_TOL( tol );

	m = nmg_find_model( &new_v->magic );

	if( NMG_TBL_END( int_faces ) == 1 )
	{
		struct intersect_fus *i_fus;

		/* only one intersect point is left, move new_v to it
		 * and don't make any faces
		 */
		if( i_fus->vp )
		{
			i_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , 0 );

			VMOVE( new_v->vg_p->coord , i_fus->vp->vg_p->coord );
			nmg_jv( new_v , i_fus->vp );
		}
		return;
	}

	if( NMG_TBL_END( int_faces ) == 2 )
	{
		struct intersect_fus *i_fus,*j_fus;
		point_t center_pt;

		/* only two intersect points left, move new_v to the center of
		 * the connecting line. No new faces needed
		 */

		i_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , 0 );
		j_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , 1 );

		if( i_fus->vp && j_fus->vp )
		{
			VCOMB2( center_pt , 0.5 , i_fus->vp->vg_p->coord , 0.5 , j_fus->vp->vg_p->coord );
			VMOVE( new_v->vg_p->coord , center_pt );
		}
		return;
	}

	/* Need to make new faces.
	 * loop around the vertex, looking at
	 * pairs of adjacent edges and deciding
	 * if a new face needs to be constructed
	 * from the two intersect vertices and new_v
	 */
	while( edge_no < NMG_TBL_END( int_faces ) )
	{
		struct intersect_fus *i_fus;
		struct intersect_fus *j_fus;
		struct vertexuse *vu1,*vu2;
		struct edgeuse *eu;
		struct loopuse *lu;
		struct loopuse *new_lu;
		struct loopuse *dup_lu;
		struct faceuse *new_fu;
		struct faceuse *fu;
		long **trans_tbl;

		/* get two consectutive structures */
		i_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , edge_no );
		next_edge_no = edge_no+1;
		if( next_edge_no == NMG_TBL_END( int_faces ) )
			 next_edge_no = 0;
		j_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , next_edge_no );

		/* Don't construct a new face across open space */
		if( (i_fus->free_edge || j_fus->free_edge) && next_edge_no == 0 )
		{
			edge_no++;
			continue;
		}

		/* if the two vertices are the same, no face needed */
		if( i_fus->vp == j_fus->vp )
		{
			edge_no++;
			continue;
		}

		/* if either vertex is null, no face needed */
		if( i_fus->vp == NULL || j_fus->vp == NULL || i_fus->eu == NULL || j_fus->eu == NULL )
		{
			edge_no++;
			continue;
		}

		/* Don't make faces with two vertices the same */
		if( i_fus->vp == new_v || j_fus->vp == new_v )
		{
			edge_no++;
			continue;
		}

		NMG_CK_VERTEX( i_fus->vp );
		NMG_CK_VERTEX( j_fus->vp );
		NMG_CK_EDGEUSE( i_fus->eu );
		NMG_CK_EDGEUSE( j_fus->eu );

		if( rt_3pts_collinear( i_fus->vp->vg_p->coord , j_fus->vp->vg_p->coord , new_v->vg_p->coord , tol ) )
		{
			edge_no++;
			continue;
		}

		/* O.K., here is where we actually start making faces.
		 * Find uses of the two vertices in the same loopuse */
		old_lu = j_fus->eu->up.lu_p;
		vu1 = (struct vertexuse *)NULL;
		vu2 = (struct vertexuse *)NULL;
		for( RT_LIST_FOR( eu , edgeuse , &old_lu->down_hd ) )
		{
			if( eu->vu_p->v_p == i_fus->vp )
				vu1 = eu->vu_p;
			else if( eu->vu_p->v_p == j_fus->vp )
				vu2 = eu->vu_p;
		}

		if( vu1 == NULL || vu2 == NULL )
		{
			rt_log( "nmg_make_faces_at_vert: ERROR: Can't find loop containing vertices x%x and x%x\n" , i_fus->vp, j_fus->vp );
			rt_log( "\t( %f %f %f ) and ( %f %f %f )\n" , V3ARGS( i_fus->vp->vg_p->coord ) , V3ARGS( j_fus->vp->vg_p->coord ) );
			edge_no++;
			continue;
		}

		/* make sure the two vertices have a third between,
		 * otherwise, don't cut the loop
		 */
		eu = vu1->up.eu_p;
		if( eu->eumate_p->vu_p == vu2 )
		{
			edge_no++;
			continue;
		}
		eu = vu2->up.eu_p;
		if( eu->eumate_p->vu_p == vu1 )
		{
			edge_no++;
			continue;
		}

		/* cut the face loop across the two vertices */
		new_lu = nmg_cut_loop( vu1 , vu2 );

		/* Fix orientations.
		 * This will never be called for a vertex that borders
		 * a free edge or a hole, so all loops are OT_SAME
		 */
		new_lu->orientation = OT_SAME;
		new_lu->lumate_p->orientation = OT_SAME;
		old_lu->orientation = OT_SAME;
		old_lu->lumate_p->orientation = OT_SAME;

		/* find which loopuse contains new_v
		 * this will be the one to become a new face
		 */
		lu = NULL;

		/* first check old_lu */
		for( RT_LIST_FOR( eu , edgeuse , &old_lu->down_hd ) )
		{
			if( eu->vu_p->v_p == new_v )
			{
				lu = old_lu;
				break;
			}
		}

		/* if not found check new_lu */
		if( lu == NULL )
		{
			for( RT_LIST_FOR( eu , edgeuse , &new_lu->down_hd ) )
			{
				if( eu->vu_p->v_p == new_v )
				{
					lu = old_lu;
					break;
				}
			}
		}

		if( lu == NULL )
		{
			fu = old_lu->up.fu_p;
			rt_log( "nmg_make_faces_at_vert: can't find loop for new face\n" );
			rt_log( "vu1 = x%x (x%x) vu2 = x%x (x%x)\n" , vu1 , vu1->v_p , vu2 , vu2->v_p );
			rt_log( "new_v = x%x\n" , new_v );
			rt_log( "old_lu = x%x , new_lu = x%x\n" , old_lu , new_lu );
			nmg_pr_fu_briefly( fu , (char *)NULL );
			rt_bomb( "nmg_make_faces_at_vert: can't find loop for new face\n" );
		}

		/* make the new face from the new loop */
		new_fu = nmg_mk_new_face_from_loop( lu );

		j_fus->fu[0] = new_fu;
		i_fus->fu[1] = new_fu;

		NMG_CK_FACEUSE( new_fu );

		/* calculate a plane equation for the new face */
		if( nmg_fu_planeeqn( new_fu , tol ) )
		{
			rt_log( "nmg_make_faces_at_vert: Failed to calculate plane eqn for face:\n " );
			rt_log( "\tnew_v is x%x at ( %f %f %f )\n" , new_v , V3ARGS( new_v->vg_p->coord ) );
			if( rt_3pts_collinear( new_v , vu1->v_p , vu2->v_p , tol ) )
				rt_log( "\tPoints are collinear\n" );
			nmg_pr_fu_briefly( new_fu , " " );
		}
		nmg_face_bb( new_fu->f_p , tol );

		edge_no++;
	}
	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "After make faces:\n" );
		nmg_pr_inter( new_v , int_faces );
	}
}

/*	N M G _ K I L L _ C R A C K S _ A T _ V E R T E X
 *
 * Look at all faces around vertex new_v and kill any two
 * consecutive eu's that go from a vertex to a second then back
 * to the original vertex
 */
void
nmg_kill_cracks_at_vertex( vp )
CONST struct vertex *vp;
{
	struct nmg_ptbl fus_at_vert;
	struct vertexuse *vu;
	struct faceuse *fu;
	int fu_no;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_kill_cracks_at_vertex( vp=x%x )\n" , vp );

	NMG_CK_VERTEX( vp );

	/* first make a list of all the faceuses at this vertex */
	nmg_tbl( &fus_at_vert , TBL_INIT , NULL );

	for( RT_LIST_FOR( vu , vertexuse , &vp->vu_hd ) )
	{
		NMG_CK_VERTEXUSE( vu );

		fu = nmg_find_fu_of_vu( vu );
		if( !fu )
			continue;

		NMG_CK_FACEUSE( fu );
		nmg_tbl( &fus_at_vert , TBL_INS_UNIQUE , (long *)fu );
	}

	/* Now look at these faceuses for cracks ( jaunts from a vertex and back to the same ) */
	for( fu_no=0 ; fu_no<NMG_TBL_END( &fus_at_vert ) ; fu_no++ )
	{
		struct loopuse *lu;
		int bad_face=0;

		fu = (struct faceuse *)NMG_TBL_GET( &fus_at_vert , fu_no );
		NMG_CK_FACEUSE( fu );

		lu = RT_LIST_FIRST( loopuse , &fu->lu_hd );
		while( RT_LIST_NOT_HEAD( lu , &fu->lu_hd ) )
		{
			struct loopuse *lu_next;
			struct edgeuse *eu;
			int bad_loop=0;

			NMG_CK_LOOPUSE( lu );

			lu_next = RT_LIST_NEXT( loopuse , &lu->l );

			if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			{
				lu = lu_next;
				continue;
			}

			eu = RT_LIST_FIRST( edgeuse , &lu->down_hd );
			while( RT_LIST_NOT_HEAD( eu , &lu->down_hd ) )
			{
				struct edgeuse *eu_prev;
				struct edgeuse *eu_next;

				NMG_CK_EDGEUSE( eu );

				eu_next = RT_LIST_NEXT( edgeuse , &eu->l );
				eu_prev = RT_LIST_PPREV_CIRC( edgeuse , &eu->l );
				NMG_CK_EDGEUSE( eu_prev );

				/* Check for a crack */
				if( EDGESADJ( eu , eu_prev ) )
				{
					/* found a crack, kill it */
					if( nmg_keu( eu ) )
					{
						/* This should never happen */
						rt_log( "ERROR: nmg_kill_cracks_at_vert: bad loopuse x%x\n" , lu );
						bad_loop = 1;
						break;
					}
					if( nmg_keu( eu_prev ) )
					{
						bad_loop = 1;
						break;
					}
				}
				eu = eu_next;
			}
			if( bad_loop )
			{
				if( nmg_klu( lu ) )
				{
					bad_face = 1;
					break;
				}
			}
			lu = lu_next;
		}
		if( bad_face )
		{
			if( nmg_kfu( fu ) )
				rt_log( "ERROR:nmg_kill_cracks_at_vert: bad shell!!!\n" );
		}
	}
	nmg_tbl( &fus_at_vert , TBL_FREE , NULL );
}

/*	N M G _ F I X _ C R O S S E D _ L O O P S
 *
 * Detect situations where edges have been split, but new vertices are
 * in wrong order. This typically happens as shown:
 *
 *                  new face planes
 *                  |
 *                  |
 *   \       \   /  |    /
 *    \       \ /<--|   /
 *     \       X       /
 *      \     / \     /
 *       \   /___\   /
 *        \         /
 *         \       /<- original face planes
 *          \     /
 *           \___/
 *
 * This can be detected by checking if the edges leaving from the new
 * vertices cross. If so, the middle face is deleted and the
 * two vertices are fused.
 *
 * Also fuses new vertices that are within tolerance of each other
 */
static void
nmg_fix_crossed_loops( new_v , int_faces , tol )
struct vertex *new_v;
struct nmg_ptbl *int_faces;
CONST struct rt_tol *tol;
{
	int edge_no=0;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_fix_crossed_loops( new_v = x%x , %d intersect_fus structs )\n" , new_v , NMG_TBL_END( int_faces ) );

	RT_CK_TOL( tol );
	NMG_CK_VERTEX( new_v );

	/* look for crossed edges */
	while( edge_no < NMG_TBL_END( int_faces ) )
	{
		int next_edge_no;
		struct intersect_fus *i_fus,*j_fus;
		struct edgeuse *i_next_eu,*j_next_eu;
		vect_t i_dir,j_dir;
		struct vertex *i_start,*i_end,*j_start,*j_end;
		point_t i_end_pt,j_end_pt;
		vect_t off_plane;
		plane_t pl;
		fastf_t dist[2];
		int ret_val;
		plane_t pl1,pl2,pl3;
		point_t new_pt;
		struct edgeuse *prev_eu;
		struct edgeuse *radial_eu;

		/* will be comparing this edge with next */
		next_edge_no = edge_no + 1;
		if( next_edge_no == NMG_TBL_END( int_faces ) )
			next_edge_no = 0;

		i_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , edge_no );
		j_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , next_edge_no );

		if( i_fus->free_edge || j_fus->free_edge )
		{
			edge_no++;
			continue;
		}
		if( !i_fus->vp || !j_fus->vp )
		{
			edge_no++;
			continue;
		}

		NMG_GET_FU_PLANE( pl , i_fus->fu[1] );

		/* get edgeuses leaving from new vertices */
		i_next_eu = RT_LIST_PNEXT_CIRC( edgeuse , &i_fus->eu->l );
		j_next_eu = RT_LIST_PNEXT_CIRC( edgeuse , &j_fus->eu->l );

		NMG_CK_EDGEUSE( i_next_eu );
		NMG_CK_EDGEUSE( j_next_eu );

		/* get endpoints for these edges */
		i_end = i_next_eu->eumate_p->vu_p->v_p;
		j_end = j_next_eu->eumate_p->vu_p->v_p;

		NMG_CK_VERTEX( i_end );
		NMG_CK_VERTEX( j_end );

		/* since the other end of these edges may not have been adjusted yet
		 * project the endpoints onto the face plane
		 */
		VJOIN1( i_end_pt , i_end->vg_p->coord , -(DIST_PT_PLANE( i_end->vg_p->coord , pl )) , pl );
		VJOIN1( j_end_pt , j_end->vg_p->coord , -(DIST_PT_PLANE( j_end->vg_p->coord , pl )) , pl );

		/* get start points, guaranteed to be on plane */
		i_start =  i_next_eu->vu_p->v_p;
		j_start =  j_next_eu->vu_p->v_p;

		NMG_CK_VERTEX( i_start );
		NMG_CK_VERTEX( j_start );

		/* calculate direction vectors for use by rt_isect_lseg3_lseg3 */
		VSUB2( i_dir , i_end_pt , i_start->vg_p->coord );
		VSUB2( j_dir , j_end_pt , j_start->vg_p->coord );

		ret_val = rt_isect_lseg3_lseg3( dist , i_start->vg_p->coord , i_dir ,
			j_start->vg_p->coord , j_dir , tol );

		if( ret_val < 0 )
		{
			/* edges do not intersect, no problem here */
			edge_no++;
			continue;
		}
		else if( ret_val == 0 )
		{
			/* edges are collinear */
			rt_bomb( "nmg_fix_crossed_loops: ERROR: edges are collinear\n" );
		}

		NMG_GET_FU_PLANE( pl1 , i_fus->fu[0] );
		NMG_GET_FU_PLANE( pl2 , i_fus->fu[1] );
		NMG_GET_FU_PLANE( pl3 , j_fus->fu[1] );

		if( rt_mkpoint_3planes( new_pt , pl1 , pl2 , pl3 ) )
		{
			rt_log( "nmg_fix_crossed_edges: can't find new point\n" );
			edge_no++;
			continue;
		}

		/* move all occurences of j_fus->vp to i_fus->vp
		 * and move i_fus->vp to the new point
		 */
		VMOVE( i_fus->vp->vg_p->coord , new_pt );
		VMOVE( i_fus->pt , new_pt );

		radial_eu = j_fus->eu->radial_p;
		prev_eu = RT_LIST_PPREV_CIRC( edgeuse , &j_fus->eu->l );

		nmg_jv( i_fus->vp , j_fus->vp );

		if( EDGESADJ( prev_eu , j_fus->eu ) )
		{
			nmg_keu( prev_eu );
			nmg_keu( j_fus->eu );
		}
		else
			rt_log( "nmg_fix_crossed_loops: ERROR: can't find adjacent edges to kill\n" );

		/* Eliminate the middle face */
		i_fus->fu[1] = j_fus->fu[1];

		if( i_fus->fu[0] && j_fus->fu[1] )
			nmg_radial_join_eu( i_fus->eu , radial_eu , tol );

		if( !i_fus->fu[0] )
			i_fus->eu = radial_eu;
		NMG_CK_EDGEUSE( i_fus->eu );

		if( j_fus->free_edge )
			i_fus->free_edge = 1;

		nmg_tbl( int_faces , TBL_RM , (long *)j_fus );

		rt_free( (char *)j_fus , "nmg_fix_crossed_loops: j_fus" );
	}
	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "After fix crossed loops:\n" );
		nmg_pr_inter( new_v , int_faces );
	}
}

/*	N M G _ C O M P L E X _ V E R T E X _ S O L V E
 *
 *	This is intended to handle the cases the "nmg_simple_vertex_solve"
 *	can't do (more than three faces intersecting at a vertex)
 *
 *	This routine may create new edges and/or faces and
 *
 *	Modifies the location of "new_v"
 *
 *	returns:
 *		0 - if everything is OK
 *		1 - failure
 */

int
nmg_complex_vertex_solve( new_v , faces , tol )
struct vertex *new_v;
CONST struct nmg_ptbl *faces;
CONST struct rt_tol *tol;
{
	struct model *m;
	struct faceuse *fu1,*fu2,*fu;
	struct face *fp1;
	struct vertexuse *vu;
	struct edgeuse *eu1,*eu;
	struct nmg_ptbl int_faces;
	struct intersect_fus *k_fus;
	point_t ave_pt;
	vect_t ave_norm,x_vec,y_vec;
	point_t rpp_min;
	int i,j,done;
	int unique_verts;
	int added_faces;
	int edge_no;
	static int pl_count=0;

	/* More than 3 faces intersect at vertex (new_v)
	 * Calculate intersection point along each edge
	 * emanating from new_v */

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_complex_vertex_solve( new_v = x%x , %d faces )\n" , new_v , NMG_TBL_END( faces ) );

	NMG_CK_VERTEX( new_v );
	RT_CK_TOL( tol );

	m = nmg_find_model( &new_v->magic );

	VSET( rpp_min , 0.0 , 0.0 , 0.0 );

	nmg_tbl( &int_faces , TBL_INIT , NULL );

	/* get int_faces table (of intersect_fus structures) partially filled in
	 * with fu's, eu, and edge line definition
	 */
	if( nmg_get_edge_lines( new_v , &int_faces , tol ) )
		return( 1 );

	/* fill in "pt" portion of intersect_fus structures with points
	 * that are the intersections of the edge line with the other
	 * faces that meet at new_v. The intersection that is furthest
	 * up the edge away from new_v is selected
	 */
	nmg_get_max_edge_inters( new_v , &int_faces , faces , tol );

	nmg_fix_folded_inters( new_v , &int_faces , tol );

	/* split edges at intersection points */
	nmg_split_edges_at_pts( new_v , &int_faces , tol );

#if 0
	/* fuse intersection points within tolerance */
	nmg_fuse_inter_verts( new_v , &int_faces , tol );

	/* vertex fusion will create cracks */
	nmg_kill_cracks_at_vertex( new_v );

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "After kill cracks:\n" );
		nmg_pr_inter( new_v , &int_faces );
	}
#else
	nmg_simplify_inter( new_v , &int_faces , tol );
#endif

	/* fix intersection points that cause loops that cross themselves */
	nmg_fix_crossed_loops( new_v , &int_faces , tol );
#if 0
	/* Fuse collinear edges */
	nmg_fuse_collinear_edges( new_v , &int_faces , tol );
#endif
	/* Build needed faces */
	nmg_make_faces_at_vert( new_v , &int_faces , tol );

	/* Where faces were not built, cracks have formed */
	nmg_kill_cracks_at_vertex( new_v );

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "After second kill cracks:\n" );
		nmg_pr_inter( new_v , &int_faces );
	}

	/* free some memory */
	for( i=0 ; i<NMG_TBL_END( &int_faces ) ; i++ )
	{
		struct intersect_fus *i_fus;

		i_fus = (struct intersect_fus *)NMG_TBL_GET( &int_faces , i );
		rt_free( (char *)i_fus , "nmg_complex_vertex_solve: intersect_fus struct\n" );
	}
	nmg_tbl( &int_faces , TBL_FREE , NULL );
	return( 0 );
}

/*	N M G _ B A D _ F A C E _ N O R M A L S
 *
 *	Look for faceuses in the shell with normals that do
 *	not agree with the geometry (i.e., in the wrong direction)
 *
 *	return:
 *		1 - at least one faceuse with a bad normal was found
 *		0 - no faceuses with bad normals were found
 */
int
nmg_bad_face_normals( s , tol )
CONST struct shell *s;
CONST struct rt_tol *tol;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu,*eu1,*eu2;
	vect_t old_normal,new_normal;
	vect_t a,b;
	point_t pa,pb,pc;

	NMG_CK_SHELL( s );
	RT_CK_TOL( tol );

	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		int done=0;

		/* only check OT_SAME faseuses */
		if( fu->orientation != OT_SAME )
			continue;

		/* get current normal */
		NMG_GET_FU_NORMAL( old_normal , fu );

		/* find a good loop */
		lu = RT_LIST_FIRST( loopuse , &fu->lu_hd );
		while( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC && RT_LIST_NOT_HEAD( lu , &fu->lu_hd ) )
			lu = RT_LIST_PNEXT( loopuse , &lu->l );

		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		/* find two edgeuse in the loop for calculating a new normal */
		eu = RT_LIST_FIRST( edgeuse , &lu->down_hd );
		eu1 = RT_LIST_PNEXT_CIRC( edgeuse , &eu->l );
		while( !done )
		{
			VMOVE( pa , eu->vu_p->v_p->vg_p->coord );
			VMOVE( pb , eu1->vu_p->v_p->vg_p->coord );
			eu2 = RT_LIST_PNEXT_CIRC( edgeuse , &eu1->l );
			VMOVE( pc , eu2->vu_p->v_p->vg_p->coord );
			VSUB2( a , pa , pb );
			VSUB2( b , pc , pb );

			/* calculate new normal */
			VCROSS( new_normal , b , a );
			if( MAGSQ( new_normal ) > tol->dist_sq )
			{
				/* reverse normal if needed */
				if( lu->orientation == OT_OPPOSITE )
				{
					VREVERSE( new_normal , new_normal );
				}
				else if( lu->orientation != OT_SAME )
					rt_log( "nmg_bad_face_normals: found loopuse with %s orientation\n" , nmg_orientation( lu->orientation ) );

				/* check if old and new agree */
				if( VDOT( old_normal , new_normal ) < 0.0 )
					return( 1 );
				else
					break;
			}
			else
			{
				/* tried the whole loop, give up */
				if( eu1 == RT_LIST_FIRST( edgeuse , &lu->down_hd ) )
				{
					rt_log( "nmg_bad_face_normals: Couldn't calculate new normal for faceuse\n" );
					done = 1;
				}
				else
				{
					/* step along loop */
					eu = eu1;
					eu1 = eu2;
				}
			}
		}
	}
	return( 0 );
}

/*
 *	N M G _ F A C E S _ A R E _ R A D I A L
 *
 *	checks if two faceuses are radial to each other
 *
 *	returns
 *		1 - the two faceuses are radial to each other
 *		0 - otherwise
 *
 */
int
nmg_faces_are_radial( fu1 , fu2 )
CONST struct faceuse *fu1,*fu2;
{
	struct edgeuse *eu,*eu_tmp;
	struct loopuse *lu;

	NMG_CK_FACEUSE( fu1 );
	NMG_CK_FACEUSE( fu2 );

	/* look at every loop in the faceuse #1 */
	for( RT_LIST_FOR( lu , loopuse , &fu1->lu_hd ) )
	{
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		/* look at every edgeuse in the loop */
		for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
		{
			/* now search radially around edge */
			eu_tmp = eu->eumate_p->radial_p;
			while( eu_tmp != eu && eu_tmp != eu->eumate_p )
			{
				struct faceuse *fu_tmp;

				/* find radial faceuse */
				fu_tmp = nmg_find_fu_of_eu( eu_tmp );

				/* if its the same as fu2 or its mate, the faceuses are radial */
				if( fu_tmp == fu2 || fu_tmp == fu2->fumate_p )
					return( 1 );

				/* go to next radial edgeuse */
				eu_tmp = eu_tmp->eumate_p->radial_p;
			}
		}
	}

	return( 0 );
}

/*
 *	N M G _ E X T R U D E _ C L E A N U P
 *
 *	Clean up after nmg_extrude_shell.
 *	intersects each face with every other face in the shell and
 *	makes new face boundaries at the intersections.
 *	decomposes the result into seperate shells.
 *	where faces have intersected, new shells will be created.
 *	These shells are detected and killed
 */
struct shell *
nmg_extrude_cleanup( is , os , thick , tol )
struct shell *is,*os;
CONST fastf_t thick;
CONST struct rt_tol *tol;
{
	struct nmg_ptbl vertex_uses;
	struct nmg_ptbl faces;
	struct nmg_ptbl bad_loops;
	struct model *m;
	struct nmgregion *new_r,*r;
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct vertexuse *vu;
	struct shell *s_fu;	/* Shell to temporarily hold fu2 */
	int i,j;

	NMG_CK_SHELL( is );
	NMG_CK_SHELL( os );
	RT_CK_TOL( tol );

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_extrude_cleanup( is=x%x , os=x%x , thick=%g)\n" , is , os , thick );

	m = nmg_find_model( &is->l.magic );
	NMG_CK_MODEL( m );

	nmg_vmodel( m );

	r = is->r_p;
	NMG_CK_REGION( r );

	s_fu = nmg_msv( r );
	NMG_CK_SHELL( s_fu );

	nmg_tbl( &vertex_uses , TBL_INIT , NULL );
	nmg_tbl( &bad_loops , TBL_INIT , NULL );

	/* intersect each face with every other face in the shell */
	for( RT_LIST_FOR( fu , faceuse , &is->fu_hd ) )
	{
		struct faceuse *fu2;

		/* only look at OT_SAME faceuses */
		if( fu->orientation != OT_SAME )
			continue;

		/* consider intersection this faceuse with all the faceuses
		 * after it in the list
		 */
		fu2 = RT_LIST_PNEXT( faceuse , &fu->l );
		while( RT_LIST_NOT_HEAD( fu2 , &is->fu_hd ) )
		{
			struct face *f,*f2;
			struct faceuse *fu_tmp,*other_fu;
			plane_t pl,pl2;
			point_t start;
			vect_t dir;
			fastf_t dist;

			/* skip faceuses radial to fu or not OT_SAME */
			if( fu2->orientation != OT_SAME || nmg_faces_are_radial( fu , fu2 ) )
			{
				fu2 = RT_LIST_PNEXT( faceuse , &fu2->l );
				continue;
			}

			f = fu->f_p;
			f2 = fu2->f_p;

			/* skip faceuse pairs that don't have overlapping BB's */
			if( !V3RPP_OVERLAP( f->min_pt , f->max_pt , f2->min_pt , f2->max_pt ) )
			{
				fu2 = RT_LIST_PNEXT( faceuse , &fu2->l );
				continue;
			}

			/* XXX This section should be replaced by a call to nmg_isect_2_generic_faces
			 *  as soon as it is ready */
#if 1
			/* move fu2 to another shell to avoid radial edge problems */
			nmg_mv_fu_between_shells( s_fu, is, fu2 );
rt_log( "nmg_isect_two_generic_faces( fu1 = x%x , fu2 = x%x , tol = x%x )\n" , fu , fu2 , tol );
nmg_pr_fu_briefly( fu , "fu1: " );
nmg_pr_fu_briefly( fu2 , "fu2: " );
			nmg_isect_two_generic_faces( fu , fu2 , tol );
rt_log( "returned from nmg_isect_two_generic_faces\n" );
			/* move fu2 back where it belongs */
			while( RT_LIST_NON_EMPTY( &s_fu->fu_hd ) )
			{
				fu_tmp = RT_LIST_FIRST( faceuse , &s_fu->fu_hd );
				NMG_CK_FACEUSE( fu_tmp );
				nmg_mv_fu_between_shells( is, s_fu, fu_tmp );
			}
		}
	}
	nmg_ks( s_fu );
#else

			/* OK, do the actual intersection */
			NMG_GET_FU_PLANE( pl , fu )
			NMG_GET_FU_PLANE( pl2 , fu2 )

			/* if the two planes don't intersect, continue */
			if( rt_isect_2planes( start , dir , pl , pl2 , is->r_p->ra_p->min_pt , tol ) )
			{
				fu2 = RT_LIST_PNEXT( faceuse , &fu2->l );
				continue;
			}
rt_log( "\nIntersect line at ( %f %f %f ) in direction ( %f %f %f )\n" , V3ARGS( start ) , V3ARGS( dir ) );
			/* intersect the line found above with the two faceuses */
			for( i=0 ; i<2 ; i++ )
			{
				/* hokey way to repeat this for the two faceuses */
				if( i == 0 )
				{
					fu_tmp = fu;
					other_fu = fu2;
				}
				else
				{
					fu_tmp = fu2;
					other_fu = fu;
				}
rt_log( "Looking at fu x%x\n" , fu_tmp );

				/* intersect with each loop */
				for( RT_LIST_FOR( lu , loopuse , &fu_tmp->lu_hd ) )
				{
					int isect_count=0;
					point_t intersection;
rt_log( "\tLooking at lu x%x\n" , lu );

					if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
						continue;

					/* and each edgeuse */
					for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
					{
						struct vertex_g *vgs,*vge;
						struct edgeuse *new_eu;
						struct edgeuse *next_eu;
						int ret_val;

rt_log( "\t\tLooking at edgeuse x%x\n" , eu );
						vgs = eu->vu_p->v_p->vg_p;
						vge = eu->eumate_p->vu_p->v_p->vg_p;

						/* intersect line with edge (line segment)
						 * skip intersections at edge start vertex
						 */
						if( (ret_val=rt_isect_line_lseg( &dist , start , dir , vgs->coord , vge->coord , tol )) > 1 )
						{
							struct vertex *v_isect=(struct vertex *)NULL;

							if( isect_count > 1 )
								rt_bomb( "Too many intersections with loops\n" );
							switch( ret_val )
							{
								case 2:
									/* intersect at end of edge */
									/* add vertexuse to list */
									next_eu = RT_LIST_PNEXT_CIRC( edgeuse , eu );
rt_log( "intersection of fus x%x and x%x (lu x%x) at ( %f %f %f ) vu = x%x (end of edge )\n",
fu , fu2 , lu , V3ARGS( next_eu->vu_p->v_p->vg_p->coord ) , next_eu->vu_p );
									if( nmg_class_pt_f( next_eu->vu_p->v_p->vg_p->coord , other_fu , tol ) == NMG_CLASS_AoutB )
										break;
									nmg_tbl( &vertex_uses , TBL_INS , (long *) next_eu->vu_p );
									isect_count++;
									break;
								case 3:
									/* intersect in middle of edge */
									VJOIN1( intersection , start , dist , dir );
									if( nmg_class_pt_f( intersection , other_fu , tol ) == NMG_CLASS_AoutB )
										break;

									/* re-use existing vertices whenever possible */
									for( j=0 ; j<NMG_TBL_END( &vertex_uses ) ; j++ )
									{
										vu = (struct vertexuse *)NMG_TBL_GET( &vertex_uses , j );
										if( rt_pt3_pt3_equal( intersection , vu->v_p->vg_p->coord , tol ) )
										{
											v_isect = vu->v_p;
											break;
										}
									}
									/* split edge at intersection */
									new_eu = nmg_esplit( v_isect , eu );
									nmg_vertex_gv( new_eu->vu_p->v_p , intersection );

rt_log( "intersection of fus x%x and x%x (lu x%x) at ( %f %f %f ) vu = x%x (middle of edge )\n",
fu , fu2 , lu , V3ARGS( new_eu->vu_p->v_p->vg_p->coord ) , new_eu->vu_p );
									/* add vertexuse to list */
									nmg_tbl( &vertex_uses , TBL_INS , (long *)new_eu->vu_p );
									isect_count++;
									break;
							}
						}
					}

					/* can't handle more than two intersections */
					if( isect_count == 1 )
					{
rt_log( "Only one intersection for this loop, removing it\n" );
						nmg_tbl( &vertex_uses , TBL_RM , (long *)NMG_TBL_GET( &vertex_uses , NMG_TBL_END( &vertex_uses ) - 1 ) );
						isect_count = 0;
					}
					else if( isect_count > 2 )
					{
						rt_log( "%d intersections of line with loop\n" , isect_count );
						rt_bomb( "!!!\n" );
					}
if( isect_count )
{
	int jra;

	rt_log( "After loop intersect:\n" );
	for( jra=0 ; jra < NMG_TBL_END( &vertex_uses ) ; jra++ )
	{
		struct vertexuse *vu_tmp;
		struct edgeuse *eu_tmp;
		struct loopuse *lu_tmp;

		vu_tmp = (struct vertexuse *)NMG_TBL_GET( &vertex_uses , jra );
		eu_tmp = vu_tmp->up.eu_p;
		lu_tmp = eu_tmp->up.lu_p;
		rt_log( "\tvu = x%x, eu = x%x, lu = x%x\n" , vu_tmp , eu_tmp , lu_tmp );
	}
}	
				}
			}
			fu2 = RT_LIST_PNEXT( faceuse , &fu2->l );
		}
	}

	/* go through list of vertexuses and cut some loops */
	if( NMG_TBL_END( &vertex_uses )%4 )
	{
		rt_log( "nmg_extrude_cleanup: NMG_TBL_END( &vertex_uses ) = %d\n" , NMG_TBL_END( &vertex_uses ) );
		rt_bomb( "nmg_extrude_cleanup: Bad number of vertex uses in intersection list\n" );
	}
	for( i=0 ; i<NMG_TBL_END( &vertex_uses ) ; i += 2 )
	{
		struct vertexuse *vu1,*vu2;
		struct loopuse *old_lu[2];
		struct loopuse *new_lu[2];
		struct loopuse *chk_lu;
		struct faceuse *new_fu;
		struct faceuse *fu_cut[2];
		struct edgeuse *eu_base=NULL;
		int orientation;
		int j;

		if( i%4 )
			j = 1;
		else
			j = 0;


		/* take vertexuses off list in pairs (that's how they were put on) */
		vu1 = (struct vertexuse *)NMG_TBL_GET( &vertex_uses , i );
		vu2 = (struct vertexuse *)NMG_TBL_GET( &vertex_uses , i+1 );

		fu_cut[j] = nmg_find_fu_of_vu( vu1 );

		/* remember orientation */
		old_lu[j] = nmg_find_lu_of_vu( vu1 );
		orientation = old_lu[j]->orientation;

		/* cut loop */
		chk_lu = nmg_find_lu_of_vu( vu2 );
		if( old_lu[j] == chk_lu )
		{
			new_lu[j] = nmg_cut_loop( vu1 , vu2 );
rt_log( "cutting loop at vus x%x and x%x made new_lu x%x\n" , vu1 , vu2 , new_lu[j] );

			/* restore orientations */
#if 0
			new_lu[j]->orientation = orientation;
			old_lu[j]->orientation = orientation;
			new_lu[j]->lumate_p->orientation = orientation;
			old_lu[j]->lumate_p->orientation = orientation;
#else
			nmg_lu_reorient( new_lu[j] , tol );
			nmg_lu_reorient( old_lu[j] , tol );
#endif

#if 0
			/* make a new face from the new loop */
			new_fu = nmg_mk_new_face_from_loop( new_lu[j] );
rt_log( "made new_fu x%x\n" , new_fu );

			if( nmg_fu_planeeqn( new_fu , tol ) )
				rt_log( "Couldn't calculate plane equation for new face\n" );

			/* make sure new_fu has orientation compatible with its radial faces */
			lu = RT_LIST_FIRST( loopuse , &new_fu->lu_hd );
			for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
			{
				struct edgeuse *eu_radial;

				eu_radial = eu->radial_p;
				fu = nmg_find_fu_of_eu( eu_radial );
				if( fu == new_fu )
					continue;

				if( fu->orientation != new_fu->orientation )
				{
					nmg_reverse_face( new_fu );
rt_log( "reversing orientation of fu x%x\n" , new_fu );
				}

				break;
			}

#endif
			/* Make sure all edguese between the vertices at vu1 and vu2 share same edge */
rt_log( "Looking for radial edges between verterx x%x and x%x\n" , vu1->v_p , vu2->v_p );
			for( RT_LIST_FOR( vu , vertexuse , &vu1->v_p->vu_hd ) )
			{
				if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
					continue;
				eu = vu->up.eu_p;
				NMG_CK_EDGEUSE( eu );
rt_log( "\tChecking edgeuse x%x on edge x%x from vertex x%x to x%x ( %f %f %f ) to ( %f %f %f ) \n" , 
eu , eu->e_p , eu->vu_p->v_p , eu->eumate_p->vu_p->v_p , V3ARGS( eu->vu_p->v_p->vg_p->coord ) , V3ARGS(  eu->eumate_p->vu_p->v_p->vg_p->coord ) );
				if( eu->eumate_p->vu_p->v_p == vu2->v_p )
				{
					if( !eu_base )
						eu_base = eu;
					else if( eu->e_p != eu_base->e_p )
					{
rt_log( "Joining eus x%x and x%x\n" , eu_base , eu );
/*						nmg_radial_join_eu_no_ck( eu_base , eu , tol ); */
						nmg_radial_join_eu( eu_base , eu , tol );
rt_log( "\tDone!\n" );
					}
				}
			}
		}

		if( i%4 )
		{
			fastf_t dist_to_plane;
			plane_t pl;
			int bad_loop=0;

			/* One loop from each face must be anti-normalward to the other
			 * face. That loop is unwanted, so put it on the "bad" list.
			 */

			/* Check the loops from first face against the second face */
			NMG_GET_FU_PLANE( pl , fu_cut[1] );
			for( RT_LIST_FOR( eu , edgeuse , &old_lu[0]->down_hd ) )
			{
				dist_to_plane = DIST_PT_PLANE( eu->vu_p->v_p->vg_p->coord , pl );
				if( dist_to_plane < (-tol->dist ) )
				{
					bad_loop = 1;
					break;
				}
			}

			if( bad_loop )
				nmg_tbl( &bad_loops , TBL_INS , (long *)old_lu[0] );
			else
			{
				for( RT_LIST_FOR( eu , edgeuse , &new_lu[0]->down_hd ) )
				{
					dist_to_plane = DIST_PT_PLANE( eu->vu_p->v_p->vg_p->coord , pl );
					if( dist_to_plane < (-tol->dist ) )
					{
						bad_loop = 1;
						break;
					}
				}
				if( bad_loop )
					nmg_tbl( &bad_loops , TBL_INS , (long *)new_lu[0] );
			}

			/* Check the loops from second face against the first face */
			bad_loop = 0;
			NMG_GET_FU_PLANE( pl , fu_cut[0] );
			for( RT_LIST_FOR( eu , edgeuse , &old_lu[1]->down_hd ) )
			{
				dist_to_plane = DIST_PT_PLANE( eu->vu_p->v_p->vg_p->coord , pl );
				if( dist_to_plane < (-tol->dist ) )
				{
					bad_loop = 1;
					break;
				}
			}

			if( bad_loop )
				nmg_tbl( &bad_loops , TBL_INS , (long *)old_lu[1] );
			else
			{
				for( RT_LIST_FOR( eu , edgeuse , &new_lu[1]->down_hd ) )
				{
					dist_to_plane = DIST_PT_PLANE( eu->vu_p->v_p->vg_p->coord , pl );
					if( dist_to_plane < (-tol->dist ) )
					{
						bad_loop = 1;
						break;
					}
				}
				if( bad_loop )
					nmg_tbl( &bad_loops , TBL_INS , (long *)new_lu[1] );
			}
		}
else
rt_log( "not cutting loop at  vus x%x and x%x (%f %f %f) (%f %f %f)\n" , vu1 , vu2 , V3ARGS( vu1->v_p->vg_p->coord ) , V3ARGS( vu2->v_p->vg_p->coord ) );
	}

	if( NMG_TBL_END( &vertex_uses ) )
#endif
	{
		/* look for self-touching loops */
		for( RT_LIST_FOR( fu , faceuse , &is->fu_hd ) )
		{
			struct loopuse *lu1;
			plane_t plane;

			if( fu->orientation != OT_SAME )
				continue;

			NMG_GET_FU_PLANE( plane , fu );

			lu = RT_LIST_LAST( loopuse , &fu->lu_hd );
			while( RT_LIST_NOT_HEAD( lu , &fu->lu_hd ) )
			{
				struct loopuse *new_lu;
				struct faceuse *new_fu;
				int orientation;

				/* check this loop */
				while( (vu=(struct vertexuse *)nmg_loop_touches_self( lu ) ) != (struct vertexuse *)NULL )
				{
					/* Split this touching loop, but give both resulting loops
					 * the same orientation as the original. This will result
					 * in the part of the loop that needs to be discarded having
					 * an incorrect orientation with respect to the face.
					 * This incorrect orientation will be discovered later by
					 * "nmg_bad_face_normals" and will result in the undesirable
					 * portion's demise
					 */
					orientation = lu->orientation;
					new_lu = nmg_split_lu_at_vu( lu , vu );
					new_lu->orientation = orientation;
					lu->orientation = orientation;
					new_lu->lumate_p->orientation = orientation;
					lu->lumate_p->orientation = orientation;
				}

				lu = RT_LIST_PLAST( loopuse , &lu->l );
			}
		}
	}

	/* if the vertex_uses list is not empty, we have work to do */
#if 0
	if( NMG_TBL_END( &vertex_uses ) )
#endif
	{
		struct nmgregion *old_r;
		struct shell *s_tmp;
		int inside_shells;

		nmg_rebound( m , tol );

		/* remember the nmgregion where "is" came from */
		old_r = is->r_p;

		/* make a new nmgregion , shell, and vertex */
		new_r = nmg_mrsv( m );

		/* s_tmp is the shell just created */
		s_tmp = RT_LIST_FIRST( shell , &new_r->s_hd );

		/* move our shell (is) to the new nmgregion
		 * in preparaion for nmg_decompose_shell.
		 * don't want to confuse pieces of this shell
		 * with other shells in "old_r"
		 */
		if( nmg_mv_shell_to_region( is , new_r ) )
			rt_bomb( "nmg_extrude_shell: Inside shell was only shell!!!\n" );

		/* kill the unused, newly created shell */
		if( nmg_ks( s_tmp ) )
			rt_bomb( "nmg_extrude_shell: Nothing got moved to new region\n" );

		/* now decompose our shell */
		if( (inside_shells=nmg_decompose_shell( is , tol )) < 2 )
		{
			/* if we still have only one shell,
			 * just move it back
			 */
rt_log( "\n%d inside shells\n" , inside_shells );
			if( nmg_mv_shell_to_region( is , old_r ) )
			{
				nmg_kr( new_r );
				new_r = NULL;
			}
		}
		else
		{
			/* look at each shell in "new_r" */
rt_log( "\n%d inside shells\n" , inside_shells );
			s_tmp = RT_LIST_FIRST( shell , &new_r->s_hd );
			while( RT_LIST_NOT_HEAD( &s_tmp->l , &new_r->s_hd ) )
			{
				struct shell *next_s;
				int kill_it=0;

				next_s = RT_LIST_PNEXT( shell , &s_tmp->l );
rt_log( "SHELL!!!\n" );
nmg_pr_s_briefly( s_tmp , (char *)NULL );

#if 0
				/* check for a shell with bad faceuse normals */
				if( nmg_bad_face_normals( s_tmp , tol ) )
				{
					/* Bad shell, kill it */
					if( nmg_ks( s_tmp ) )
					{
						nmg_kr( new_r );
						new_r = NULL;
						is = NULL;
						break;
					}
				}

				/* check if inside shell is too close to outer shell */
rt_log( "Comparing shells x%x and x%x at thickness %f tolerance dist is %f\n" , s_tmp , os , thick , tol->dist );
				if( nmg_shells_within_dist( os , s_tmp , thick , tol ) )
				{
#endif
#if 0

				for( RT_LIST_FOR( fu , faceuse , &s_tmp->fu_hd ) )
				{
					for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
					{
						if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
							continue;
				
						for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
						{
							if( nmg_check_radial( eu , tol ) )
							{
								rt_log( "BAD EU RADIALS for shell x%x\n" , s_tmp );
								kill_it = 1;
								break;
							}
						}
						if( kill_it )
							break;
					}
					if( kill_it )
						break;
				}

				if( kill_it )
				{
					/* Bad shell, kill it */
					if( nmg_ks( s_tmp ) )
					{
						nmg_kr( new_r );
						new_r = NULL;
						is = NULL;
						break;
					}
				}
#endif

				for( RT_LIST_FOR( fu , faceuse , &s_tmp->fu_hd ) )
				{
					for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
					{
						struct loopuse *bad_lu;

						for( i=0 ; i<NMG_TBL_END( &bad_loops ) ; i++ )
						{
							bad_lu = (struct loopuse *)NMG_TBL_GET( &bad_loops , i );
							if( lu == bad_lu )
							{
								kill_it = 1;
								break;
							}
						}
						if( kill_it )
							break;
					}
					if( kill_it )
						break;
				}
				if( kill_it )
				{
					/* Bad shell, kill it */
					if( nmg_ks( s_tmp ) )
					{
						nmg_kr( new_r );
						new_r = NULL;
						is = NULL;
						break;
					}
				}
				s_tmp = next_s;
			}
		}

		nmg_tbl( &bad_loops , TBL_FREE , NULL );

		if( new_r )
		{
			/* merge remaining shells in "new_r" */
			is = RT_LIST_FIRST( shell , &new_r->s_hd );
			s_tmp = RT_LIST_PNEXT( shell , &is->l );
			while( RT_LIST_NOT_HEAD( &s_tmp->l , &new_r->s_hd ) )
			{
				struct shell *next_s;

				next_s = RT_LIST_PNEXT( shell , &s_tmp->l );

				if( s_tmp == is )
				{
					s_tmp = next_s;
					continue;
				}

				nmg_js( is , s_tmp , tol );
				s_tmp = next_s;
			}

			/* move it all back into the original nmgregion */
			(void)nmg_mv_shell_to_region( is , old_r );

			/* kill the temporary nmgregion */
			if( RT_LIST_NON_EMPTY( &new_r->s_hd ) )
				rt_log( "nmg_extrude_cleanup: temporary nmgregion not empty!!\n" );
			(void)nmg_kr( new_r );
		}
	}
	return( is );
}

/*	N M G _ M O V E _ E D G E _ T H R U _ P T
 *
 *	moves indicated edgeuse (mv_eu) so that it passes thru
 *	the given point (pt). The direction of the edgeuse
 *	is not changed, so new edgeuse is parallel to the original.
 *
 *	plane equations of all radial faces on this edge are changed
 *	and all vertices (except one anchor point) in radial loops are adjusted
 *	Note that the anchor point is chosen arbitrarily.
 *
 *	returns:
 *		1 - failure
 *		0 - success
 */

int
nmg_move_edge_thru_pt( mv_eu , pt , tol )
struct edgeuse *mv_eu;
CONST point_t pt;
CONST struct rt_tol *tol;
{
	struct nmgregion *r;
	struct faceuse *fu,*fu1;
	struct edgeuse *eu,*eu1;
	struct edge_g *eg;
	struct model *m;
	vect_t e_dir;
	long *flags;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_move_edge_thru_pt( mv_eu=x%x , pt=( %f %f %f) )\n" , mv_eu , V3ARGS( pt ) );

	NMG_CK_EDGEUSE( mv_eu );
	RT_CK_TOL( tol );

	m = nmg_find_model( &mv_eu->l.magic );
	NMG_CK_MODEL( m );

	/* get edge direction */
	eg = mv_eu->e_p->eg_p;
	if( eg )
	{
		VMOVE( e_dir , eg->e_dir );
		if( mv_eu->orientation == OT_OPPOSITE )
		{
			VREVERSE( e_dir , e_dir );
		}
	}
	else
	{
		VSUB2( e_dir , mv_eu->eumate_p->vu_p->v_p->vg_p->coord , mv_eu->vu_p->v_p->vg_p->coord );
		VUNITIZE( e_dir );
	}

	eu = mv_eu;
	fu1 = nmg_find_fu_of_eu( eu );

	if( fu1 == NULL )
	{
		vect_t to_pt;
		vect_t move_v;
		fastf_t edir_comp;
		point_t new_loc;

		/* This must be a wire edge, just adjust the endpoints */
		/* keep edge the same length, and move vertices perpendicular to e_dir */

		VSUB2( to_pt , pt , eu->vu_p->v_p->vg_p->coord );
		edir_comp = VDOT( to_pt , e_dir );
		VJOIN1( move_v , to_pt , -edir_comp , e_dir );

		/* move the vertices */
		VADD2( new_loc , eu->vu_p->v_p->vg_p->coord , move_v );
		nmg_vertex_gv( eu->vu_p->v_p , new_loc );
		VADD2( new_loc , eu->eumate_p->vu_p->v_p->vg_p->coord , move_v );
		nmg_vertex_gv( eu->eumate_p->vu_p->v_p , new_loc );
		return( 0 );
	}

	/* Move edge geometry to new point */
	if( eg )
	{
		VMOVE( eg->e_pt , pt );
	}

	/* modify plane equation for each face radial to mv_eu */
	fu = fu1;
	do
	{
		struct edgeuse *eu_next;
		plane_t plane;
		int done;

		NMG_CK_EDGEUSE( eu );
		NMG_CK_FACEUSE( fu );

		/* find an anchor point for face to rotate about
		 * go forward in loop until we find a vertex that is
		 * far enough from the line of mv_eu to produce a
		 * non-zero cross product
		 */
		eu_next = eu;
		done = 0;
		while( !done )
		{
			vect_t next_dir;
			struct vertex *anchor_v;
			fastf_t mag;

			/* get next edgeuse in loop */
			eu_next = RT_LIST_PNEXT_CIRC( edgeuse , &eu_next->l );

			/* check if we have circled the entire loop */
			if( eu_next == eu )
			{
				rt_log( "nmg_move_edge_thru_pt: cannot calculate new plane eqn\n" );
				return( 1 );
			}

			/* anchor point is endpoint of this edgeuse */
			anchor_v = eu_next->eumate_p->vu_p->v_p;

			/* calculate new plane */
			VSUB2( next_dir , anchor_v->vg_p->coord , pt );
			VCROSS( plane , e_dir , next_dir );
			mag = MAGNITUDE( plane );
			if( mag > SQRT_SMALL_FASTF )
			{
				/* this is an acceptable plane */
				mag = 1.0/mag;
				VSCALE( plane , plane , mag );
				plane[3] = VDOT( plane , pt );
				if( fu->orientation == OT_SAME )
					nmg_face_g( fu , plane );
				else
					nmg_face_g( fu->fumate_p , plane );
				done = 1;
			}
		}

		/* move on to next radial face */
		eu = eu->eumate_p->radial_p;
		fu = nmg_find_fu_of_eu( eu );
	}
	while( fu != fu1 && fu != fu1->fumate_p );

	/* now recalculate vertex coordinates for all affected vertices,
	 * could be lots of them
	 */

	flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "nmg_move_edge_thru_pt: flags" );

	eu1 = mv_eu;
	fu1 = nmg_find_fu_of_eu( eu1 );
	fu = fu1;
	do
	{
		struct loopuse *lu;
		struct edgeuse *eu;
		struct vertex *v;

		for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			struct nmg_ptbl faces;

			NMG_CK_LOOPUSE( lu );

			if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )
			{
				struct vertexuse *vu;
				vu = RT_LIST_FIRST( vertexuse , &lu->down_hd );
				if( NMG_INDEX_TEST_AND_SET( flags , vu->v_p ) )
				{
					int free_edges=0;

					nmg_tbl( &faces , TBL_INIT , NULL );

					/* find all unique faces that intersect at this vertex (vu->v_p) */
					if( nmg_find_isect_faces( vu->v_p , &faces , &free_edges , tol ) < 4 )
					{
						if( nmg_simple_vertex_solve( vu->v_p , &faces , tol ) )
						{
							/* failed */
							rt_log( "nmg_move_edge_thru_pt: Could not solve simple vertex\n" );
							nmg_tbl( &faces , TBL_FREE , NULL );
							return( 1 );
						}
					}
					else
						nmg_complex_vertex_solve( vu->v_p , &faces , tol );
					
				}
				continue;
			}

			eu = RT_LIST_FIRST( edgeuse , &lu->down_hd );
			while( RT_LIST_NOT_HEAD( eu , &lu->down_hd ) )
			{
				struct vertexuse *vu;
				struct edgeuse *eu_next;

				eu_next = RT_LIST_PNEXT( edgeuse , &eu->l );

				vu = eu->vu_p;
				if( NMG_INDEX_TEST_AND_SET( flags , vu->v_p ) )
				{
					int free_edges=0;

					nmg_tbl( &faces , TBL_INIT , NULL );

					/* find all unique faces that intersect at this vertex (vu->v_p) */
					if( nmg_find_isect_faces( vu->v_p , &faces , &free_edges , tol ) < 4 )
					{
						if( nmg_simple_vertex_solve( vu->v_p , &faces , tol ) )
						{
							/* failed */
							rt_log( "nmg_move_edge_thru_pt: Could not solve simple vertex\n" );
							nmg_tbl( &faces , TBL_FREE , NULL );
							return( 1 );
						}
					}
					else
						nmg_complex_vertex_solve( vu->v_p , &faces , tol );
				}
				eu = eu_next;
			}
			nmg_tbl( &faces , TBL_FREE , NULL );
		}

		/* move on to next radial face */
		eu1 = eu1->eumate_p->radial_p;
		fu = nmg_find_fu_of_eu( eu1 );
	}
	while( fu != fu1 && fu != fu1->fumate_p );

	rt_free( (char *)flags , "mg_move_edge_thru_pt: flags" );

	return( 0 );
}

/*	N M G _ V L I S T _ T O _ W I R E _ E D G E S
 *
 *	Convert a vlist to NMG wire edges
 *
 */
void
nmg_vlist_to_wire_edges( s , vhead )
struct shell *s;
struct rt_list *vhead;
{
	struct rt_vlist *vp;
	struct edgeuse *eu;
	struct vertex *v1,*v2;
	point_t pt1,pt2;

	NMG_CK_SHELL( s );
	NMG_CK_LIST( vhead );

	v1 = (struct vertex *)NULL;
	v2 = (struct vertex *)NULL;

	vp = RT_LIST_FIRST( rt_vlist , vhead );
	if( vp->nused < 2 )
		return;

	for( RT_LIST_FOR( vp , rt_vlist , vhead ) )
	{
		register int i;
		register int nused = vp->nused;

		for( i=0 ; i<vp->nused ; i++ )
		{
			switch( vp->cmd[i] )
			{
				case RT_VLIST_LINE_MOVE:
				case RT_VLIST_POLY_MOVE:
					v1 = (struct vertex *)NULL;
					v2 = (struct vertex *)NULL;
					VMOVE( pt2 , vp->pt[i] );
					break;
				case RT_VLIST_LINE_DRAW:
				case RT_VLIST_POLY_DRAW:
					VMOVE( pt1 , pt2 );
					v1 = v2;
					VMOVE( pt2 , vp->pt[i] );
					v2 = (struct vertex *)NULL;
					eu = nmg_me( v1 , v2 , s );
					v1 = eu->vu_p->v_p;
					v2 = eu->eumate_p->vu_p->v_p;
					nmg_vertex_gv( v2 , pt2 );
					if( !v1->vg_p )
						nmg_vertex_gv( v1 , pt1 );
					break;
				case RT_VLIST_POLY_START:
				case RT_VLIST_POLY_END:
					break;
			}
		}
	}
}

void
nmg_follow_free_edges_to_vertex( vpa, vpb, vpbad1, vpbad2, eu, verts, found )
CONST struct vertex *vpa,*vpb,*vpbad1,*vpbad2;
CONST struct edgeuse *eu;
struct nmg_ptbl *verts;
int *found;
{
	struct vertexuse *vu;

	NMG_CK_EDGEUSE( eu );
	NMG_CK_VERTEX( vpa );
	NMG_CK_VERTEX( vpb );
	if( vpbad1 )
		NMG_CK_VERTEX( vpbad1 );
	if( vpbad2 )
		NMG_CK_VERTEX( vpbad2 );

	NMG_CK_VERTEX( eu->eumate_p->vu_p->v_p );

	for( RT_LIST_FOR( vu , vertexuse , &eu->eumate_p->vu_p->v_p->vu_hd ) )
	{
		struct edgeuse *eu1;

		NMG_CK_VERTEXUSE( vu );

		if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
			continue;

		eu1 = vu->up.eu_p;

		NMG_CK_EDGEUSE( eu1 );

		/* stick to free edges */
		if( eu1->eumate_p != eu1->radial_p )
			continue;

		/* don`t go back the way we came */
		if( eu1 == eu->eumate_p )
			continue;

		if( eu1->eumate_p->vu_p->v_p == vpb )
		{
			/* found it!!! */
			nmg_tbl( verts , TBL_INS , (long *)vu->v_p );
			nmg_tbl( verts , TBL_INS , (long *)vpb );
			*found = 1;
		}
		else if( eu1->eumate_p->vu_p->v_p == vpa )
		{
			/* back where we started */
			*found = (-1);
		}
		else if( eu1->eumate_p->vu_p->v_p == vpbad1 || eu1->eumate_p->vu_p->v_p == vpbad2 )
		{
			/* this is the wrong way */
			*found = (-2);
		}
		else
		{
			nmg_tbl( verts , TBL_INS , (long *)vu->v_p );
			nmg_follow_free_edges_to_vertex( vpa , vpb , vpbad1, vpbad2 , eu1 , verts , found );
		}
		if( *found )
			return;
	}
}

static struct nmg_ptbl *
nmg_find_middle_verts( vpa, vpb, vpbad1, vpbad2 )
CONST struct vertex *vpa,*vpb,*vpbad1,*vpbad2;
{
	int done;
	static struct nmg_ptbl verts;
	struct vertexuse *vua;


	NMG_CK_VERTEX( vpa );
	NMG_CK_VERTEX( vpb );
	if( vpbad1 )
		NMG_CK_VERTEX( vpbad1 );
	if( vpbad2 )
		NMG_CK_VERTEX( vpbad2 );

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "nmg_find_middle_verts( vpa=x%x ( %f %f %f ), vpb=x%x ( %f %f %f )\n",
			vpa, V3ARGS( vpa->vg_p->coord ), vpb, V3ARGS( vpb->vg_p->coord ) );
		if( vpbad1 )
			rt_log( "\tvpbad1=x%x ( %f %f %f )\n",
				vpbad1, V3ARGS( vpbad1->vg_p->coord ) );
		if( vpbad2 )
			rt_log( "\tvpbad2=x%x ( %f %f %f )\n",
				vpbad2, V3ARGS( vpbad2->vg_p->coord ) );
	}

	nmg_tbl( &verts , TBL_INIT , NULL );
	nmg_tbl( &verts , TBL_INS , (long *)vpa );

	for( RT_LIST_FOR( vua , vertexuse , &vpa->vu_hd ) )
	{
		struct edgeuse *eua;

		NMG_CK_VERTEXUSE( vua );

		if( *vua->up.magic_p != NMG_EDGEUSE_MAGIC )
			continue;

		eua = vua->up.eu_p;

		NMG_CK_EDGEUSE( eua );

		if( eua->eumate_p != eua->radial_p )
			continue;

		if( eua->eumate_p->vu_p->v_p == vpbad1 || eua->eumate_p->vu_p->v_p == vpbad2 )
			continue;

		if( eua->eumate_p->vu_p->v_p == vpb )
		{
			nmg_tbl( &verts , TBL_INS , (long *)vpb );
			return( &verts );
		}

		done = 0;
		nmg_follow_free_edges_to_vertex( vpa, vpb, vpbad1, vpbad2, eua, &verts, &done );

		if( done > 0 )
			break;

		nmg_tbl( &verts , TBL_RST , NULL );
		nmg_tbl( &verts , TBL_INS , (long *)vpa );
	}

	if( done == 1 )
		return( &verts );

    if( done < 1 )
    {
        int free_edge_count;
    	struct vertexuse *vu1,*vu4;

        rt_log( "ERROR: nmg_find_middle_verts: cannot find intervening vertices\n" );
        rt_log( "verts are: x%x ( %f %f %f ) and x%x ( %f %f %f )\n",
		vpa, V3ARGS( vpa->vg_p->coord ), vpb, V3ARGS( vpb->vg_p->coord ) );

        free_edge_count = 0;
        for( RT_LIST_FOR( vu1 , vertexuse , &vpa->vu_hd ) )
        {
            struct edgeuse *eu1;

            if( *vu1->up.magic_p != NMG_EDGEUSE_MAGIC )
                continue;

            eu1 = vu1->up.eu_p;
            if( eu1->radial_p == eu1->eumate_p )
                free_edge_count++;
        }
        rt_log( "%d free edges at vertex x%x\n" , free_edge_count , vpa );

        free_edge_count = 0;
        for( RT_LIST_FOR( vu4 , vertexuse , &vpb->vu_hd ) )
        {
            struct edgeuse *eu4;

            if( *vu4->up.magic_p != NMG_EDGEUSE_MAGIC )
                continue;

            eu4 = vu4->up.eu_p;
            if( eu4->radial_p == eu4->eumate_p )
                free_edge_count++;
        }
        rt_log( "%d free edges at vertex x%x\n" , free_edge_count , vpb );
    }
}

void
nmg_glue_face_in_shell( fu , s , tol )
CONST struct faceuse *fu;
struct shell *s;
CONST struct rt_tol *tol;
{
	struct loopuse *lu;

	NMG_CK_FACEUSE( fu );
	NMG_CK_SHELL( s );
	RT_CK_TOL( tol );

	for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		struct edgeuse *eu;

		NMG_CK_LOOPUSE( lu );
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
		{
			struct edgeuse *eu1;

			NMG_CK_EDGEUSE( eu );

			eu1 = nmg_findeu( eu->vu_p->v_p, eu->eumate_p->vu_p->v_p, s, eu, 1 );
			if( eu1 )
			{
				NMG_CK_EDGEUSE( eu1 );
				nmg_radial_join_eu( eu1, eu, tol );
			}
		}
	}
}

static void
nmg_make_connect_faces( dst , vpa , vpb , verts , tol )
struct shell *dst;
struct vertex *vpa,*vpb;
struct nmg_ptbl *verts;
CONST struct rt_tol *tol;
{
	int i;
	int verts_in_face=0;
	struct vertex *face_verts[20];
	int max_verts=20;
	int made_face;

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "nmg_make_connect_faces( dst=x%x, vpa=x%x ( %f %f %f ), vpb=x%x ( %f %f %f )\n",
				dst, vpa, V3ARGS( vpa->vg_p->coord ), vpb, V3ARGS( vpb->vg_p->coord ) );
		for( i=0 ; i<NMG_TBL_END( verts ) ; i++ )
		{
			struct vertex *v;

			v = (struct vertex *)NMG_TBL_GET( verts , i );
			rt_log( "\tx%x ( %f %f %f )\n" , v , V3ARGS( v->vg_p->coord ) );
		}
	}

	NMG_CK_SHELL( dst );
	NMG_CK_VERTEX( vpa );
	NMG_CK_VERTEX( vpb );
	RT_CK_TOL( tol );

	face_verts[0] = (struct vertex *)NULL;
	for( i=0 ; i<NMG_TBL_END( verts ) ; i++ )
	{
		struct vertex *v;
		vect_t to_vpa,to_vpb;
		fastf_t dist_to_a_sq,dist_to_b_sq;

		v = (struct vertex *)NMG_TBL_GET( verts , i );
		NMG_CK_VERTEX( v );

		VSUB2( to_vpa , vpa->vg_p->coord , v->vg_p->coord );
		VSUB2( to_vpb , vpb->vg_p->coord , v->vg_p->coord );

		dist_to_a_sq = MAGSQ( to_vpa );
		dist_to_b_sq = MAGSQ( to_vpb );

		if( face_verts[0] == (struct vertex *)NULL )
		{
			if( dist_to_a_sq < dist_to_b_sq )
				face_verts[0] = vpa;
			else
				face_verts[0] = vpb;

			face_verts[1] = v;
			verts_in_face = 2;
		}
		else
		{
			face_verts[verts_in_face++] = v;
				
			if( !rt_3pts_collinear( face_verts[0]->vg_p->coord,
				face_verts[1]->vg_p->coord,
				face_verts[verts_in_face - 1]->vg_p->coord, tol) )
			{
				struct faceuse *new_fu;
				struct loopuse *lu;
				struct edgeuse *eu;

				if( rt_g.NMG_debug & DEBUG_BASIC )
				{
					int debug_int;

					rt_log( "make face:\n" );
					for( debug_int=0 ; debug_int<verts_in_face ; debug_int++ )
						rt_log( "\tx%x ( %f %f %f )\n" , face_verts[debug_int],
							V3ARGS( face_verts[debug_int]->vg_p->coord ) );
				}

				new_fu = nmg_cface( dst , face_verts , verts_in_face );
				if( nmg_fu_planeeqn( new_fu , tol ) )
					rt_bomb( "nmg_make_connect_faces: Failed to calculate plane eqn\n" );

				made_face = 1;

				/* glue this face in */
				nmg_glue_face_in_shell( new_fu , dst , tol );

				if( dist_to_b_sq <= dist_to_a_sq && face_verts[0] == vpa )
				{
					/* make middle face */
					face_verts[1] = v;
					face_verts[2] = vpb;

					if( rt_g.NMG_debug & DEBUG_BASIC )
					{
						int debug_int;

						rt_log( "make middle face:\n" );
						for( debug_int=0 ; debug_int<verts_in_face ; debug_int++ )
							rt_log( "\tx%x ( %f %f %f )\n" , face_verts[debug_int],
								V3ARGS( face_verts[debug_int]->vg_p->coord ) );
					}

					new_fu = nmg_cface( dst , face_verts , 3 );
					if( nmg_fu_planeeqn( new_fu , tol ) )
						rt_bomb( "nmg_make_connect_faces: Failed to calculate plane eqn\n" );

					nmg_glue_face_in_shell( new_fu , dst , tol );
				}

				/* get ready for next face, if necessary */
				if( i < NMG_TBL_END( verts ) )
				{
					if( dist_to_a_sq < dist_to_b_sq )
						face_verts[0] = vpa;
					else
						face_verts[0] = vpb;

					face_verts[1] = v;
					verts_in_face = 2;
				}
			}
			else
				made_face = 0;
		}
	}

	if( !made_face )
	{
		struct vertex *v1,*v2;
		struct edgeuse *eu;
		struct edgeuse *new_eu;

		/* didn't make last face, must be collinear points
		 * so split an edge to get the last vertex in
		 */

		v1 = (struct vertex *)NMG_TBL_GET( verts , NMG_TBL_END( verts ) - 2 );
		NMG_CK_VERTEX( v1 );

		v2 = (struct vertex *)NMG_TBL_GET( verts , NMG_TBL_END( verts ) - 1 );
		NMG_CK_VERTEX( v2 );

		eu = nmg_findeu( vpb , v1 , dst , (struct edgeuse *)NULL , 1 );
		if( !eu )
		{
			rt_log( "nmg_make_connect_faces: Cannot find eu from x%x to x%x\n" , vpb , v1 );
			rt_bomb( "\n" );
		}

		new_eu = nmg_esplit( v2 , eu );

		if( rt_g.NMG_debug & DEBUG_BASIC )
			rt_log( "Split eu x%x (x%x -> x%x) at vertex x%x\n" , eu , eu->vu_p->v_p , eu->eumate_p->vu_p->v_p, v2 );
	}
}

/*	N M G _ O P E N _ S H E L L S _ C O N N E C T
 *
 *	Two open shells are connected along their free edges by building
 *	new faces.  The resluting closed shell is in "dst", and "src" shell
 *	is destroyed.  The "copy_tbl" is a translation table that provides
 *	a one-to-one translation between the vertices in the two shells, i.e.,
 *	NMG_INDEX_GETP(vertex, copy_tbl, v), where v is a pointer to a vertex
 *	in "dst" shell, provides a pointer to the corresponding vertex in "src" shell
 *
 *	returns:
 *		0 - All is well
 *		1 - failure
 */
int
nmg_open_shells_connect( dst , src , copy_tbl , tol )
struct shell *dst;
struct shell *src;
CONST long **copy_tbl;
CONST struct rt_tol *tol;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct vertex *vp[3];
	struct nmg_ptbl faces;
	int vertex_count=0;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_open_shells_connect( dst=x%x , src=x%x , copy_tbl=x%x)\n" , dst , src , copy_tbl );

	NMG_CK_SHELL( dst );
	NMG_CK_SHELL( src );

	if( nmg_ck_closed_surf( dst , tol ) )
	{
		rt_log( "nmg_open_shells_connect: destination shell is closed!\n" );
		return( 1 );
	}

	if( nmg_ck_closed_surf( src , tol ) )
	{
		rt_log( "nmg_open_shells_connect: source shell is closed!\n" );
		return( 1 );
	}

	/* find free edges in "dst" shell */
	for( RT_LIST_FOR( fu , faceuse , &dst->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		if( fu->orientation != OT_SAME )
			continue;
		for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			NMG_CK_LOOPUSE( lu );
			if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )
				continue;

			for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
			{
				NMG_CK_EDGEUSE( eu );
				if( eu->eumate_p == eu->radial_p )
				{
					struct vertexuse *test_vu;
					struct vertex *vp1,*vp2;
					struct vertex *vpa,*vpb;
					struct vertex *vpbad1,*vpbad2;
					struct nmg_ptbl *verts;
					int i;

					/* found a free edge, find corresponding edge in "src" shell */
					vpa = eu->vu_p->v_p;
					NMG_CK_VERTEX( vpa );
					vp1 = NMG_INDEX_GETP(vertex, copy_tbl, vpa );
					NMG_CK_VERTEX( vp1 );
					vpb = eu->eumate_p->vu_p->v_p;
					NMG_CK_VERTEX( vpb );
					vp2 = NMG_INDEX_GETP(vertex, copy_tbl, vpb );
					NMG_CK_VERTEX( vp2 );

					/* look for another dangling edge at this vertex
					 * to establish direction for nmg_find_middle_verts
					 * to look in
					 */

					vpbad1 = (struct vertex *)NULL;
					for( RT_LIST_FOR( test_vu , vertexuse , &eu->vu_p->v_p->vu_hd ) )
					{
						struct edgeuse *test_eu;

						NMG_CK_VERTEXUSE( test_vu );

						if( *test_vu->up.magic_p != NMG_EDGEUSE_MAGIC )
							continue;

						test_eu = test_vu->up.eu_p;
						NMG_CK_EDGEUSE( test_eu );

						if( test_eu == eu )
							continue;

						if( test_eu->eumate_p != test_eu->radial_p )
							continue;

						vpbad1 = test_eu->eumate_p->vu_p->v_p;
					}

					if( !vpbad1 )
					{
						for( RT_LIST_FOR( test_vu , vertexuse , &eu->eumate_p->vu_p->v_p->vu_hd ) )
						{
							struct edgeuse *test_eu;

							NMG_CK_VERTEXUSE( test_vu );

							if( *test_vu->up.magic_p != NMG_EDGEUSE_MAGIC )
								continue;

							test_eu = test_vu->up.eu_p;
							NMG_CK_EDGEUSE( test_eu );

							if( test_eu->eumate_p == eu )
								continue;

							if( test_eu->eumate_p != test_eu->radial_p )
								continue;

							vpbad1 = test_eu->eumate_p->vu_p->v_p;
						}
					}

					if( vpbad1 )
						vpbad2 = NMG_INDEX_GETP(vertex, copy_tbl, vpbad1 );
					else
						vpbad2 = (struct vertex *)NULL;

					/* find vertices between vp1 and vp2 */
					verts = nmg_find_middle_verts( vp1 , vp2 , vpbad1 , vpbad2 );

					/* make faces connecting the two shells */
					nmg_make_connect_faces( src , vpa , vpb , verts , tol );

					nmg_tbl( verts , TBL_FREE , NULL );
				}
			}
		}
	}

	nmg_js( dst , src , tol );

	/* now glue it all together */
	nmg_tbl( &faces , TBL_INIT , NULL );
	for( RT_LIST_FOR( fu , faceuse , &dst->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		if( fu->orientation == OT_SAME )
			nmg_tbl( &faces , TBL_INS , (long *)fu );
	}
	nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ) );
	nmg_tbl( &faces , TBL_FREE , NULL );

	return( 0 );
}

/*	N M G _ I N _ V E R T
 *
 *	Move vertex so it is at the intersection of the newly created faces
 *
 *	This routine is used by "nmg_extrude_shell" to move vertices. Each
 *	plane has already been moved a distance inward and
 *	the surface normals have been reversed.
 *
 */
int
nmg_in_vert( new_v , tol )
struct vertex *new_v;
CONST struct rt_tol *tol;
{
	struct model *m;
	struct nmg_ptbl faces;
	int failed=0;
	int free_edges=0;
	int face_count;
	int i;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_in_vert( new_v=x%x )\n" , new_v );

	NMG_CK_VERTEX( new_v );
	RT_CK_TOL( tol );

	m = nmg_find_model( &new_v->magic );

	nmg_tbl( &faces , TBL_INIT , NULL );

	/* find all unique faces that intersect at this vertex (new_v) */
	face_count = nmg_find_isect_faces( new_v , &faces , &free_edges , tol );
	if( (face_count < 4 && !free_edges) || face_count < 3 )
	{
		if( nmg_simple_vertex_solve( new_v , &faces , tol ) )
		{
			failed = 1;
			rt_log( "Could not solve simple vertex\n" );
		}
	}
	else
	{
		if( nmg_complex_vertex_solve( new_v , &faces , tol ) )
		{
			failed = 1;
			rt_log( "Could not solve complex vertex\n" );
		}
	}

	/* Free memory */
	nmg_tbl( &faces , TBL_FREE , NULL );

	return( failed );
}

/*
 *	N M G _ E X T R U D E _ S H E L L
 *
 *	Hollows out a shell producing a wall thickness of thickness "thick"
 *	by creating a new "inner" shell and combining the two shells.
 *
 *	If the original shell is closed, the new shell is simply
 *	merged with the original shell.  If the original shell is open, then faces
 *	are constructed along the free edges of the two shells to make a closed shell.
 *
 */
void
nmg_extrude_shell( s , thick , tol )
struct shell *s;
CONST fastf_t thick;
CONST struct rt_tol *tol;
{
	struct nmgregion *new_r;
	struct vertex *v;
	struct vertexuse *vu;
	struct edgeuse *eu;
	struct loopuse *lu;
	struct faceuse *fu;
	struct face_g *fg_p;
	struct model *m;
	struct shell *is;	/* inside shell */
	struct nmg_ptbl vertex_uses,faces;
	long *flags;
	long **copy_tbl;
	int i,j;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_extrude_shell( s=x%x , thick=%f)\n" , s , thick );

	NMG_CK_SHELL( s );
	RT_CK_TOL( tol );

	if( thick < 0.0 )
	{
		rt_log( "nmg_extrude_shell: thickness less than zero not allowed" );
		return;
	}

	if( thick < tol->dist )
	{
		rt_log( "nmg_extrude_shell: thickness less than tolerance not allowed" );
		return;
	}

	m = nmg_find_model( (long *)s );

	/* first make a copy of this shell */
	is = nmg_dup_shell( s , &copy_tbl );

	/* make a translation table for this model */
	flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "nmg_extrude_shell flags" );

	/* now adjust all the planes, first move them inward by distance "thick" */
	for( RT_LIST_FOR( fu , faceuse , &is->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		NMG_CK_FACE( fu->f_p );
		fg_p = fu->f_p->fg_p;
		NMG_CK_FACE_G( fg_p );

		/* move the faces by the distance "thick" */
		if( NMG_INDEX_TEST_AND_SET( flags , fg_p ) )
			fg_p->N[3] -= thick;
	}

	/* Reverse the normals of all the faces */
	for( RT_LIST_FOR( fu , faceuse , &is->fu_hd ) )
	{
		/* "nmg_reverse_face" does the fu and fu mate */
		if( fu->orientation == OT_SAME )
		{
			if( NMG_INDEX_TEST_AND_SET( flags , fu->f_p ) )
				nmg_reverse_face( fu );
		}
	}

	/* now start adjusting the vertices
	 * Use the original shell so that we can pass the original vertex to nmg_inside_vert
	 */
	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		if( fu->orientation != OT_SAME )
			continue;

		for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			NMG_CK_LOOPUSE( lu );
			if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )
			{
				/* the vertex in a loop of one vertex
				 * must show up in an edgeuse somewhere,
				 * so don't mess with it here */
				continue;
			}
			else
			{
				for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					struct vertex *new_v;

					NMG_CK_EDGEUSE( eu );
					vu = eu->vu_p;
					NMG_CK_VERTEXUSE( vu );
					new_v = NMG_INDEX_GETP( vertex , copy_tbl , vu->v_p );
					NMG_CK_VERTEX( new_v )
					if( NMG_INDEX_TEST_AND_SET( flags , new_v ) )
					{
						/* move this vertex */
						if( nmg_in_vert( new_v , tol ) )
							rt_bomb( "Failed to get a new point from nmg_inside_vert\n" );
					}
				}
			}
		}
	}
	/* recompute the bounding boxes */
	nmg_region_a( is->r_p , tol );

	nmg_vmodel( m );

	is = nmg_extrude_cleanup( is , s , thick , tol );

	/* Inside shell is done */
	if( is )
	{
		if( nmg_ck_closed_surf( s , tol ) )
		{
			if( !nmg_ck_closed_surf( is , tol ) )
			{
				rt_log( "nmg_extrude_shell: inside shell is not closed, calling nmg_close_shell\n" );
				nmg_close_shell( is );
			}

			nmg_shell_coplanar_face_merge( is , tol , 0 );
			nmg_simplify_shell( is );

			/* now merge the inside and outside shells */
			nmg_js( s , is , tol );
		}
		else
		{
			if( nmg_ck_closed_surf( is , tol ) )
			{
				rt_log( "nmg_extrude_shell: inside shell is closed, outer isn't!!\n" );
				nmg_shell_coplanar_face_merge( is , tol , 0 );
				nmg_simplify_shell( is );
				nmg_js( s , is , tol );
			}
			else
			{
				/* connect the boundaries of the two open shells */
				nmg_open_shells_connect( s , is , copy_tbl , tol );
			}
		}
	}

	/* recompute the bounding boxes */
	nmg_region_a( s->r_p , tol );

	/* free memory */
	rt_free( (char *)flags , "nmg_extrude_shell: flags" );
	rt_free( (char *)copy_tbl , "nmg_extrude_shell: copy_tbl" );
}

/*		N M G _ M I R R O R _ M O D E L
 *
 *	mirror model across the y-axis
 *	this does not copy the model, it changes the
 *	model passed to it
 */

void
nmg_mirror_model( m )
struct model *m;
{
	struct nmg_ptbl vertices;
	struct nmgregion *r;
	int i;
	long *flags;

	NMG_CK_MODEL( m );

	/* mirror all vertices across the y axis */
	nmg_vertex_tabulate( &vertices , &m->magic );

	for( i=0 ; i<NMG_TBL_END( &vertices ) ; i++ )
	{
		struct vertex *v;

		v = (struct vertex *)NMG_TBL_GET( &vertices , i );
		NMG_CK_VERTEX( v );

		v->vg_p->coord[Y] = (-v->vg_p->coord[Y]);
	}
	(void)nmg_tbl( &vertices , TBL_FREE , NULL );

	/* adjust the direction of all the faces */
	flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "nmg_mirror_model: flags" );
	for( RT_LIST_FOR( r , nmgregion , &m->r_hd ) )
	{
		struct shell *s;

		for( RT_LIST_FOR( s , shell , &r->s_hd ) )
		{
			struct faceuse *fu;

			for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
			{
				int orientation;

				if( NMG_INDEX_TEST_AND_SET( flags , fu ) )
				{
					/* switch orientations of all faceuses */
					orientation = fu->orientation;
					fu->orientation = fu->fumate_p->orientation;
					fu->fumate_p->orientation = orientation;
					NMG_INDEX_SET( flags , fu->fumate_p );

					if( NMG_INDEX_TEST_AND_SET( flags , fu->f_p->fg_p ) )
					{
						/* correct normal vector */
						fu->f_p->fg_p->N[Y] = (-fu->f_p->fg_p->N[Y]);
					}
				}
			}
		}
	}

	rt_free( (char *)flags , "nmg_mirror_model: flags " );
}
