/*
 *	S H _ G R A S S . C
 *
 *	A procedural shader to produce grass
 *
 */
#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./material.h"
#include "./mathtab.h"
#include "./rdebug.h"

#define SHADE_CONT	0
#define SHADE_ABORT_GRASS	1	/* bit_flag */
#define SHADE_ABORT_STACK	2	/* bit_flag */


#define grass_MAGIC 0x1834    /* make this a unique number for each shader */
#define CK_grass_SP(_p) RT_CKMAG(_p, grass_MAGIC, "grass_specific")


#define GRASSRAY_MAGIC 2048
struct grass_ray {
	long		magic;
	struct xray	r;
	double		d_min;
	double		d_max;
	vect_t		rev;
	double		diverge;
	double		radius;
	struct bn_tol	tol;
	struct hit	hit;
};

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct grass_specific {
	long	magic;	/* magic # for memory validity check, must come 1st */
	double	cell[2];	/* size of a cell in Region coordinates */
	double	ppc;		/* mean # plants_per_cell */
	double	ppcd;		/* deviation of plants_per_cell */
	double	t;		/* mean length of leaf segment */
	double	blade_width;	/* max width of blade segment */
	int	nsegs;		/* #segs per blade */
	double	seg_ratio;
	double	lacunarity;	/* the usual noise parameters */
	double	h_val;
	double	octaves;
	double	size;		/* size of noise coordinate space */
	point_t	vscale;		/* size of noise coordinate space */
	vect_t	delta;

	mat_t	m_to_sh;	/* model to shader space matrix */
	mat_t	sh_to_m;	/* model to shader space matrix */
};

/* The default values for the variables in the shader specific structure */
CONST static
struct grass_specific grass_defaults = {
	grass_MAGIC,
	{300.0, 300.0},			/* cell */
	2.0,				/* plants_per_cell */
	1.0,				/* deviation of plants_per_cell */
	70.0,				/* "t" mean length of leaf seg */
	3.0,				/* max width (mm) of blade segment */
	3,				/* # segs per blade */
	1.0,				/* seg ratio */
	2.1753974,			/* lacunarity */
	1.0,				/* h_val */
	4.0,				/* octaves */
	1.0,				/* size */
	{ 1.0, 1.0, 1.0 },		/* vscale */
	{ 1000.0, 1000.0, 1000.0 },	/* delta into noise space */
	{	0.0, 0.0, 0.0, 0.0,	/* m_to_sh */
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0 }
	};

#define SHDR_NULL	((struct grass_specific *)0)
#define SHDR_O(m)	offsetof(struct grass_specific, m)
#define SHDR_AO(m)	offsetofarray(struct grass_specific, m)

/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct bu_structparse grass_print_tab[] = {
	{"%f",  2, "cell",		SHDR_AO(cell),		FUNC_NULL },
	{"%f",	1, "ppc",		SHDR_O(ppc),		FUNC_NULL },
	{"%f",	1, "ppcd",		SHDR_O(ppcd),		FUNC_NULL },
	{"%f",	1, "t",			SHDR_O(t),		FUNC_NULL },
	{"%f",	1, "width",		SHDR_O(blade_width),	FUNC_NULL },
	{"%f",	1, "lacunarity",	SHDR_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "H", 		SHDR_O(h_val),		FUNC_NULL },
	{"%f",	1, "octaves", 		SHDR_O(octaves),	FUNC_NULL },
	{"%f",  1, "size",		SHDR_O(size),		FUNC_NULL },
	{"%d",	1, "nsegs",		SHDR_O(nsegs),		FUNC_NULL },
	{"%f",	1, "seg_ratio",		SHDR_O(seg_ratio),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }

};
struct bu_structparse grass_parse_tab[] = {
	{"i",	bu_byteoffset(grass_print_tab[0]), "grass_print_tab", 0, FUNC_NULL },
	{"%f",  2, "c",			SHDR_AO(cell),		FUNC_NULL },
	{"%f",	1, "p",			SHDR_O(ppc),		FUNC_NULL },
	{"%f",	1, "pd",		SHDR_O(ppcd),		FUNC_NULL },
	{"%f",	1, "l",			SHDR_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "o", 		SHDR_O(octaves),	FUNC_NULL },
	{"%f",  1, "s",			SHDR_O(size),		FUNC_NULL },
	{"%f",	1, "w",			SHDR_O(blade_width),	FUNC_NULL },
	{"%d",	1, "n",			SHDR_O(nsegs),		FUNC_NULL },
	{"%f",	1, "r",			SHDR_O(seg_ratio),	FUNC_NULL },
	
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int	grass_setup(), grass_render();
HIDDEN void	grass_print(), grass_free();

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 */
CONST struct mfuncs grass_mfuncs[] = {
	{MF_MAGIC,	"grass",	0,	MFI_NORMAL|MFI_HIT|MFI_UV,  MFF_PROC,
	grass_setup,	grass_render,	grass_print,	grass_free },

	{0,		(char *)0,	0,		0,		0,
	0,		0,		0,		0 }
};


/*	G R A S S _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
grass_setup( rp, matparm, dpp, mfp, rtip)
register struct region	*rp;
struct rt_vls		*matparm;
char			**dpp;	/* pointer to reg_udata in *rp */
struct mfuncs		*mfp;
struct rt_i		*rtip;	/* New since 4.4 release */
{
	register struct grass_specific	*grass_sp;

