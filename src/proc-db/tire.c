/*                          T I R E . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file tire.c
 *
 * Tire Generator
 *
 * Program to create basic tire shapes.
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"

#define D2R(x) (x * DEG2RAD)
#define R2D(x) (x / DEG2RAD)

#define ROWS 5
#define COLS 5
#define F_PI 3.1415926535897932384626433832795029L 

/* Help message printed when -h option is supplied */
void show_help(const char *name) {
    printf("%s [-d <width>/<aspect>R<rim size>] [-a|-n <name>] [-t] [-h]\n", name);
    printf("-a\t\tAutomatically generate top level name \n\t\t(tire-<width>-<aspect>R<rim size>)\n\n");
    printf("-c <count>\tSpecify number of repeated tread patterns around tire\n\n");
    printf("-d <width>/<aspect>R<rim size>\n\t\tSpecify tire dimensions (American units)\n\n");
    printf("-g <depth>\tSpecify tread depth in terms of integer 32nds of an inch.\n\n");
    printf("-j <width>\tSpecify rim width in inches.\n\n");
    printf("-n <name>\tSpecify custom top level root name\n\n");
    printf("-p <type>\tGenerate tread with tread pattern as specified\n\n");
    printf("-t <type>\tGenerate tread with tread type as specified\n\n");
    printf("-u <thickness>\tSpecify tire thickness\n\n");
    printf("-h\t\tShow help\n\n");
    return;
}

/* Return matrix needed to rotate an object around the y axis by theta degrees*/
void getYRotMat(mat_t (*t), fastf_t theta)
{
    fastf_t sin_ = sin(theta);
    fastf_t cos_ = cos(theta);
    mat_t r;
    MAT_ZERO(r);
    r[0] = cos_;
    r[2] = sin_;
    r[5] = 1;
    r[8] = -sin_;
    r[10] = cos_;
    r[15] = 1;
    memcpy(*t, r, sizeof(*t));
}


/**********************************************************************
 *                                                                    *
 *               Explicit Solvers for Ellipse Equations               *
 *                                                                    *
 **********************************************************************/


/* Evaluate Partial Derivative of Ellipse Equation at a point */
fastf_t GetEllPartialAtPoint(fastf_t *inarray, fastf_t x, fastf_t y)
{
    fastf_t A,B,C,D,E,partial;
    A = inarray[0];
    B = inarray[1];
    C = inarray[2];
    D = inarray[3];
    E = inarray[4];
    partial = -(D+y*B+2*x*A)/(E+2*y*C+x*B);
    return partial;
}

/* Evaluate z value of Ellipse Equation at a point */
fastf_t GetValueAtZPoint(fastf_t *inarray, fastf_t y)
{
    fastf_t A,B,C,D,E,F,z;
    A = inarray[0];
    B = inarray[1];
    C = inarray[2];
    D = inarray[3];
    E = inarray[4];
    F = 1;
    z = (sqrt(-4*A*F-4*y*A*E+D*D+2*y*B*D-4*y*y*A*C+y*y*B*B)-D-y*B)/(2*A); 
    return z;
}


/**********************************************************************
 *                                                                    *
 *               Matrix Definitions for General Conic                 *
 *               Equations.                                           *
 *                                                                    *
 **********************************************************************/


/* Create General Conic Matrix for Ellipse describing tire slick (non-tread) surfaces */
 void Create_Ell1_Mat(fastf_t **mat, fastf_t dytred, fastf_t dztred, fastf_t d1, fastf_t ztire) 
{
    fastf_t y1,z1,y2,z2,y3,z3,y4,z4,y5,z5;

    y1 = dytred/2;
    z1 = ztire-dztred;
    y2 = dytred/2;
    z2 = ztire - (dztred+2*(d1-dztred));
    y3 = 0.0;
    z3 = ztire;
    y4 = 0.0;
    z4 = ztire-2*d1;
    y5 = -dytred/2;
    z5 = ztire-dztred;

    mat[0][0] = y1*y1;
    mat[0][1] = y1*z1;
    mat[0][2] = z1*z1;
    mat[0][3] = y1;
    mat[0][4] = z1;
    mat[0][5] = -1;
    mat[1][0] = y2*y2;
    mat[1][1] = y2*z2;
    mat[1][2] = z2*z2;
    mat[1][3] = y2;
    mat[1][4] = z2;
    mat[1][5] = -1;
    mat[2][0] = y3*y3;
    mat[2][1] = y3*z3;
    mat[2][2] = z3*z3;
    mat[2][3] = y3;
    mat[2][4] = z3;
    mat[2][5] = -1;
    mat[3][0] = y4*y4;
    mat[3][1] = y4*z4;
    mat[3][2] = z4*z4;
    mat[3][3] = y4;
    mat[3][4] = z4;
    mat[3][5] = -1;
    mat[4][0] = y5*y5;
    mat[4][1] = y5*z5;
    mat[4][2] = z5*z5;
    mat[4][3] = y5;
    mat[4][4] = z5;
    mat[4][5] = -1;
}
 
/* Create General Conic Matrix for Ellipse describing the tire side */
void Create_Ell2_Mat(fastf_t **mat, fastf_t dytred, fastf_t dztred, fastf_t dyside1, fastf_t zside1, fastf_t ztire, fastf_t dyhub, fastf_t zhub, fastf_t ell1partial) 
{
    mat[0][0] = (dyside1 / 2) * (dyside1 / 2);
    mat[0][1] = (zside1 * dyside1 / 2);
    mat[0][2] = zside1 * zside1;
    mat[0][3] = dyside1 / 2;
    mat[0][4] = zside1;
    mat[0][5] = -1;
    mat[1][0] = (dytred / 2) * (dytred / 2);
    mat[1][1] = (ztire - dztred) * (dytred / 2);
    mat[1][2] = (ztire - dztred) * (ztire - dztred);
    mat[1][3] = (dytred / 2);
    mat[1][4] = (ztire - dztred);
    mat[1][5] = -1;
    mat[2][0] = (dyhub/2) * (dyhub/2);
    mat[2][1] = (dyhub/2) * zhub;
    mat[2][2] = zhub * zhub;
    mat[2][3] = dyhub/2;
    mat[2][4] = zhub;
    mat[2][5] = -1;
    mat[3][0] = 2 * (dytred / 2);
    mat[3][1] = (ztire - dztred) + (dytred / 2) * ell1partial;
    mat[3][2] = 2*(ztire - dztred) * ell1partial;
    mat[3][3] = 1;
    mat[3][4] = ell1partial;
    mat[3][5] = 0;
    mat[4][0] = 2 * (dyhub / 2);
    mat[4][1] = zhub + (dyhub / 2) * -ell1partial;
    mat[4][2] = 2*zhub*-ell1partial;
    mat[4][3] = 1;
    mat[4][4] = -ell1partial;
    mat[4][5] = 0;
}

/**********************************************************************
 *                                                                    *
 *                 Matrix Routines for Solving General                *
 *                 Conic Equations.                                   *
 *                                                                    *
 **********************************************************************/


/* Sort Rows of 5x6 matrix - for use in Gaussian Elimination */
void SortRows(fastf_t **mat, int colnum)
{
    int high_row, exam_row;
    int colcount;
    fastf_t temp_elem;
    high_row = colnum;
    for (exam_row = high_row + 1; exam_row < 5; exam_row++) {
	if (fabs(mat[exam_row][colnum]) > fabs(mat[high_row][colnum]))
	    high_row = exam_row;
    }
    if (high_row != colnum) {
	for(colcount = colnum; colcount < 6; colcount++){
	    temp_elem = mat[colnum][colcount];
	    mat[colnum][colcount] = mat[high_row][colcount];
	    mat[high_row][colcount] = temp_elem;
	}
    }
}

/* Convert 5x6 matrix to Reduced Echelon Form */
void Echelon(fastf_t **mat)
{
    int i,j,k;
    fastf_t pivot,rowmult;
    for(i = 0; i < 5; i++){
	SortRows(mat,i);
	pivot = mat[i][i];
	for (j = i; j < 6; j++){
	    mat[i][j] = mat[i][j] / pivot;}
	for (k = i + 1; k < 5 ; k++){
	    rowmult=mat[k][i];
	    for (j = i; j < 6; j++){
		mat[k][j] -= rowmult*mat[i][j];
	    }
	}
     }
}
	   
/* Take Reduced Echelon form of Matrix and solve for 
 * General Conic Coefficients
 */ 
void SolveEchelon(fastf_t **mat, fastf_t *result1)
{
    int i,j;
    fastf_t inter;
    for(i = 4; i >= 0; i--){
	inter = mat[i][5];
	for(j = 4; j > i; j--){
	    inter -= mat[i][j]*result1[j];
	}
	result1[i]=inter;
    }
}


/**********************************************************************
 *                                                                    *
 *                 Convert General Conic Equation                     *
 *                 Coefficients to BRL-CAD eto input                  *
 *                 Parameters                                         *
 *                                                                    *
 **********************************************************************/