	/* check the arguments */
	RT_CHECK_RTI(rtip);
	RT_VLS_CHECK( matparm );
	RT_CK_REGION(rp);


	if( rdebug&RDEBUG_SHADE)
		rt_log("grass_setup(%s)\n", rp->reg_name);

	/* Get memory for the shader parameters and shader-specific data */
	GETSTRUCT( grass_sp, grass_specific );
	*dpp = (char *)grass_sp;

	/* initialize the default values for the shader */
	memcpy(grass_sp, &grass_defaults, sizeof(struct grass_specific) );

	if (rp->reg_aircode == 0) {
		bu_log("%s\n%s\n",
		"*** WARNING: grass shader applied to non-air region!!! ***",
		"Set air flag with \"edcodes\" in mged");
		rt_bomb("");
	}

	/* parse the user's arguments for this use of the shader. */
	if( bu_struct_parse( matparm, grass_parse_tab, (char *)grass_sp ) < 0 )
		return(-1);

	/* The shader needs to operate in a coordinate system which stays
	 * fixed on the region when the region is moved (as in animation).
	 * We need to get a matrix to perform the appropriate transform(s).
	 */
	db_region_mat(grass_sp->m_to_sh, rtip->rti_dbip, rp->reg_name);

	mat_inv(grass_sp->sh_to_m, grass_sp->m_to_sh);

	if( rdebug&RDEBUG_SHADE) {
		bu_struct_print( " Parameters:", grass_print_tab, (char *)grass_sp );
		mat_print( "m_to_sh", grass_sp->m_to_sh );
		mat_print( "sh_to_m", grass_sp->sh_to_m );
	}

	return(1);
}

/*
 *	G R A S S _ P R I N T
 */
HIDDEN void
grass_print( rp, dp )
register struct region *rp;
char	*dp;
{
	bu_struct_print( rp->reg_name, grass_print_tab, (char *)dp );
}

/*
 *	G R A S S _ F R E E
 */
HIDDEN void
grass_free( cp )
char *cp;
{
	rt_free( cp, "grass_specific" );
}

/* compute the Region coordinates of the origin of a cell */
#define CELL_POS(cell_pos, grass_sp, cell_num) \
	cell_pos[X] = cell_num[X] * grass_sp->cell[X]; \
	cell_pos[Y] = cell_num[Y] * grass_sp->cell[Y]

#define BLADE_SEGS_MAX 4

#define LEAF_MAGIC 1024
#define BLADE_MAGIC 1023
#define PLANT_MAGIC 1022

struct leaf_segment {
	long	magic;
	double len;
	vect_t blade;	/* direction of blade growth */ 
	vect_t	N;	/* surface normal of blade segment */
};

struct blade {
	long			magic;
	double			tot_len;
	int			segs;
	struct leaf_segment	leaf[BLADE_SEGS_MAX];
	point_t			pmin;
	point_t			pmax;
};

#define BLADE_MAX 4
struct plant {
	long		magic;
	point_t			root;	/* location of base of blade */
	int 		blades;
	struct blade	b[BLADE_MAX];
	point_t			pmin;
	point_t			pmax;
};
/*
 *
 *
 */
static void
make_blade(bl, root, c, cell_pos, grass_sp)
struct blade *bl;
point_t		root;	/* location of base of plant */
point_t		c;	/* derived from cell_num */
CONST point_t	cell_pos;
CONST struct grass_specific *grass_sp;
{
	CONST static point_t z_axis = { 0.0, 0.0, 1.0 };
	point_t cl;	/* our local copy of the cell_pos for changing */
	vect_t	left;
	int t;
	double val;
	vect_t v;
	point_t pmin, pmax;
	point_t seg_start;

	CK_grass_SP(grass_sp);

	if (rdebug&RDEBUG_SHADE)
		bu_log("\tmake blade..");

	bl->magic = BLADE_MAGIC;

	VMOVE(cl, c);

	bl->segs = grass_sp->nsegs;

	/* get vector for blade growth */
	bl->leaf[0].magic = LEAF_MAGIC;
	bn_noise_vec(cl, bl->leaf[0].blade);
	bl->leaf[0].blade[Z] += 1.0;
	VUNITIZE(bl->leaf[0].blade);
	bl->tot_len = bl->leaf[0].len = grass_sp->t + 
		bn_noise_turb(cl, grass_sp->h_val,
			grass_sp->lacunarity, grass_sp->octaves) *
		grass_sp->t;


	/* get surface normal for blade */
	VCROSS(left, bl->leaf[0].blade, z_axis);
	VUNITIZE(left);
	VCROSS(bl->leaf[0].N, left, bl->leaf[0].blade);
	VUNITIZE(bl->leaf[0].N);

	if (rdebug&RDEBUG_SHADE) {
		bu_log("root:%g %g %g\n\t  blade:%g %g %g N:%g %g %g  len:%g\n",
			V3ARGS(root), V3ARGS(bl->leaf[0].blade),
			V3ARGS(bl->leaf[0].N),
			bl->leaf[0].len);
	}

	VJOIN1(seg_start, root, bl->leaf[0].len, bl->leaf[0].blade);

	for (t=1; t < bl->segs ; t++) {

		bl->leaf[t].magic = LEAF_MAGIC;

		/* droop the blade */
		VMOVE(bl->leaf[t].blade, bl->leaf[t-1].blade);

		VJOIN1(v, root, t, bl->leaf[t-1].blade);

		val = bn_noise_turb(v, grass_sp->h_val,
				grass_sp->lacunarity, grass_sp->octaves);
		if (val < .3) val = .3;
		if (val > .9) val = .9;

		bl->leaf[t].blade[Z] *= val;

		VUNITIZE(bl->leaf[t].blade);

		val = 1.0 - val;
		if (val < .3) val = .3;
		if (val > .9) val = .9;

		bl->tot_len += bl->leaf[t].len = bl->leaf[t-1].len * val;

		VCROSS(bl->leaf[t].N, left, bl->leaf[t].blade);
		VUNITIZE(bl->leaf[t].N);

		if (rdebug&RDEBUG_SHADE) {
	                bu_log("\t  seg:%g %g %g N:%g %g %g  len:%g\n",
	                	V3ARGS(bl->leaf[t].blade),
				V3ARGS(bl->leaf[t].N),
				bl->leaf[t].len);
		}

		VJOIN1(seg_start, seg_start, bl->leaf[t].len, bl->leaf[t].blade);
		/* compute bounding box */
		VMINMAX(bl->pmin, bl->pmax, seg_start);
	}


	if (rdebug&RDEBUG_SHADE) {
		bu_log("\tbbox: %g %g %g   %g %g %g\n",
			V3ARGS(bl->pmin), V3ARGS(bl->pmax));
	}

	for (t = bl->segs ; t < BLADE_SEGS_MAX ; t++)
		bl->leaf[t].magic = 0;


}
static void
make_plant(pl, c, cell_pos, grass_sp)
struct plant 			*pl;
point_t				c;	/* derived from cell_num */
CONST point_t			cell_pos;
CONST struct grass_specific 	*grass_sp;
{
	int i;
	point_t pt;
	double f, g;

	CK_grass_SP(grass_sp);

	pl->magic = PLANT_MAGIC;
	pl->blades = 1;	

	f = bn_noise_turb(c, grass_sp->h_val,
			grass_sp->lacunarity, grass_sp->octaves);

	g = f * (BLADE_MAX - 1);

	pl->blades = 1 + (g + .5);
	
	if (rdebug&RDEBUG_SHADE)
		bu_log("%g means %d blades on plant\n", f, pl->blades);

	/* get position of the root of the blade */
	pl->root[X] = cell_pos[X] + 
		grass_sp->cell[X] * bn_noise_perlin(c);

	VSCALE(pt, c, grass_sp->lacunarity);
	pl->root[Y] = cell_pos[Y] + 
		grass_sp->cell[Y] * bn_noise_perlin(pt);
	pl->root[Z] = 0.0;	/* XXX not valid for non-slab grass */
	
	VMOVE(pl->pmin, pl->root);
	VMOVE(pl->pmax, pl->root);

	VMOVE(pt, c);

	for (i=0 ; i < pl->blades ; i++ ) {
		make_blade(&pl->b[i], pl->root, pt, cell_pos, grass_sp);
		if (i == 0) {
			VMOVE(pl->pmin, pl->b[i].pmin);
			VMOVE(pl->pmax, pl->b[i].pmax);
		} else {
			VMINMAX(pl->pmin, pl->pmax, pl->b[i].pmin);
			VMINMAX(pl->pmin, pl->pmax, pl->b[i].pmax);
		}

		VSCALE(pt, pt, grass_sp->lacunarity);
	}
	for (i = pl->blades ; i < BLADE_MAX ; i++) {
		pl->b[i].magic = 0;
	}

	if (rdebug&RDEBUG_SHADE) {
		bu_log("\tplant bbox: %g %g %g   %g %g %g\n",
			V3ARGS(pl->pmin), V3ARGS(pl->pmax));
	}


}

/*	Intersect ray with leaf segment.  We already know we're within
 *	max width of the segment.
 *
 *	Return:
 *		0	missed
 *		1	hit something
 */
static int
hit_blade(pl, r, swp, grass_sp, seg, ldist)
CONST struct blade *pl;
struct grass_ray *r;
struct shadework	*swp;	/* defined in material.h */
CONST struct grass_specific *grass_sp;
int seg;
double ldist[2];
{
	point_t PCA;

	CK_grass_SP(grass_sp);
	BU_CKMAG(r, GRASSRAY_MAGIC, "grass_ray");


	/* get the hit point/PCA */
	if (rdebug&RDEBUG_SHADE)
		bu_log("\t  hit_blade()\n");


	if (ldist[0] < r->hit.hit_dist) {

		/* we're the closest hit on the cell */
		r->hit.hit_dist = ldist[0];
		VJOIN1(r->hit.hit_point, r->r.r_pt, ldist[0], r->r.r_dir);
		VMOVE(r->hit.hit_normal, pl->leaf[seg].N);

		if (rdebug&RDEBUG_SHADE) {
			bu_log("  New closest hit %g < %g\n",
				ldist[0], r->hit.hit_dist);
			bu_log("  pt:(%g %g %g)\n  Normal:(%g %g %g)\n",
				V3ARGS(r->hit.hit_point),
				V3ARGS(r->hit.hit_normal));
		}


		return SHADE_ABORT_GRASS;
	} else {
		if (rdebug&RDEBUG_SHADE)
			bu_log("abandon hit in cell: %g > %g\n",
				ldist[0], r->hit.hit_dist);
	}
	return SHADE_CONT;
}

/*	intersect ray with leaves of single blade
 *
 *	Return:
 *		0	missed
 *		1	hit something
 */
static int
isect_blade(bl, root, r, swp, grass_sp)
CONST struct blade *bl;
point_t root;
CONST struct grass_ray *r;
struct shadework	*swp;	/* defined in material.h */
CONST struct grass_specific *grass_sp;
{
	double ldist[2];
	point_t pt;
	int cond;
	int seg;
	point_t PCA_ray;
	double	PCA_ray_radius;
	point_t PCA_grass;
	vect_t	tmp;
	double dist;
	double accum_len;/* accumulated distance along blade from prev segs */
	double fract;	/* fraction of total blade length to PCA */
	double blade_width;/* width of blade at PCA with ray */


	CK_grass_SP(grass_sp);
	BU_CKMAG(r, GRASSRAY_MAGIC, "grass_ray");

	if (rdebug&RDEBUG_SHADE)
		bu_log("\t  isect_blade()\n");

	BU_CKMAG(bl, BLADE_MAGIC, "blade");

	VMOVE(pt, root);

	accum_len = 0.0;

	for (seg=0 ; seg < bl->segs ; accum_len += bl->leaf[seg].len ) {

		BU_CKMAG(&bl->leaf[seg].magic, LEAF_MAGIC, "leaf");

		cond = rt_dist_line3_line3(ldist, r->r.r_pt, r->r.r_dir,
			pt, bl->leaf[seg].blade, &r->tol);

		if (rdebug&RDEBUG_SHADE)
			bu_log("\t    cond:%d d1:%g d2:%g\n", cond, V2ARGS(ldist));

		if (ldist[0] < 0.0 		/* behind ray */ || 
		    ldist[0] >= r->d_max	/* beyond out point */ ||
		    ldist[1] < 0.0 		/* under ground */ ||
		    ldist[1] > bl->leaf[seg].len/* beyond end of seg */
		    )	goto iter;

		VJOIN1(PCA_ray, r->r.r_pt, ldist[0], r->r.r_dir);
		PCA_ray_radius = r->radius + ldist[0] * r->diverge;

		VJOIN1(PCA_grass, pt, ldist[1], bl->leaf[seg].blade);
		VSUB2(tmp, PCA_grass, PCA_ray);
		dist = MAGNITUDE(tmp);

		/* We want to narrow the blade of grass toward the tip.
		 * So we scale the width of the blade based upon the 
		 * fraction of total blade length to PCA.
		 */
		fract = (accum_len + ldist[1]) / bl->tot_len;
		blade_width = grass_sp->blade_width * (1.0 - fract);

		if (dist < (PCA_ray_radius+blade_width)) {
			if (rdebug&RDEBUG_SHADE)
				bu_log("\thit grass: %g < (%g + %g)\n",
					dist, PCA_ray_radius,
					grass_sp->blade_width);

			return hit_blade(bl, r, swp, grass_sp, seg, ldist);
		}
		if (rdebug&RDEBUG_SHADE) bu_log("missed aside\n");

iter:
		/* compute origin of NEXT leaf segment */
		VJOIN1(pt, pt, bl->leaf[seg].len, bl->leaf[seg].blade);
		seg++;
	}

	return SHADE_CONT;
}

static int
isect_plant(pl, r, swp, grass_sp)
CONST struct plant *pl;
struct grass_ray *r;
struct shadework	*swp;	/* defined in material.h */
CONST struct grass_specific *grass_sp;
{
	int i, status;

	CK_grass_SP(grass_sp);
	BU_CKMAG(r, GRASSRAY_MAGIC, "grass_ray");
	BU_CKMAG(pl, PLANT_MAGIC, "plant");

	if( rdebug&RDEBUG_SHADE) {
		bu_log("isect_plant()\n");
		r->r.r_min = r->r.r_max = 0.0;
	}

	if (! rt_in_rpp(&r->r, r->rev, pl->pmin, pl->pmax) ) {
		if( rdebug&RDEBUG_SHADE) {
			point_t in_pt, out_pt;
			bu_log("min:%g max:%g\n", r->r.r_min, r->r.r_max);

			bu_log("ray %g %g %g->%g %g %g misses:\n\trpp %g %g %g, %g %g %g\n",
				V3ARGS(r->r.r_pt), V3ARGS(r->r.r_dir),
				V3ARGS(pl->pmin),  V3ARGS(pl->pmax));
			VJOIN1(in_pt, r->r.r_pt, r->r.r_min, r->r.r_dir);
			VPRINT("\tin_pt", in_pt);
	
			VJOIN1(out_pt, r->r.r_pt, r->r.r_max, r->r.r_dir);
			VPRINT("\tout_pt", out_pt);
		}
		return 0;
	} else { 
		if( rdebug&RDEBUG_SHADE) {
			point_t in_pt, out_pt;
			bu_log("min:%g max:%g\n", r->r.r_min, r->r.r_max);
			bu_log("ray %g %g %g->%g %g %g hit:\n\trpp %g %g %g, %g %g %g\n",
				V3ARGS(r->r.r_pt),
				V3ARGS(r->r.r_dir),
				V3ARGS(pl->pmin),
				V3ARGS(pl->pmax));
			VJOIN1(in_pt, r->r.r_pt, r->r.r_min, r->r.r_dir);
			VPRINT("\tin_pt", in_pt);
	
			VJOIN1(out_pt, r->r.r_pt, r->r.r_max, r->r.r_dir);
			VPRINT("\tout_pt", out_pt);
		}
	}

	for (i=0 ; i < pl->blades ; i++) {
		status = isect_blade(&pl->b[i], pl->root, r, swp, grass_sp);
		if (status) return status;
	}
	return 0;
}


static int
stat_cell(cell_pos, r, grass_sp, swp, dist_to_cell)
point_t cell_pos;	/* origin of cell in region coordinates */
struct grass_ray	*r;
struct grass_specific	*grass_sp;
struct shadework	*swp;
double dist_to_cell;
{
	double val;
	point_t tmp;
	/* the ray is "large" so just pick something appropriate */

	CK_grass_SP(grass_sp);
	BU_CKMAG(r, GRASSRAY_MAGIC, "grass_ray");

	if (rdebug&RDEBUG_SHADE)
		bu_log("statistical bailout\n");

	r->hit.hit_dist = dist_to_cell;
	VJOIN1(r->hit.hit_point, r->r.r_pt, dist_to_cell, r->r.r_dir);
#if 0
	VSET(swp->sw_color, 1.0, 0.0, 0.0);
#else

	bn_noise_vec(cell_pos, r->hit.hit_normal);
	r->hit.hit_normal[Z] += 1.0;
	VUNITIZE(r->hit.hit_normal);

#endif

	return SHADE_ABORT_GRASS;
}


/*	I S E C T _ C E L L
 *
 *  Intersects a region-space ray with a grid cell of grass.
 *
 *
 *
 *  Return:
 *	0	continue grid marching
 *	~0	hit something
 */
static int
isect_cell(cell, r, swp, out_dist, grass_sp)
long			cell[3];
struct grass_ray	*r;
double 			out_dist;
struct shadework	*swp;
struct grass_specific	*grass_sp;
{
	point_t c;		/* float version of cell # */
	point_t cell_pos;	/* origin of cell in region coordinates */
	double val;
	vect_t v;
	int p;		/* current plant number (loop variable) */
	int ppc;	/* # plants in this cell */
	struct plant pl;
	int hit = 0;
	double dist_to_cell;