void CalcInputVals(fastf_t *inarray, fastf_t *outarray, int orientation)
{
    fastf_t A,B,C,D,E,Fp;
    fastf_t App, Bpp,Cpp;
    fastf_t x0,y0;
    fastf_t theta;
    fastf_t length1, length2;
    fastf_t semimajor, semiminor;
    fastf_t semimajorx,semimajory;

    A = inarray[0];
    B = inarray[1];
    C = inarray[2];
    D = inarray[3];
    E = inarray[4];

    /* Translation to Center of Ellipse */
    x0 = -(B*E-2*C*D)/(4*A*C-B*B);
    y0 = -(B*D-2*A*E)/(4*A*C-B*B);

    /* Translate to the Origin for Rotation */
    Fp = 1-y0*E-x0*D+y0*y0*C+x0*y0*B+x0*x0*A;

    /* Rotation Angle */
    theta = .5*atan(1000*B/(1000*A-1000*C));
    
    /* Calculate A'', B'' and C'' - B'' is zero with above theta choice */
    App = A*cos(theta)*cos(theta)+B*cos(theta)*sin(theta)+C*sin(theta)*sin(theta);
    Bpp = 2*(C-A)*cos(theta)*sin(theta)+B*cos(theta)*cos(theta)-B*sin(theta)*sin(theta);
    Cpp = A*sin(theta)*sin(theta)-B*sin(theta)*cos(theta)+C*cos(theta)*cos(theta);

    /* Solve for semimajor and semiminor lengths*/
    length1 = sqrt(-Fp/App);
    length2 = sqrt(-Fp/Cpp);

    /* BRL-CAD's eto primitive requires that C is the semimajor */
    if (length1 > length2) {
	semimajor = length1;
	semiminor = length2;
    } else {
	semimajor = length2;
	semiminor = length1;
    }

    /* Based on orientation of Ellipse, largest component of C is either in the
     * y direction (0) or z direction (1) - find the components.
     */

    if (orientation == 0){
	semimajorx = semimajor*cos(-theta);
	semimajory = semimajor*sin(-theta);
    }else{
	semimajorx = semimajor*sin(-theta);
	semimajory = semimajor*cos(-theta);
    }

    /* Return final BRL-CAD input parameters */
    outarray[0] = -x0;
    outarray[1] = -y0;
    outarray[2] = semimajorx;
    outarray[3] = semimajory;
    outarray[4] = semiminor;
}

/**********************************************************************
 *                                                                    *
 *                 Create Wheel Structure to fit                      *
 *                 Specified Tire Parameters                          *
 *                                                                    *
 **********************************************************************/



void MakeWheelCenter(struct rt_wdb (*file), char *suffix, fastf_t fixing_start_middle, fastf_t fixing_width, fastf_t rim_thickness, fastf_t bead_height, fastf_t zhub, fastf_t dyhub, fastf_t spigot_diam, int bolts,fastf_t bolt_circ_diam, fastf_t bolt_diam) 
{

    vect_t height, a, b, c;
    point_t origin, vertex;
    mat_t y;
    struct bu_vls str;
    bu_vls_init(&str);
    int i;
    struct wmember bolthole,boltholes,hubhole,hubholes,innerhub;

    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Inner-Hub%s.s", suffix);	
    VSET(origin, 0, fixing_start_middle+fixing_width/4, 0);
    VSET(a, (zhub-2*bead_height),0,0);
    VSET(b, 0, -dyhub/3, 0);
    VSET(c, 0, 0, (zhub-2*bead_height));
    mk_ell(file,bu_vls_addr(&str), origin, a, b, c);
   
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Inner-Hub-Cut1%s.s", suffix);
    VSET(origin, 0, fixing_start_middle+fixing_width/4-rim_thickness*1.5, 0);
    mk_ell(file,bu_vls_addr(&str), origin, a, b, c);
	
    VSET(origin, 0,0, 0);
    VSET(height, 0, dyhub/2, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Inner-Hub-Cut2%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), origin, height, spigot_diam/2);   

    /* Make the circular pattern of holes in the hub - this involves the creation of
     * one primitive, a series of transformed combinations which use that primitive,
     * and a combination combining all of the previous combinations.  Since it requires
     * both primitives and combinations it is placed between the two sections.
     */

    VSET(vertex, 0, 0, (zhub - (bolt_circ_diam/2+bolt_diam/2))/1.25);
    VSET(height, 0, dyhub/2, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Hub-Hole%s.s", suffix); 
    mk_rcc(file, bu_vls_addr(&str), vertex, height,(zhub - (bolt_circ_diam/2+bolt_diam/2))/6.5 );


    BU_LIST_INIT(&hubhole.l);
    BU_LIST_INIT(&hubholes.l);
    for (i=1; i<=10; i++) {
	bu_vls_trunc(&str,0);
	bu_vls_printf(&str, "Hub-Hole%s.s",suffix);
	getYRotMat(&y,D2R(i*360/10));
	(void)mk_addmember(bu_vls_addr(&str), &hubhole.l, y, WMOP_UNION);
	bu_vls_trunc(&str,0);
	bu_vls_printf(&str, "Hub-Hole-%1.0d%s.c", i, suffix);
	mk_lcomb(file, bu_vls_addr(&str), &hubhole, 0, NULL, NULL, NULL, 0);
	(void)mk_addmember(bu_vls_addr(&str), &hubholes.l, NULL, WMOP_UNION);
	(void)BU_LIST_POP_T(&hubhole.l,struct wmember);
    }
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Hub-Holes%s.c",suffix);
    mk_lcomb(file, bu_vls_addr(&str), &hubholes, 0, NULL, NULL, NULL, 0);

    /* Make the bolt holes in the hub 
     */

    VSET(vertex, 0, 0, bolt_circ_diam/2);
    VSET(height, 0, dyhub/2, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Bolt-Hole%s.s", suffix); 
    mk_rcc(file, bu_vls_addr(&str), vertex, height, bolt_diam/2 );


    BU_LIST_INIT(&bolthole.l);
    BU_LIST_INIT(&boltholes.l);
    for (i=1; i<=bolts; i++) {
	bu_vls_trunc(&str,0);
	bu_vls_printf(&str, "Bolt-Hole%s.s",suffix);
	getYRotMat(&y,D2R(i*360/bolts));
	(void)mk_addmember(bu_vls_addr(&str), &bolthole.l, y, WMOP_UNION);
	bu_vls_trunc(&str,0);
	bu_vls_printf(&str, "Bolt-Hole-%1.0d%s.c", i, suffix);
	mk_lcomb(file, bu_vls_addr(&str), &bolthole, 0, NULL, NULL, NULL, 0);
	(void)mk_addmember(bu_vls_addr(&str), &boltholes.l, NULL, WMOP_UNION);
	(void)BU_LIST_POP_T(&bolthole.l,struct wmember);
    }
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Bolt-Holes%s.c",suffix);
    mk_lcomb(file, bu_vls_addr(&str), &boltholes, 0, NULL, NULL, NULL, 0);
    
    /*Make combination for Inner-Hub*/ 
    BU_LIST_INIT(&innerhub.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Inner-Hub%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &innerhub.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Inner-Hub-Cut1%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &innerhub.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Inner-Hub-Cut2%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &innerhub.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Hub-Holes%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &innerhub.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Bolt-Holes%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &innerhub.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Inner-Hub%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &innerhub, 0, NULL, NULL, NULL, 0);
}


void MakeWheelRims(struct rt_wdb (*file), char *suffix, fastf_t dyhub, fastf_t zhub, int bolts, fastf_t bolt_diam, fastf_t bolt_circ_diam, fastf_t spigot_diam, fastf_t fixing_offset, fastf_t bead_height, fastf_t bead_width, fastf_t rim_thickness)
{
    struct wmember tireleftbead, tirerightbead, tireleftflat, tirerightflat, tirefixingface;
    struct wmember tirebeadlefttrans, tirebeadrighttrans, wheelrim,  wheel;
    fastf_t inner_width, left_width, right_width, fixing_width, inner_width_left_start, inner_width_right_start;
    fastf_t fixing_width_left_trans, fixing_width_right_trans, fixing_width_middle;
    fastf_t fixing_start_left, fixing_start_right, fixing_start_middle;
    unsigned char rgb[3];
    vect_t normal, height;
    point_t vertex;
    struct bu_vls str;
    bu_vls_init(&str);
    
    /* Set wheel color */
    VSET(rgb, 217, 217, 217);
    
    /* Insert primitives */
    VSET(vertex, 0, -dyhub/2, 0);
    VSET(height, 0, bead_width, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Left-Bead%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, zhub);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Left-Bead-Cut%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, zhub - rim_thickness);
    VSET(vertex, 0, dyhub/2, 0);
    VSET(height, 0, -bead_width, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Right-Bead%s.s", suffix); 
    mk_rcc(file, bu_vls_addr(&str), vertex, height, zhub);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Right-Bead-Cut%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, zhub - rim_thickness);
    VSET(vertex, 0, -dyhub/2+bead_width, 0);
    VSET(normal, 0, 1, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Left-Bead-Trans%s.s", suffix);	
    mk_cone(file, bu_vls_addr(&str), vertex, normal, bead_width, zhub, zhub-bead_height);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Left-Bead-Trans-cut%s.s", suffix);
    mk_cone(file, bu_vls_addr(&str), vertex, normal, bead_width, zhub-rim_thickness, zhub-bead_height-rim_thickness);
    VSET(vertex, 0, dyhub/2-bead_width, 0);
    VSET(normal, 0, -1, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Right-Bead-Trans%s.s", suffix);	
    mk_cone(file, bu_vls_addr(&str), vertex, normal, bead_width, zhub, zhub-bead_height);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Right-Bead-Trans-cut%s.s", suffix);
    mk_cone(file, bu_vls_addr(&str), vertex, normal, bead_width, zhub-rim_thickness, zhub-bead_height-rim_thickness);
    
    /* At this point, some intermediate values are calculated to ease the bookkeeping */

    inner_width = dyhub-4*bead_width;
    inner_width_left_start = -dyhub/2 + 2*bead_width;
    inner_width_right_start = dyhub/2 - 2*bead_width;
    fixing_width = .25*inner_width;
    fixing_width_left_trans = .25*fixing_width;
    fixing_width_right_trans = .25*fixing_width;
    fixing_width_middle = .5*fixing_width;
    fixing_start_middle = fixing_offset - fixing_width_middle/2;
    fixing_start_left = fixing_start_middle - fixing_width_left_trans;
    fixing_start_right = fixing_offset + fixing_width_middle;
    right_width = inner_width_right_start - fixing_start_left - fixing_width;
    left_width = inner_width_left_start - fixing_start_left;

    VSET(vertex, 0, inner_width_left_start, 0);
    VSET(height, 0, -left_width, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "LeftInnerRim%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, zhub - bead_height);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "LeftInnerRim-cut%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, zhub - rim_thickness - bead_height);
    VSET(vertex, 0, inner_width_right_start, 0);
    VSET(height, 0, -right_width, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "RightInnerRim%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, zhub - bead_height);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "RightInnerRim-cut%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, zhub - rim_thickness - bead_height);
    VSET(vertex, 0, fixing_start_left, 0);
    VSET(normal, 0, 1, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Left-Fixing-Trans%s.s", suffix);	
    mk_cone(file, bu_vls_addr(&str), vertex, normal, fixing_width_left_trans, zhub - bead_height, zhub-2*bead_height);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Left-Fixing-Trans-cut%s.s", suffix);
    mk_cone(file, bu_vls_addr(&str), vertex, normal, fixing_width_left_trans, zhub- bead_height -rim_thickness, zhub-2*bead_height-rim_thickness);
    VSET(vertex, 0, fixing_start_right, 0);
    VSET(normal, 0, -1, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Right-Fixing-Trans%s.s", suffix);	
    mk_cone(file, bu_vls_addr(&str), vertex, normal, fixing_width_right_trans, zhub - bead_height, zhub-2*bead_height);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Right-Fixing-Trans-cut%s.s", suffix);
    mk_cone(file, bu_vls_addr(&str), vertex, normal, fixing_width_right_trans, zhub- bead_height -rim_thickness, zhub-2*bead_height-rim_thickness);
    VSET(vertex, 0, fixing_start_middle, 0);
    VSET(height, 0, fixing_width_middle, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Inner-Fixing%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, zhub - 2*bead_height);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Inner-Fixing-cut%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, zhub - rim_thickness - 2*bead_height);


    /*Make combination for Left bead*/ 
    BU_LIST_INIT(&tireleftbead.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Left-Bead%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tireleftbead.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Left-Bead-Cut%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tireleftbead.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Left-Bead%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tireleftbead, 0, NULL, NULL, NULL, 0);

    /*Make combination for Right bead*/ 
    BU_LIST_INIT(&tirerightbead.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Right-Bead%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tirerightbead.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Right-Bead-Cut%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tirerightbead.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Right-Bead%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tirerightbead, 0, NULL, NULL, NULL, 0);

    /*Make combination for Left bead transition*/ 
    BU_LIST_INIT(&tirebeadlefttrans.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Left-Bead-Trans%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tirebeadlefttrans.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Left-Bead-Trans-cut%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tirebeadlefttrans.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Left-Bead-Trans%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tirebeadlefttrans, 0, NULL, NULL, NULL, 0);

    /*Make combination for Right bead transition*/ 
    BU_LIST_INIT(&tirebeadrighttrans.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Right-Bead-Trans%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tirebeadrighttrans.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Right-Bead-Trans-cut%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tirebeadrighttrans.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Right-Bead-Trans%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tirebeadrighttrans, 0, NULL, NULL, NULL, 0);

    /*Make combination for Left Flat*/ 
    BU_LIST_INIT(&tireleftflat.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "LeftInnerRim%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tireleftflat.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "LeftInnerRim-cut%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tireleftflat.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Left-Inner-Rim%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tireleftflat, 0, NULL, NULL, NULL, 0);

    /*Make combination for Right Flat*/ 
    BU_LIST_INIT(&tirerightflat.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "RightInnerRim%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tirerightflat.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "RightInnerRim-cut%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tirerightflat.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Right-Inner-Rim%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tirerightflat, 0, NULL, NULL, NULL, 0);

    /*Make combination for Fixing */
    BU_LIST_INIT(&tirefixingface.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Left-Fixing-Trans%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tirefixingface.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Left-Fixing-Trans-cut%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tirefixingface.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Right-Fixing-Trans%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tirefixingface.l, NULL, WMOP_UNION);
     bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Right-Fixing-Trans-cut%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tirefixingface.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Inner-Fixing%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tirefixingface.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Inner-Fixing-cut%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tirefixingface.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Fixing%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tirefixingface, 0, NULL, NULL, NULL, 0);

    /*Make wheel rim combination*/
    BU_LIST_INIT(&wheelrim.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Left-Bead%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &wheelrim.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Right-Bead%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &wheelrim.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Left-Bead-Trans%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &wheelrim.l, NULL, WMOP_UNION);
     bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Right-Bead-Trans%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &wheelrim.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Left-Inner-Rim%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &wheelrim.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Right-Inner-Rim%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &wheelrim.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Fixing%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &wheelrim.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Wheel-Rim%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &wheelrim, 0, NULL, NULL, NULL, 0);


    MakeWheelCenter(file, suffix, fixing_start_middle, fixing_width, rim_thickness, bead_height, zhub, dyhub, spigot_diam, bolts, bolt_circ_diam, bolt_diam);



    /*Make combination for Wheel*/ 
    BU_LIST_INIT(&wheel.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Inner-Hub%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &wheel.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Wheel-Rim%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &wheel.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "wheel%s.r", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &wheel, 1, "plastic", "di=.8 sp=.2", rgb, 0);

}


/**********************************************************************
 *                                                                    *
 *               Extrusion Creation Routines for Tire                 *
 *               Tread Patterns - makes sketch and                    *
 *               uses sketch to make extrustion                       *
 *                                                                    *
 **********************************************************************/

void MakeExtrude(struct rt_wdb (*file), char *suffix, point2d_t *verts, int vertcount, fastf_t patternwidth1, fastf_t patternwidth2, fastf_t tirewidth, fastf_t zbase, fastf_t ztire)
{
    struct rt_sketch_internal *skt;
    struct line_seg *lsg;
    point_t V;
    vect_t u_vec, v_vec, h;
    int i;
    struct bu_vls str;
    bu_vls_init(&str);
    struct bu_vls str2;
    bu_vls_init(&str2);
    point2d_t tmpvert[] = {{0,0}};

    /* Basic allocation of structure */
    skt = (struct rt_sketch_internal *)bu_calloc( 1, sizeof( struct rt_sketch_internal ), "sketch" );
    skt->magic = RT_SKETCH_INTERNAL_MAGIC;


    /* Set vertex and orientation vectors?? */
    VSET( V, 0, -tirewidth/2, zbase-.1*zbase );
    VSET( u_vec, 1, 0, 0 );
    VSET( v_vec, 0, 1, 0 );
    VMOVE( skt->V, V );
    VMOVE( skt->u_vec, u_vec );
    VMOVE( skt->v_vec, v_vec );



    /* Define links between/order of vertices */
    skt->vert_count = vertcount;/* this probably needs to be counted at input or included in pre-defineds */
    skt->verts = (point2d_t *)bu_calloc( skt->vert_count, sizeof( point2d_t ), "verts" );
    for ( i=0; i<skt->vert_count; i++ ) {
	tmpvert[0][0] = verts[i][0]*patternwidth2-patternwidth2/2;
	tmpvert[0][1] = verts[i][1]*tirewidth;
	V2MOVE( skt->verts[i], tmpvert[0] );
    }

    /* Specify number of segments and allocate memory for reverse?? and segments */
    skt->skt_curve.seg_count = vertcount;
    skt->skt_curve.reverse = (int *)bu_calloc( skt->skt_curve.seg_count, sizeof( int ), "sketch: reverse" );    
    skt->skt_curve.segments = (genptr_t *)bu_calloc( skt->skt_curve.seg_count, sizeof( genptr_t ), "segs" );

    
    /* Insert all line segments except the last one */
    for (i = 0; i < vertcount-1; i++){
	lsg = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "sketch: lsg" );
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = i;
	lsg->end = i+1;
	skt->skt_curve.segments[i] = (genptr_t)lsg;
    }

    /* Connect the last connected vertex to the first vertex */
    lsg = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "sketch: lsg" );
    lsg->magic = CURVE_LSEG_MAGIC;
    lsg->start = vertcount-1;
    lsg->end = 0;
    skt->skt_curve.segments[vertcount-1] = (genptr_t)lsg;

    /* Make the sketch */
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str,"sketch%s",suffix);
    mk_sketch(file, bu_vls_addr(&str), skt );


    /* Make first slanted extrusion for depth vs. width of tread effect */
    VSET( h, patternwidth1/2-patternwidth2/2, 0, ztire-(zbase-.11*zbase));
    bu_vls_trunc(&str2,0);
    bu_vls_printf(&str2,"extrude1%s",suffix);
    mk_extrusion(file, bu_vls_addr(&str2), bu_vls_addr(&str), V, h, u_vec, v_vec, 0);    

    /* Make second slanted extrusion for depth vs. width effect */
    VSET( h, -patternwidth1/2+patternwidth2/2, 0, ztire-(zbase-.11*zbase)); 
    bu_vls_trunc(&str2,0);
    bu_vls_printf(&str2,"extrude2%s",suffix);
    mk_extrusion(file, bu_vls_addr(&str2), bu_vls_addr(&str), V, h, u_vec, v_vec, 0);

    /* Direct extrusion */
    VSET( h, 0, 0, ztire-(zbase-.11*zbase));
    bu_vls_trunc(&str2,0);
    bu_vls_printf(&str2,"extrude3%s",suffix);
    mk_extrusion(file, bu_vls_addr(&str2), bu_vls_addr(&str), V, h, u_vec, v_vec, 0);
}