	CK_grass_SP(grass_sp);

	if (rdebug&RDEBUG_SHADE)
		bu_log("isect_cell(%ld,%ld)\n", V2ARGS(cell));


	/* get coords of cell */
	CELL_POS(cell_pos, grass_sp, cell);

	VSUB2(v, cell_pos, r->r.r_pt);
	dist_to_cell = MAGNITUDE(v);

	
	/* radius of ray at cell origin */
	val = r->radius + r->diverge * dist_to_cell;


	if (rdebug&RDEBUG_SHADE)
		bu_log("\t  ray radius @cell %g = %g, %g, %g   (%g)\n\t   cell:%g,%g\n",
			val, r->radius, r->diverge, dist_to_cell,  val*32.0,
			V2ARGS(grass_sp->cell));

	val *= 32.0;
	if (val >= grass_sp->cell[X] || val >= grass_sp->cell[Y])
		return stat_cell(cell_pos, r, grass_sp, swp, dist_to_cell);


	/* Figure out how many plants are in this cell */
	VMOVE(c, cell); /* int/float conv */
	val = bn_noise_perlin(c);
	ppc = (int) (grass_sp->ppc + grass_sp->ppcd * val * 2.0);

	if (rdebug&RDEBUG_SHADE)
		bu_log("cell pos(%g,%g .. %g,%g)\n", V2ARGS(cell_pos),
			cell_pos[X]+grass_sp->cell[X],
			cell_pos[Y]+grass_sp->cell[Y]);

	if (rdebug&RDEBUG_SHADE)
		bu_log("%d plants  ppc:%g v:%g\n", ppc, grass_sp->ppcd, val);

	/* intersect the ray with each plant */
	for (p=0 ;  p < ppc ; p++) {

		make_plant(&pl, c, cell_pos, grass_sp);

		hit |= isect_plant(&pl, r, swp, grass_sp);

		VSCALE(c, c, grass_sp->lacunarity);
	}

	return hit;
}



/*  Process a grid cell and any unprocessed adjoining neighbors.
 *
 * The flags argument indicates which cells (relative to the central
 * one) need to be processed.  The bit values for the relative
 * positions are:
 *
 *  pos   bit	 pos  bit   pos  bit
 *------------------------------------
 * -1, 1  0100	0, 1 0200  1, 1 0400
 * -1, 0  0010  0, 0 0020  1, 0 0040
 * -1,-1  0001  0,-1 0002  1,-1 0004
 *
 *
 *  Return:
 *	0	continue grid marching
 *	!0	abort grid marching
 */
static int
do_cells(cell_num, r, flags, swp, out_dist, grass_sp)
long			cell_num[3];
struct grass_ray	*r;
short 			flags;		/* which adj cells need processing */
double 			out_dist;
struct shadework	*swp;	/* defined in material.h */
struct grass_specific	*grass_sp;
{
	int x, y;
	long cell[3];
	int hit = 0;

#define FLAG(x,y) ((1 << ((y+1)*3)) << (x+1))
#define ISDONE(x,y,flags) ( ! (FLAG(x,y) & flags))