void MakeTreadPattern2(struct rt_wdb (*file), char *suffix, fastf_t dwidth, fastf_t z_base, fastf_t ztire, int number_of_patterns)
{
    struct bu_vls str;
    bu_vls_init(&str);
    struct bu_vls str2;
    bu_vls_init(&str2);
    struct wmember treadpattern, tread, treadrotated;
    fastf_t patternwidth1,patternwidth2;
    mat_t y;
    int i,j;
    int sketchnum = 4;
    int vertcounts[sketchnum];
    point2d_t *verts[sketchnum];
    unsigned char rgb[3];
    VSET(rgb, 40, 40, 40);

    patternwidth1=ztire*sin(F_PI/number_of_patterns);
    patternwidth2=z_base*sin(F_PI/number_of_patterns);
    
    point2d_t verts1[] = {
	{ 0, 0 },
	{ 0, .1 },
	{ .66, .34 },
	{ .34, .66 },
	{ 1, .9 },
	{ 1, 1 },

	{ 1.4, 1 },
	{ 1.33, .9 },
	{ .66, .66 },
	{ 1, .34 },
	{ .33, .1 },
	{ .4, 0 }
    };
    
    verts[0] = verts1;
    vertcounts[0] = 12;
    
    point2d_t verts2[] = {
	{ .2, .13 },
	{ -.5, .2 },
	{ -.2, .27 },
	{ .4, .2 }
    };

    verts[1] = verts2;
     vertcounts[1] = 4;

    point2d_t verts3[] = {
        { .5, .45 },
        { -.1, .4 },
        { -.4, .5 },
        { .3, .55 }
    };

    verts[2] = verts3;
     vertcounts[2] = 4;

    point2d_t verts4[] = {
        { .6, .73 },
        { -.3, .8 },
        { -.1, .87 },
        { .8, .8 }
    };

    verts[3] = verts4;
    vertcounts[3] = 4;

    BU_LIST_INIT(&treadpattern.l);

    for ( i=0; i<sketchnum; i++ ){
	bu_vls_trunc(&str,0);
	bu_vls_printf(&str,"-%d%s",i+1,suffix);
	MakeExtrude(file, bu_vls_addr(&str), verts[i], vertcounts[i], 2*patternwidth1, 2*patternwidth2, dwidth, z_base, ztire);
	for (j = 1; j<=3; j++) {
	    bu_vls_trunc(&str,0);
	    bu_vls_printf(&str, "extrude%d-%d%s",j,i+1,suffix);
	    (void)mk_addmember(bu_vls_addr(&str), &treadpattern.l, NULL, WMOP_UNION);
	}
    }

    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tread_master%s.c",suffix);
    mk_lcomb(file, bu_vls_addr(&str), &treadpattern, 0, NULL, NULL, NULL, 0);

    BU_LIST_INIT(&tread.l);
    BU_LIST_INIT(&treadrotated.l);

    /*
    for (i=1; i<=number_of_patterns; i++) {
	bu_vls_trunc(&str,0);
	bu_vls_printf(&str, "tread_master%s.c",suffix);
	getYRotMat(&y,i*2*F_PI/number_of_patterns);
	bu_vls_trunc(&str2,0);
	bu_vls_printf(&str2, "tire-tread-shape%s.c",suffix);
	(void)mk_addmember(bu_vls_addr(&str2),&tread.l, NULL, WMOP_UNION);
	(void)mk_addmember(bu_vls_addr(&str), &tread.l, y, WMOP_SUBTRACT);
	bu_vls_trunc(&str,0);
	bu_vls_printf(&str, "tread-component-%d%s.c", i,suffix);
	mk_lcomb(file, bu_vls_addr(&str), &tread, 0, NULL, NULL, NULL, 0);
	(void)mk_addmember(bu_vls_addr(&str), &treadrotated.l, NULL, WMOP_UNION);
	(void)BU_LIST_POP_T(&tread.l,struct wmember);
    }
    */
    
    bu_vls_trunc(&str2,0);
    bu_vls_printf(&str2, "tire-tread-shape%s.c",suffix);
    (void)mk_addmember(bu_vls_addr(&str2),&tread.l, NULL, WMOP_UNION);
    for (i=1; i<=number_of_patterns; i++) {
	bu_vls_trunc(&str,0);
	bu_vls_printf(&str, "tread_master%s.c",suffix);
	getYRotMat(&y,i*2*F_PI/number_of_patterns);
	(void)mk_addmember(bu_vls_addr(&str), &tread.l, y, WMOP_SUBTRACT);
    }
    

    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tread%s.c",suffix);
    mk_lcomb(file, bu_vls_addr(&str), &tread, 0, NULL,NULL,NULL ,0);
}


void MakeTreadPattern1(struct rt_wdb (*file), char *suffix, fastf_t dwidth, fastf_t z_base, fastf_t ztire, int number_of_patterns)
{
    struct bu_vls str;
    bu_vls_init(&str);
    struct bu_vls str2;
    bu_vls_init(&str2);
    struct wmember treadpattern, tread, treadrotated;
    fastf_t patternwidth1,patternwidth2;
    mat_t y;
    int i,j;
    int sketchnum = 9;
    int vertcounts[sketchnum];
    point2d_t *verts[sketchnum];
    unsigned char rgb[3];
    VSET(rgb, 40, 40, 40);

    patternwidth1=ztire*sin(F_PI/number_of_patterns);
    patternwidth2=z_base*sin(F_PI/number_of_patterns);
    
    point2d_t verts1[] = {
	{ .9, 0 },
	{ .6, .3 },
	{ .94,.7 },
	{ .45, 1 },

	{ .58, 1 },
	{ 1.07, .7 },
	{ .73, .3 },
	{ 1.03,0 }
    };
    
    verts[0] = verts1;
    vertcounts[0] = 8;
    
    point2d_t verts2[] = {
	{ .3,0 },
	{ .0,.3 },
	{ .34, .7 },
	{ -.15, 1 },

	{ 0.05,1 },
	{ .44,.7 },
	{ .1,.3 },
	{ .4,0 }
    };
    
    verts[1] = verts2;
    vertcounts[1] = 8;

    point2d_t verts3[] = {
        { -.1,.1 },
        { -.1,.12 },
        { 1.1,.12 },
        { 1.1,.1 }
    };

    verts[2] = verts3;
     vertcounts[2] = 4;

    point2d_t verts4[] = {
        { -.1,.20 },
        { -.1,.23 },
        { 1.1,.23 },
        { 1.1,.20 }
    };

    verts[3] = verts4;
    vertcounts[3] = 4;

    point2d_t verts5[] = {
        { -.1,.37 },
        { -.1,.4 },
        { 1.1,.4 },
        { 1.1,.37 }
    };

    verts[4] = verts5;
    vertcounts[4] = 4;
 
    point2d_t verts6[] = {
        { -.1,.49 },
        { -.1,.51},
        { 1.1,.51 },
        { 1.1,.49 }
    };

    verts[5] = verts6;
    vertcounts[5] = 4;

    point2d_t verts7[] = {
        { -.1,.6 },
        { -.1,.63 },
        { 1.1,.63 },
        { 1.1,.6 }
    };

    verts[6] = verts7;
    vertcounts[6] = 4;

    point2d_t verts8[] = {
        { -.1,.77 },
        { -.1,.80 },
        { 1.1,.80 },
        { 1.1,.77 }
    };

    verts[7] = verts8;
    vertcounts[7] = 4;

    point2d_t verts9[] = {
        { -.1,.88 },
        { -.1,.9 },
        { 1.1,.9 },
        { 1.1,.88 }
    };

    verts[8] = verts9;
    vertcounts[8] = 4;

    BU_LIST_INIT(&treadpattern.l);

    for ( i=0; i<sketchnum; i++ ){
	bu_vls_trunc(&str,0);
	bu_vls_printf(&str,"-%d%s",i+1,suffix);
	MakeExtrude(file, bu_vls_addr(&str), verts[i], vertcounts[i], 2*patternwidth1, 2*patternwidth2, dwidth, z_base, ztire);
	for (j = 1; j<=3; j++) {
	    bu_vls_trunc(&str,0);
	    bu_vls_printf(&str, "extrude%d-%d%s",j,i+1,suffix);
	    (void)mk_addmember(bu_vls_addr(&str), &treadpattern.l, NULL, WMOP_UNION);
	}
    }

    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tread_master%s.c",suffix);
    mk_lcomb(file, bu_vls_addr(&str), &treadpattern, 0, NULL, NULL, NULL, 0);

    BU_LIST_INIT(&tread.l);
    BU_LIST_INIT(&treadrotated.l);

    /*
    for (i=1; i<=number_of_patterns; i++) {
	bu_vls_trunc(&str,0);
	bu_vls_printf(&str, "tread_master%s.c",suffix);
	getYRotMat(&y,i*2*F_PI/number_of_patterns);
	bu_vls_trunc(&str2,0);
	bu_vls_printf(&str2, "tire-tread-shape%s.c",suffix);
	(void)mk_addmember(bu_vls_addr(&str2),&tread.l, NULL, WMOP_UNION);
	(void)mk_addmember(bu_vls_addr(&str), &tread.l, y, WMOP_SUBTRACT);
	bu_vls_trunc(&str,0);
	bu_vls_printf(&str, "tread-component-%d%s.c", i,suffix);
	mk_lcomb(file, bu_vls_addr(&str), &tread, 0, NULL, NULL, NULL, 0);
	(void)mk_addmember(bu_vls_addr(&str), &treadrotated.l, NULL, WMOP_UNION);
	(void)BU_LIST_POP_T(&tread.l,struct wmember);
    }
    */
    
    bu_vls_trunc(&str2,0);
    bu_vls_printf(&str2, "tire-tread-shape%s.c",suffix);
    (void)mk_addmember(bu_vls_addr(&str2),&tread.l, NULL, WMOP_UNION);
    for (i=1; i<=number_of_patterns; i++) {
	bu_vls_trunc(&str,0);
	bu_vls_printf(&str, "tread_master%s.c",suffix);
	getYRotMat(&y,i*2*F_PI/number_of_patterns);
	(void)mk_addmember(bu_vls_addr(&str), &tread.l, y, WMOP_SUBTRACT);
    }
    

    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tread%s.c",suffix);
    mk_lcomb(file, bu_vls_addr(&str), &tread, 0, NULL,NULL,NULL ,0);
}