	CK_grass_SP(grass_sp);
	BU_CKMAG(r, GRASSRAY_MAGIC, "grass_ray");

	if( rdebug&RDEBUG_SHADE)
		bu_log("do_cells(%ld,%ld)\n", V2ARGS(cell_num));

	for (y=-1; y < 2 ; y++) {
		for (x=-1; x < 2 ; x++) {
#if 0
			short f;
			f = FLAG(x,y);
			if( rdebug&RDEBUG_SHADE)
				bu_log("\n0%03o &\n0%03o = %d ",
					flags, f, (flags & f) != 0 );
#endif
			if ( ISDONE(x,y,flags) ) continue;

			cell[X] = cell_num[X] + x;
			cell[Y] = cell_num[Y] + y;

			if( rdebug&RDEBUG_SHADE)
				bu_log("checking relative cell %2d,%2d at(%d,%d)\n",
					x, y, V2ARGS(cell));
			hit |= isect_cell(cell, r, swp, out_dist, grass_sp);
		}
	}
	return hit;
}

/*
 *	G R A S S _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c) once for each hit point
 *	to be shaded.  The purpose here is to fill in values in the shadework
 *	structure.
 */
int
grass_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;	/* defined in material.h */
char			*dp;	/* ptr to the shader-specific struct */
{
	register struct grass_specific *grass_sp =
		(struct grass_specific *)dp;
	struct grass_ray	gr;
	vect_t		v;
	vect_t		in_rad, out_rad;
	point_t 	in_pt, out_pt, curr_pt;
	double		curr_dist, out_dist;
	double		radius;
	short		flags;
	double		t[2], tD[2];
	int		n;
	double		t_orig[2];
	long		tD_iter[2];
	long		cell_num[3];	/* cell number */
	long		old_cell_num[3];	/* cell number */
	int		status;


	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_grass_SP(grass_sp);

	if( rdebug&RDEBUG_SHADE)
		bu_struct_print( "grass_render Parameters:", grass_print_tab, (char *)grass_sp );

#if 0
	if (swp->sw_xmitonly) {
		if( rdebug&RDEBUG_SHADE)
			bu_log("grass no shadows\n");
		swp->sw_transmit = 1.0;
		return 0;
	}
#endif
	/* XXX now how would you transform the tolerance structure
	 * to the local coordinate space ?
	 */
	gr.magic = GRASSRAY_MAGIC;

#if 1
	gr.tol = ap->a_rt_i->rti_tol;
#else
	gr.tol.magic = BN_TOL_MAGIC;
	gr.tol.dist = grass_sp->blade_width;
	gr.tol.dist_sq = grass_sp->blade_width * grass_sp->blade_width;
#endif
	/* Convert everything over to "Region" space.
	 * First the ray and its radius then
	 * the in/out points
	 */
	MAT4X3PNT(gr.r.r_pt, grass_sp->m_to_sh, ap->a_ray.r_pt);
	MAT4X3VEC(gr.r.r_dir, grass_sp->m_to_sh, ap->a_ray.r_dir);
	VUNITIZE(gr.r.r_dir);
	VINVDIR(gr.rev, gr.r.r_dir);

	/* In Hit point */
	MAT4X3PNT(in_pt, grass_sp->m_to_sh, swp->sw_hit.hit_point);

	/* The only thing we can really do is get (possibly 3 different)
	 * radii in the Region coordinate system
	 */
	radius = ap->a_rbeam + swp->sw_hit.hit_dist * ap->a_diverge;
	VSETALL(v, radius);
	VUNITIZE(v);
	MAT4X3VEC(in_rad, grass_sp->m_to_sh, v);

	VSUB2(v, gr.r.r_pt, in_pt);

	gr.radius = ap->a_rbeam;   /* XXX Bogus if Region != Model space */
	gr.diverge = ap->a_diverge;/* XXX Bogus if Region != Model space */

	/* Out point */
	VJOIN1(v, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
	MAT4X3VEC(out_pt, grass_sp->m_to_sh, v);
	radius = ap->a_rbeam + pp->pt_outhit->hit_dist * ap->a_diverge;
	VSETALL(v, radius);
	MAT4X3VEC(out_rad, grass_sp->m_to_sh, v);

	/* set up a DDA grid to march through. */
	VSUB2(v, in_pt, gr.r.r_pt);
	curr_dist = MAGNITUDE(v);

	/* We set up a hit on the out point so that when we get a hit on
	 * a grass blade in a cell we can tell if it's closer than the 
	 * previous hits.  This way we end up using the closest hit for 
	 * the final result.
	 */
	VSUB2(v, out_pt, gr.r.r_pt);
	gr.d_max = gr.hit.hit_dist = out_dist = MAGNITUDE(v);
	VMOVE(gr.hit.hit_point, out_pt);
	MAT4X3VEC(gr.hit.hit_normal, grass_sp->m_to_sh, swp->sw_hit.hit_normal);


	if( rdebug&RDEBUG_SHADE) {
		bu_log("Pt: (%g %g %g)\nRPt:(%g %g %g)\n", V3ARGS(ap->a_ray.r_pt),
			V3ARGS(gr.r.r_pt));
		bu_log("Dir: (%g %g %g)\nRDir:(%g %g %g)\n", V3ARGS(ap->a_ray.r_dir),
			V3ARGS(gr.r.r_dir));
		bu_log("rev: (%g %g %g)\n", V3ARGS(gr.rev));

		bu_log("Hit Pt:(%g %g %g) d:%g r:  %g\nRegHPt:(%g %g %g) d:%g Rr:(%g %g %g)\n",
			V3ARGS(swp->sw_hit.hit_point), swp->sw_hit.hit_dist,
			ap->a_rbeam + swp->sw_hit.hit_dist * ap->a_diverge,
			V3ARGS(in_pt), curr_dist, V3ARGS(in_rad));

		VJOIN1(v, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
		bu_log("Out Pt:(%g %g %g) d:%g r:   %g\nRegOPt:(%g %g %g) d:%g ROr:(%g %g %g)\n",
			V3ARGS(v), pp->pt_outhit->hit_dist, radius,
			V3ARGS(out_pt), out_dist, V3ARGS(out_rad));
	}


	/* tD[X] is the distance along the ray we have to travel
	 * to traverse a cell (travel a unit distance) along the
	 * X axis of the grid.
	 *
	 * t[X] is the distance along the ray to the first cell boundary
	 * beyond the hit point in the X direction.
	 *
	 * t_orig[X] is the same as t[X] but won't get changed as we
	 * march across the grid.  At each X grid line crossing, we recompute
	 * a new t[X] from t_orig[X], tD[X], tD_iter[X].  The tD_iter[X] is 
	 * the number of times we've made a full step in the X direction.
	 */
	for (n=X ; n < Z ; n++) {
		char *s;
		if (n == X) s="X";
		else s="Y";

		/* compute distance from cell origin to in_pt */
		t[n] = in_pt[n] - 
			floor(in_pt[n]/grass_sp->cell[n]) *
				       grass_sp->cell[n];
		if( rdebug&RDEBUG_SHADE)
			bu_log("t[%s]:%g = in_pt[%s]:%g - floor():%g * cell[%s]%g\n",
				s, t[n], s, in_pt[n],
				floor(in_pt[n]/grass_sp->cell[n]),
				s, grass_sp->cell[n]);

		if (gr.r.r_dir[n] < -SMALL_FASTF) {
			tD[n] = -grass_sp->cell[n] / gr.r.r_dir[n];
			t[n] = t[n] / -gr.r.r_dir[n];
			t[n] += curr_dist;
		} else if (gr.r.r_dir[n] > SMALL_FASTF) {
			tD[n] = grass_sp->cell[n] / gr.r.r_dir[n];
			t[n] = grass_sp->cell[n] - t[n];
			t[n] = t[n] / gr.r.r_dir[n];
			t[n] += curr_dist;
		} else {
			tD[n] = t[n] = MAX_FASTF;
		}
		if (t[n] > out_dist) t[n] = out_dist;
		t_orig[n] = t[n];
		tD_iter[n] = 0;
	}
	

	if( rdebug&RDEBUG_SHADE) {
		bu_log("t[X]:%g tD[X]:%g\n", t[X], tD[X]);
		bu_log("t[Y]:%g tD[Y]:%g\n\n", t[Y], tD[Y]);
	}

	/* The flags argument indicates which cells (relative to the central
	 * one) have already been processed.  The bit values for the relative
	 * positions are:
	 *
	 *  pos   bit	 pos  bit   pos  bit
	 *------------------------------------
	 * -1, 1  0100	0, 1 0200  1, 1 0400
	 * -1, 0  0010  0, 0 0020  1, 0 0040
	 * -1,-1  0001  0,-1 0002  1,-1 0004
	 *
	 */
	flags = 0777;
	VMOVE(curr_pt, in_pt);
	cell_num[X] = (long)(in_pt[X] / grass_sp->cell[X]);
	cell_num[Y] = (long)(in_pt[Y] / grass_sp->cell[Y]);
	cell_num[Z] = 0.0;

	/* Time to go marching across the grid */
	while (curr_dist < out_dist) {


		if( rdebug&RDEBUG_SHADE) {
			point_t	cell_pos; /* cell origin position */

			if( rdebug&RDEBUG_SHADE) {
				bu_log("dist:%g (%g %g %g)\n", curr_dist,
					V3ARGS(curr_pt));
				bu_log("cell num: %d %d\n", V2ARGS(cell_num));
			}
			CELL_POS(cell_pos, grass_sp, cell_num);

			if( rdebug&RDEBUG_SHADE)
				bu_log("cell pos: %g %g\n", V2ARGS(cell_pos));
		}

		if (status = do_cells(cell_num, &gr, flags,
		    swp, out_dist, grass_sp))
			goto done;

		VMOVE(old_cell_num, cell_num);
		if (t[X] < t[Y]) {
			if( rdebug&RDEBUG_SHADE)
				bu_log("stepping X %le\n", t[X]);
			if (gr.r.r_dir[X] < 0.0) {
				flags = 0111;
				cell_num[X]--;
			} else {
				flags = 0444;
				cell_num[X]++;
			}
			n = X;
		} else {
			if( rdebug&RDEBUG_SHADE)
				bu_log("stepping Y %le\n", t[Y]);
			if (gr.r.r_dir[Y] < 0.0) {
				flags = 0007;
				cell_num[Y]--;
			} else {
				flags = 0700;
				cell_num[Y]++;
			}
			n = Y;
		}
		curr_dist = t[n];
		t[n] = t_orig[n] + tD[n] * ++tD_iter[n];

		if (t[n] > out_dist) t[n] = out_dist; 

		VJOIN1(curr_pt, gr.r.r_pt, curr_dist, gr.r.r_dir);
		curr_pt[n] = ((long) ((curr_pt[n] / grass_sp->cell[n]) + .1))
			 * grass_sp->cell[n];
	}


	if( rdebug&RDEBUG_SHADE)
		bu_log("Missed grass blades\n");

	/* Missed everything */
	/* setting basecolor to 1.0 prevents "filterglass" effect */
	VSETALL(swp->sw_basecolor, 1.0); 
	swp->sw_transmit = 1.0;
	(void)rr_render( ap, pp, swp );

	/* Tell stacker to abort shading */
	return 0;

done:
	if( rdebug&RDEBUG_SHADE)
		bu_log("Hit grass blades\n");


	/* if we got here then we hit something.  Convert the hit info
	 * back into model coordinates.
	 *
	 * Tell stacker to shade with plastic
	 * XXX another possibility here is to do:
	 *	extern struct mfuncs phg_mfuncs[];
	 *	return phg_mfuncs[1].mf_render(ap, pp, swp, dp);
	 * This would require calling the mf_setup routine in our _setup(),
	 * and hiding a pointer to a phong_specific in grass_specific.
	 */

	MAT4X3PNT(swp->sw_hit.hit_point, grass_sp->sh_to_m, gr.hit.hit_point);
	VSUB2(v, swp->sw_hit.hit_point, ap->a_ray.r_pt);
	swp->sw_hit.hit_dist = MAGNITUDE(v);

	swp->sw_transmit = 0.0;

	if (status & SHADE_ABORT_STACK) {
		if( rdebug&RDEBUG_SHADE)
			bu_log("Aborting stack (statistical grass)\n");
		return 0;
	}


	if (swp->sw_xmitonly) return 0;

	if( rdebug&RDEBUG_SHADE)
		bu_log("normal before xform:(%g %g %g)\n",
			V3ARGS(swp->sw_hit.hit_normal));

	MAT4X3VEC(swp->sw_hit.hit_normal, grass_sp->sh_to_m, gr.hit.hit_normal);

	if( rdebug&RDEBUG_SHADE) {
		VPRINT("Rnormal", gr.hit.hit_normal);
		VPRINT("Mnormal", swp->sw_hit.hit_normal);
		bu_log("MAG(Rnormal)%g, MAG(Mnormal)%g\n", MAGNITUDE(gr.hit.hit_normal), MAGNITUDE(swp->sw_hit.hit_normal));
	}
	VUNITIZE(swp->sw_hit.hit_normal);


	return 1;


}