void TreadPattern(struct rt_wdb (*file), char *suffix, fastf_t dwidth, fastf_t z_base, fastf_t ztire, int number_of_patterns, int patterntype)
{
    typedef void (* MakeTreadPattern)(struct rt_wdb (*file), char *suffix, fastf_t dwidth, fastf_t z_base, fastf_t ztire, int number_of_patterns);

    MakeTreadPattern TreadPatterns[2];
    TreadPatterns[0] = &MakeTreadPattern1;
    TreadPatterns[1] = &MakeTreadPattern2;
    (*TreadPatterns[patterntype-1])(file, suffix, dwidth, z_base, ztire, number_of_patterns);
}

/**********************************************************************
 *                                                                    *
 *           Routine which does actual primitive insertion            *
 *           to form slick tire surfaces, using results               *
 *           of General Conic Equation solver.  Also forms            *
 *           proper combinations of inserted primitives.              *
 *                                                                    *
 **********************************************************************/


void MakeTireSurface(struct rt_wdb (*file), char *suffix, fastf_t *ell1cadparams, fastf_t *ell2cadparams, fastf_t ztire, fastf_t dztred, fastf_t dytred, fastf_t dyhub, fastf_t zhub, fastf_t dyside1)
{
    struct wmember tiresideoutercutright, tiresideoutercutleft, tiresideinnercutright, tiresideinnercutleft,tirecuttopcyl;
    struct wmember tireslick, tiresides, tiresurface;
    struct wmember innersolid;
    struct wmember tire; 
    struct bu_vls str;
    bu_vls_init(&str);
    vect_t vertex,height;
    point_t origin,normal,C;
   
    /* Insert primitives */
    VSET(origin, 0, ell1cadparams[0], 0);
    VSET(normal, 0, 1, 0);
    VSET(C, 0, ell1cadparams[2],ell1cadparams[3]);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse1%s.s", suffix);	
    mk_eto(file, bu_vls_addr(&str), origin, normal, C, ell1cadparams[1], ell1cadparams[4]);
    VSET(origin, 0, ell2cadparams[0], 0);
    VSET(normal, 0, ell2cadparams[2]/sqrt(ell2cadparams[2]*ell2cadparams[2]), 0);
    VSET(C, 0, ell2cadparams[2],ell2cadparams[3]);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse2%s.s", suffix);	
    mk_eto(file, bu_vls_addr(&str), origin, normal, C, ell2cadparams[1], ell2cadparams[4]);
    VSET(origin, 0, -ell2cadparams[0], 0);
    VSET(normal, 0, -ell2cadparams[2]/sqrt(ell2cadparams[2]*ell2cadparams[2]), 0);
    VSET(C, 0, -ell2cadparams[2],-ell2cadparams[3]);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse3%s.s", suffix);	
    mk_eto(file, bu_vls_addr(&str), origin, normal, C, ell2cadparams[1], ell2cadparams[4]);
    VSET(vertex, 0, 0, 0);
    VSET(height, 0, ell1cadparams[2]+ell1cadparams[2]*.01, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "TopClipR%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, ztire - dztred);
    VSET(vertex, 0, 0, 0);
    VSET(height, 0, -ell1cadparams[2]-ell1cadparams[2]*.01, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "TopClipL%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, ztire - dztred);
    VSET(vertex, 0, -dyside1/2-0.1*dyside1/2,0);
    VSET(height, 0, dyside1+0.1*dyside1, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "SideClipInner%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, zhub);
    VSET(vertex, 0, -dytred/2, 0);
    VSET(height, 0, dytred, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "SideClipOuter1%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, ztire - dztred);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "SideClipOuter2%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, ztire);
 
    /* Insert primitives to ensure a solid interior - based
     * on dimensions, either add or subtract cones from the
     * sides of the solid.
     */
    VSET(vertex, 0, -dytred/2, 0);
    VSET(height, 0, dytred, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "InnerSolid%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, ztire-dztred);
    if (!NEAR_ZERO(dytred/2 - dyhub/2, SMALL_FASTF)){
	VSET(normal, 0, 1, 0);
	bu_vls_trunc(&str,0);
    	bu_vls_printf(&str, "LeftCone%s.s", suffix);	
    	mk_cone(file, bu_vls_addr(&str), vertex, normal, dytred/2 - dyhub/2, ztire-dztred,zhub);
    	VSET(vertex, 0, dytred/2, 0);
    	VSET(normal, 0, -1, 0);
    	bu_vls_trunc(&str,0);
    	bu_vls_printf(&str, "RightCone%s.s", suffix);	
    	mk_cone(file, bu_vls_addr(&str), vertex, normal, dytred/2 - dyhub/2, ztire-dztred,zhub);
    }	

    /* Define the tire tread surface trimming volume */
    BU_LIST_INIT(&tirecuttopcyl.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "SideClipOuter2%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tirecuttopcyl.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "SideClipOuter1%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tirecuttopcyl.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0) ;
    bu_vls_printf(&str, "tire-outer-sides-trim%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tirecuttopcyl, 0, NULL, NULL, NULL, 0);
    
    /* Define outer cut for right (positive y) side of tire */
    BU_LIST_INIT(&tiresideoutercutright.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse2%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresideoutercutright.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-outer-sides-trim%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresideoutercutright.l, NULL, WMOP_INTERSECT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-side-outer-cut-right%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tiresideoutercutright, 0, NULL, NULL, NULL, 0);
 
    /* Define outer cut for left (negative y) side of tire */
    BU_LIST_INIT(&tiresideoutercutleft.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse3%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresideoutercutleft.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-outer-sides-trim%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresideoutercutleft.l, NULL, WMOP_INTERSECT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-side-outer-cut-left%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tiresideoutercutleft, 0, NULL, NULL, NULL, 0);

    /* Define inner cut for right (positive y) side of tire */
    BU_LIST_INIT(&tiresideinnercutright.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse2%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresideinnercutright.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "SideClipInner%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresideinnercutright.l, NULL, WMOP_INTERSECT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-side-inner-cut-right%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tiresideinnercutright, 0, NULL, NULL, NULL, 0);

    /* Define inner cut for left (negative y) side of tire */
    BU_LIST_INIT(&tiresideinnercutleft.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse3%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresideinnercutleft.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "SideClipInner%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresideinnercutleft.l, NULL, WMOP_INTERSECT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-side-inner-cut-left%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tiresideinnercutleft, 0, NULL, NULL, NULL, 0);

    /* Combine cuts and primitives to make final tire side surfaces */
    BU_LIST_INIT(&tiresides.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse2%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresides.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-side-inner-cut-right%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresides.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-side-outer-cut-right%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresides.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse3%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresides.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-side-inner-cut-left%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresides.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-side-outer-cut-left%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresides.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-sides%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tiresides, 0, NULL, NULL, NULL, 0);

    /* Combine cuts and primitives to make final tire slick surface */
    BU_LIST_INIT(&tireslick.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse1%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tireslick.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "TopClipR%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tireslick.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "TopClipL%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tireslick.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-slick-surface%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tireslick, 0, NULL, NULL, NULL, 0);

    /* Combine tire tread surface and tire side surfaces to make final
     * tire surface.
     */
    BU_LIST_INIT(&tiresurface.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-sides%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresurface.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-slick-surface%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresurface.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-surface%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tiresurface, 0, NULL, NULL, NULL, 0);

    /* Combine inner solid, cones, and cuts to ensure a filled inner volume. */
    BU_LIST_INIT(&innersolid.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "InnerSolid%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &innersolid.l, NULL, WMOP_UNION);
    if ((dytred/2 - dyhub/2) > 0 && !NEAR_ZERO(dytred/2 - dyhub/2, SMALL_FASTF)) {
    	bu_vls_trunc(&str,0);
    	bu_vls_printf(&str, "LeftCone%s.s", suffix);	
	(void)mk_addmember(bu_vls_addr(&str), &innersolid.l, NULL, WMOP_SUBTRACT);
    	bu_vls_trunc(&str,0);
    	bu_vls_printf(&str, "RightCone%s.s", suffix);	
	(void)mk_addmember(bu_vls_addr(&str), &innersolid.l, NULL, WMOP_SUBTRACT);
    }
    if ((dytred/2 - dyhub/2) < 0 && !NEAR_ZERO(dytred/2 - dyhub/2, SMALL_FASTF)) {
    	bu_vls_trunc(&str,0);
    	bu_vls_printf(&str, "LeftCone%s.s", suffix);	
	(void)mk_addmember(bu_vls_addr(&str), &innersolid.l, NULL, WMOP_UNION);
    	bu_vls_trunc(&str,0);
    	bu_vls_printf(&str, "RightCone%s.s", suffix);	
	(void)mk_addmember(bu_vls_addr(&str), &innersolid.l, NULL, WMOP_UNION);
    }
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "SideClipInner%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &innersolid.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-solid%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &innersolid, 0, NULL, NULL, NULL, 0);

    /* Assemble surfaces and interior fill to make final tire shape */
    BU_LIST_INIT(&tire.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-surface%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tire.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-solid%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tire.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tire, 0, NULL, NULL, NULL, 0);
  
    bu_vls_free(&str);
}

/**********************************************************************
 *                                                                    *
 *           Routines to handle creation of tread solids              *
 *           and invocation of the tread pattern creater               *
 *                                                                    *
 **********************************************************************/

void MakeTreadSolid(struct rt_wdb (*file), char *suffix, fastf_t *ell2coefficients, fastf_t ztire, fastf_t dztred, fastf_t d1, fastf_t dytred, fastf_t dyhub, fastf_t zhub, fastf_t dyside1, int number_of_tread_patterns, int pattern_type)
{
    fastf_t **matrixelltred1, **matrixelltred2;
    fastf_t ell1tredcoefficients[5],ell2tredcoefficients[5];
    fastf_t ell1tredcadparams[5],ell2tredcadparams[5];
    fastf_t d1_intercept;
    fastf_t elltredpartial;
    struct wmember tiretreadshape;
    int i;
    struct bu_vls str;
    bu_vls_init(&str);
 
    matrixelltred1 = (fastf_t **)bu_malloc(5 * sizeof(fastf_t *),"matrixrows");
    for (i = 0; i < 5; i++)
	matrixelltred1[i] = (fastf_t *)bu_malloc(6 * sizeof(fastf_t),"matrixcols");

    matrixelltred2 = (fastf_t **)bu_malloc(5 * sizeof(fastf_t *),"matrixrows");
    for (i = 0; i < 5; i++)
	matrixelltred2[i] = (fastf_t *)bu_malloc(6 * sizeof(fastf_t),"matrixcols");


    /* Find point on side where tread will start */
    d1_intercept = GetValueAtZPoint(ell2coefficients,ztire-d1);

    Create_Ell1_Mat(matrixelltred1, dytred, dztred, d1, ztire);
    Echelon(matrixelltred1);
    SolveEchelon(matrixelltred1,ell1tredcoefficients);
    if ((ztire-d1) < (dytred/2)){
	CalcInputVals(ell1tredcoefficients,ell1tredcadparams,1);
    }else{
	CalcInputVals(ell1tredcoefficients,ell1tredcadparams,0);
    }
    elltredpartial = GetEllPartialAtPoint(ell1tredcoefficients,dytred/2,ztire-dztred);
    Create_Ell2_Mat(matrixelltred2, dytred, dztred, d1_intercept*2, ztire-d1, ztire, dyhub, zhub, elltredpartial);
    Echelon(matrixelltred2);
    SolveEchelon(matrixelltred2,ell2tredcoefficients);
    if ((ztire-dztred)-zhub > (dyside1/2-dytred/2)){
	CalcInputVals(ell2tredcoefficients,ell2tredcadparams,1);
    }else{
	CalcInputVals(ell2tredcoefficients,ell2tredcadparams,0);
    }
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str,"-tread-outer%s",suffix);
    MakeTireSurface(file,bu_vls_addr(&str),ell1tredcadparams,ell2tredcadparams,ztire,dztred,dytred,dyhub,zhub,d1_intercept*2);
	

    /* The tire tread shape needed is the subtraction of the slick surface from the tread shape,
     * which is handled here to supply the correct shape for later tread work.
     */
    BU_LIST_INIT(&tiretreadshape.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str,"tire-tread-outer%s.c",suffix);
    (void)mk_addmember(bu_vls_addr(&str), &tiretreadshape.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str,"tire-solid%s.c",suffix);
    (void)mk_addmember(bu_vls_addr(&str), &tiretreadshape.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str,"tire-tread-shape%s.c",suffix);
    mk_lcomb(file, bu_vls_addr(&str), &tiretreadshape, 0, NULL, NULL, NULL, 0);
    
    
    /* Call function to generate primitives and combinations for actual tread pattern */
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str,"%s",suffix);
    TreadPattern(file, bu_vls_addr(&str), d1_intercept*2, ztire-d1, ztire, number_of_tread_patterns,pattern_type);

    for (i = 0; i < 5; i++)
    	bu_free((char *)matrixelltred1[i], "matrixell1 element");
    bu_free((char *)matrixelltred1,"matrixell1");
    for (i = 0; i < 5; i++)
    	bu_free((char *)matrixelltred2[i], "matrixell2 element");
    bu_free((char *)matrixelltred2,"matrixell2");


}


void MakeTreadSolid1(struct rt_wdb (*file), char *suffix, fastf_t *ell2coefficients, fastf_t ztire, fastf_t dztred, fastf_t d1, fastf_t dytred, fastf_t dyhub, fastf_t zhub, fastf_t dyside1, int number_of_tread_patterns, int patterntype)
{
    fastf_t **matrixelltred1, **matrixelltred2;
    fastf_t ell1tredcoefficients[5];
    fastf_t ell1tredcadparams[5];
    fastf_t d1_intercept;
    struct wmember tiretreadsolid, tiretreadshape;
    int i;
    struct bu_vls str;
    bu_vls_init(&str);
    vect_t vertex,height;
    point_t origin,normal,C;
 
    matrixelltred1 = (fastf_t **)bu_malloc(5 * sizeof(fastf_t *),"matrixrows");
    for (i = 0; i < 5; i++)
	matrixelltred1[i] = (fastf_t *)bu_malloc(6 * sizeof(fastf_t),"matrixcols");

    matrixelltred2 = (fastf_t **)bu_malloc(5 * sizeof(fastf_t *),"matrixrows");
    for (i = 0; i < 5; i++)
	matrixelltred2[i] = (fastf_t *)bu_malloc(6 * sizeof(fastf_t),"matrixcols");


    /* Find point on side where tread will start */
    d1_intercept = GetValueAtZPoint(ell2coefficients,ztire-d1);

    Create_Ell1_Mat(matrixelltred1, d1_intercept*2, dztred, d1, ztire);
    Echelon(matrixelltred1);
    SolveEchelon(matrixelltred1,ell1tredcoefficients);
    CalcInputVals(ell1tredcoefficients,ell1tredcadparams,0);
    VSET(origin, 0, ell1tredcadparams[0], 0);
    VSET(normal, 0, 1, 0);
    VSET(C, 0, ell1tredcadparams[2],ell1tredcadparams[3]);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse-tread-outer%s.s", suffix);	
    mk_eto(file, bu_vls_addr(&str), origin, normal, C, ell1tredcadparams[1], ell1tredcadparams[4]);
    
    VSET(vertex, 0, -d1_intercept, 0);
    VSET(height, 0, d1_intercept*2, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse-constrain%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, ztire);

    VSET(vertex, 0, -d1_intercept, 0);
    VSET(height, 0, d1_intercept*2, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse-tread-inner-cut%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, d1);
    
    BU_LIST_INIT(&tiretreadsolid.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str,"Ellipse-tread-outer%s.s",suffix);
    (void)mk_addmember(bu_vls_addr(&str), &tiretreadsolid.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str,"Ellipse-constrain%s.s",suffix);
    (void)mk_addmember(bu_vls_addr(&str), &tiretreadsolid.l, NULL, WMOP_INTERSECT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str,"Ellipse-tread-inner-cut%s.s",suffix);
    (void)mk_addmember(bu_vls_addr(&str), &tiretreadsolid.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str,"tire-tread-outer%s.c",suffix);
    mk_lcomb(file, bu_vls_addr(&str), &tiretreadsolid, 0, NULL, NULL, NULL, 0);
	

    /* The tire tread shape needed is the subtraction of the slick surface from the tread shape,
     * which is handled here to supply the correct shape for later tread work.
     */
    BU_LIST_INIT(&tiretreadshape.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str,"tire-tread-outer%s.c",suffix);
    (void)mk_addmember(bu_vls_addr(&str), &tiretreadshape.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str,"tire-solid%s.c",suffix);
    (void)mk_addmember(bu_vls_addr(&str), &tiretreadshape.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str,"tire-tread-shape%s.c",suffix);
    mk_lcomb(file, bu_vls_addr(&str), &tiretreadshape, 0, NULL, NULL, NULL, 0);
    
    
    /* Call function to generate primitives and combinations for actual tread pattern */
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str,"%s",suffix);
    TreadPattern(file, bu_vls_addr(&str), d1_intercept*2, ztire-d1, ztire, number_of_tread_patterns,patterntype);

    for (i = 0; i < 5; i++)
    	bu_free((char *)matrixelltred1[i], "matrixell1 element");
    bu_free((char *)matrixelltred1,"matrixell1");
    for (i = 0; i < 5; i++)
    	bu_free((char *)matrixelltred2[i], "matrixell2 element");
    bu_free((char *)matrixelltred2,"matrixell2");


}

/**********************************************************************
 *                                                                    *
 *           MakeTire is the "top level" tire generation              *
 *           function - it is responsible for managing the            *
 *           matricies, calling the solvers with the correct          *
 *           input parameters, and using the other tire               *
 *           routines to define a hollow tire with tread.             *
 *                                                                    *
 *           Decisions such as tread extrusion type, tread            *
 *           pattern, and whether to insert a wheel are               *
 *           handled at this level.                                   *
 *                                                                    *
 **********************************************************************/


void MakeTire(struct rt_wdb (*file), char *suffix, fastf_t dytred, fastf_t dztred, fastf_t d1, fastf_t dyside1, fastf_t zside1, fastf_t ztire, fastf_t dyhub, fastf_t zhub, fastf_t thickness, int tread_type, int number_of_tread_patterns, fastf_t tread_depth, fastf_t patterntype)
{
    int i;
    fastf_t ell1partial;
    fastf_t ell1coefficients[5],ell2coefficients[5];
    fastf_t ell1cadparams[5],ell2cadparams[5];
    fastf_t **matrixell1,**matrixell2;
    fastf_t cut_dytred,cut_dztred,cut_d1,cut_dyside1,cut_zside1,cut_ztire,cut_dyhub,cut_zhub;
    fastf_t cut1partial;
    fastf_t cut1coefficients[5],cut2coefficients[5];
    fastf_t cut1cadparams[5],cut2cadparams[5];
    fastf_t **matrixcut1,**matrixcut2;
    fastf_t ztire_with_offset;
    struct wmember tire;
    unsigned char rgb[3];

    struct bu_vls str;
    bu_vls_init(&str);
    struct bu_vls str2;
    bu_vls_init(&str2);


    /* Set Tire color */
    VSET(rgb, 40, 40, 40);


    if (tread_type != 0){
	ztire_with_offset = ztire-tread_depth*bu_units_conversion("in");
    } else {
	ztire_with_offset = ztire;
    }

    matrixell1 = (fastf_t **)bu_malloc(5 * sizeof(fastf_t *),"matrixrows");
    for (i = 0; i < 5; i++)
	matrixell1[i] = (fastf_t *)bu_malloc(6 * sizeof(fastf_t),"matrixcols");
    
    matrixell2 = (fastf_t **)bu_malloc(5 * sizeof(fastf_t *),"matrixrows");
    for (i = 0; i < 5; i++)
	matrixell2[i] = (fastf_t *)bu_malloc(6 * sizeof(fastf_t),"matrixcols");


 

    matrixcut1 = (fastf_t **)bu_malloc(5 * sizeof(fastf_t *),"matrixrows");
    for (i = 0; i < 5; i++)
	matrixcut1[i] = (fastf_t *)bu_malloc(6 * sizeof(fastf_t),"matrixcols");
    
    matrixcut2 = (fastf_t **)bu_malloc(5 * sizeof(fastf_t *),"matrixrows");
    for (i = 0; i < 5; i++)
	matrixcut2[i] = (fastf_t *)bu_malloc(6 * sizeof(fastf_t),"matrixcols");

    /* Find slick surface ellipse equation */
    Create_Ell1_Mat(matrixell1, dytred, dztred, d1, ztire_with_offset);
    Echelon(matrixell1);
    SolveEchelon(matrixell1,ell1coefficients);
    ell1partial = GetEllPartialAtPoint(ell1coefficients,dytred/2,ztire_with_offset-dztred);
    /* Find outer side ellipse equation */
    Create_Ell2_Mat(matrixell2, dytred, dztred, dyside1, zside1, ztire_with_offset, dyhub, zhub, ell1partial);
    Echelon(matrixell2);
    SolveEchelon(matrixell2,ell2coefficients);
    /* Calculate BRL-CAD input parameters for outer tread ellipse */
    if ((ztire-d1) < (dytred/2)){
	CalcInputVals(ell1coefficients,ell1cadparams,1);
    }else{
	CalcInputVals(ell1coefficients,ell1cadparams,0);
    }
    /* Calculate BRL-CAD input parameters for outer side ellipse */
    if ((ztire-dztred)-zhub > (dyside1/2-dytred/2)){
	CalcInputVals(ell2coefficients,ell2cadparams,1);
    } else {
	CalcInputVals(ell2coefficients,ell2cadparams,0);
    }
    /* Insert outer tire volume */
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str,"-solid%s",suffix);
    MakeTireSurface(file,bu_vls_addr(&str),ell1cadparams,ell2cadparams,ztire_with_offset,dztred,dytred,dyhub,zhub,dyside1);


    typedef void (*MakeTreadProfile) (struct rt_wdb (*file), char *suffix, fastf_t *ell2coefficients, fastf_t ztire, fastf_t dztred, fastf_t d1, fastf_t dytred, fastf_t dyhub, fastf_t zhub, fastf_t dyside1, int number_of_tread_patterns,int patterntype);
    
    MakeTreadProfile TreadProfile[2];
    TreadProfile[0] = &MakeTreadSolid;
    TreadProfile[1] = &MakeTreadSolid1;

    /* Make Tread solid and call routine to create tread */

    if (tread_type != 0) {
	(*TreadProfile[tread_type-1])(file,suffix,ell2coefficients,ztire,dztred,d1,dytred,dyhub,zhub,dyside1,number_of_tread_patterns,patterntype);
    }
    
    /* Calculate input parameters for inner cut*/
    cut_ztire = ztire_with_offset-thickness;
    cut_dyside1 = dyside1-thickness*2;
    cut_zside1 = zside1;
    cut_d1 = d1;
    cut_dytred = dytred*cut_dyside1/dyside1;
    cut_dztred = dztred*cut_ztire/ztire_with_offset;
    cut_dyhub = dyhub - thickness*2;
    cut_zhub = zhub;

    /* Find inner tread cut ellipse equation */
    Create_Ell1_Mat(matrixcut1, cut_dytred, cut_dztred, cut_d1, cut_ztire);
    Echelon(matrixcut1);
    SolveEchelon(matrixcut1,cut1coefficients);
    cut1partial = GetEllPartialAtPoint(cut1coefficients,cut_dytred/2,cut_ztire-cut_dztred);
    /* Find inner cut side ellipse equation */
    Create_Ell2_Mat(matrixcut2, cut_dytred, cut_dztred, cut_dyside1, cut_zside1, cut_ztire, cut_dyhub, cut_zhub, cut1partial);
    Echelon(matrixcut2);
    SolveEchelon(matrixcut2,cut2coefficients);
    /* Calculate BRL-CAD input parameters for inner cut tread ellipse */
    if ((cut_ztire-cut_d1) < (cut_dytred/2)){
	CalcInputVals(cut1coefficients,cut1cadparams,1);
    }else{
	CalcInputVals(cut1coefficients,cut1cadparams,0);
    }
    /* Calculate BRL-CAD input parameters for inner cut side ellipse */
    if ((cut_ztire-cut_dztred)-cut_zhub > (cut_dyside1/2-cut_dytred/2)){
	CalcInputVals(cut2coefficients,cut2cadparams,1);
    } else {
	CalcInputVals(cut2coefficients,cut2cadparams,0);
    }
    /* Insert inner tire cut volume */
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str,"-cut%s",suffix);
    MakeTireSurface(file,bu_vls_addr(&str),cut1cadparams,cut2cadparams,cut_ztire,cut_dztred,cut_dytred,cut_dyhub,zhub,dyside1);

   /* Combine the tire solid, tire cut and tread into the
    * final tire region.
    */
    BU_LIST_INIT(&tire.l);

    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-solid%s.c",suffix);
    (void)mk_addmember(bu_vls_addr(&str), &tire.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-cut%s.c",suffix);
    (void)mk_addmember(bu_vls_addr(&str), &tire.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tread%s.c",suffix);
    if (tread_type != 0) (void)mk_addmember(bu_vls_addr(&str),&tire.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire%s.r",suffix);
    mk_lcomb(file, bu_vls_addr(&str), &tire, 1,  "plastic", "di=.8 sp=.2", rgb, 0);







    for (i = 0; i < 5; i++)
    	bu_free((char *)matrixell1[i], "matrixell1 element");
    bu_free((char *)matrixell1,"matrixell1");
    for (i = 0; i < 5; i++)
    	bu_free((char *)matrixell2[i], "matrixell2 element");
    bu_free((char *)matrixell2,"matrixell2");


    for (i = 0; i < 5; i++)
    	bu_free((char *)matrixcut1[i], "matrixcut1 element");
    bu_free((char *)matrixcut1,"matrixcut1");
    for (i = 0; i < 5; i++)
    	bu_free((char *)matrixcut2[i], "matrixell2 element");
    bu_free((char *)matrixcut2,"matrixell2");


}





/* Process command line arguments */
int ReadArgs(int argc, char **argv, fastf_t *isoarray, struct bu_vls *name, struct bu_vls *dimens, int *gen_name, int *treadtype, int *number_of_tread_patterns, int *tread_depth, int *tire_thickness, int *hub_width, int *pattern_type)
{
    int c = 0;
    char *options="d:n:c:g:j:p:t:u:ah";
    int d1, d2, d3;
    int count;
    int treadep,tdtype,tthickness, hwidth, ptype;
    char spacer1,tiretype;
 
    bu_opterr = 0;

    while ((c=bu_getopt(argc, argv, options)) != -1) {
        switch (c) {
	    case 'a' :
		*gen_name = 1;
		break;
	    case 'c' :
		sscanf(bu_optarg,"%d",&count);
		*number_of_tread_patterns = count;
		break;
	    case 'd' :
		sscanf(bu_optarg,"%d%c%d%c%d",&d1,&spacer1,&d2,&tiretype,&d3);
		bu_log("Dimensions: Width=%2.0dmm, Ratio=%2.0d, Wheel Diameter=%2.0din\n",d1,d2,d3);
		bu_vls_printf(dimens,"%d-%dR%d",d1,d2,d3);
		isoarray[0] = d1;
		isoarray[1] = d2;
		isoarray[2] = d3;
                break;
	    case 'g':
		sscanf(bu_optarg,"%d",&treadep);
		*tread_depth = treadep;
		break;
	    case 'j':
		sscanf(bu_optarg,"%d",&hwidth);
		*hub_width = hwidth;
		break;
	    case 'n':
		bu_vls_trunc(name,0);
		bu_vls_printf(name, "%s", bu_optarg);
		break;
	    case 'p':
		sscanf(bu_optarg,"%d",&ptype);
		*pattern_type = ptype;
		break;
	    case 't':
		sscanf(bu_optarg,"%d",&tdtype);
		*treadtype = tdtype;
		break;
	    case 'u':
		sscanf(bu_optarg,"%d",&tthickness);
		*tire_thickness = tthickness;
		break;
	    case 'h':
		show_help(*argv);
		bu_exit(EXIT_SUCCESS, "exiting after help");
	}
	 }
	 return(bu_optind);
 }


int main(int ac, char *av[])
{
    struct rt_wdb *db_fp;
    fastf_t dytred,dztred,dyside1,zside1,ztire,dyhub,zhub,d1;
    fastf_t width, ratio, wheeldiam;
    int bolts;
    fastf_t bolt_diam, bolt_circ_diam, spigot_diam, fixing_offset, bead_height, bead_width, rim_thickness;
    struct wmember wheel_and_tire;
    fastf_t isoarray[3];
    struct bu_vls name;
    struct bu_vls dimen;
    struct bu_vls str;
    int gen_name = 0;
    int tread_type = 0;
    int number_of_tread_patterns = 30;
    int tread_depth = 11;
    int tire_thickness = 0;
    fastf_t f_tire_thickness = 0;
    int hub_width = 0;
    int pattern_type = 0;

    bu_vls_init(&str);
    bu_vls_init(&name);
    bu_vls_init(&dimen);
    bu_vls_trunc(&name,0);
    bu_vls_trunc(&dimen,0);

    /* Set Default Parameters - 215/55R17 */ 
    isoarray[0] = 215;
    isoarray[1] = 55;
    isoarray[2] = 17;

    /* Process arguments */
    ReadArgs(ac, av, isoarray, &name, &dimen, &gen_name, &tread_type, &number_of_tread_patterns, &tread_depth,&tire_thickness, &hub_width, &pattern_type);

    /* Calculate floating point value for tread depth */
    fastf_t tread_depth_float = tread_depth/32.0;

    /* Based on arguments, assign name for toplevel object
     * Default of "tire" is respected unless overridden by
     * user supplied options.
     */
    if (bu_vls_strlen(&name) == 0 && gen_name == 0) {
	bu_vls_printf(&name, "tire");
    }

    if (bu_vls_strlen(&name) != 0 && gen_name == 1) {
	bu_vls_printf(&name, "-%d-%dR%d",(int)isoarray[0],(int)isoarray[1],(int)isoarray[2]);
    }

    if (bu_vls_strlen(&name) == 0 && gen_name == 1) {
	bu_vls_printf(&name, "tire-%d-%dR%d",(int)isoarray[0],(int)isoarray[1],(int)isoarray[2]);
    }
    

    /* Use default dimensional info to create a suffix for names, if not supplied in args */
    if (bu_vls_strlen(&dimen) == 0) {
	bu_vls_printf(&dimen, "-%d-%dR%d",(int)isoarray[0],(int)isoarray[1],(int)isoarray[2]);
    }

    /* Create file name if supplied,
     * else use "tire.g"
     */
    db_fp = wdb_fopen( av[bu_optind] );
    if (db_fp == NULL){
	db_fp = wdb_fopen("tire.g");
        mk_id(db_fp, "Tire");
    }


    /*Automatic conversion from std dimension info to geometry*/
    width = isoarray[0];
    ratio = isoarray[1];
    wheeldiam = isoarray[2]*bu_units_conversion("in");
    dyside1 = width;
    ztire = ((width*ratio/100)*2+wheeldiam)/2;
    zhub = ztire-width*ratio/100;
    zside1 = ztire-((ztire-zhub)/2*1.2);
    dztred = .001*ratio*zside1;
    dytred = .8 * width;
    d1 = (ztire-zhub)/2.5;
   
    if(tire_thickness == 0){
	f_tire_thickness = dztred;
    }else{
	f_tire_thickness = tire_thickness;
    }

    if(hub_width == 0){
	dyhub = dytred;
    }else{
	dyhub = hub_width*bu_units_conversion("in");
    }

    if (tread_type == 1 && pattern_type == 0) pattern_type = 1;
    if (tread_type == 2 && pattern_type == 0) pattern_type = 2;
    if (pattern_type == 1 && tread_type == 0) tread_type = 1;
    if (pattern_type == 2 && tread_type == 0) tread_type = 2;


    /* Make the tire region*/
    MakeTire(db_fp, bu_vls_addr(&dimen), dytred, dztred, d1, dyside1, zside1, ztire, dyhub, zhub, f_tire_thickness, tread_type, number_of_tread_patterns, tread_depth_float, pattern_type);
     

    bolts = 5;
    bolt_diam = 10;
    bolt_circ_diam = 75;
    spigot_diam = 40;
    fixing_offset = 15;
    bead_height = 8;
    bead_width = 8;
    rim_thickness = f_tire_thickness/2.0;

    /* Make the wheel region*/
    MakeWheelRims(db_fp, bu_vls_addr(&dimen), dyhub, zhub, bolts, bolt_diam, bolt_circ_diam, spigot_diam, fixing_offset, bead_height, bead_width, rim_thickness);

    /* Final top level providing a single name for tire+wheel */
    BU_LIST_INIT(&wheel_and_tire.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire%s.r",bu_vls_addr(&dimen));
    (void)mk_addmember(bu_vls_addr(&str), &wheel_and_tire.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "wheel%s.r",bu_vls_addr(&dimen));
    (void)mk_addmember(bu_vls_addr(&str), &wheel_and_tire.l, NULL, WMOP_UNION);
    mk_lcomb(db_fp, bu_vls_addr(&name), &wheel_and_tire, 0,  NULL, NULL, NULL, 0);


    /* Close database */
    wdb_close(db_fp);

    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
