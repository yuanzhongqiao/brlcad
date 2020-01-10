/*            C D T _ O V L P S _ G R P S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2019 United States Government as represented by
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
/** @addtogroup libbrep */
/** @{ */
/** @file cdt_ovlps.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 * This file is logic specifically for refining meshes to clear overlaps
 * between sets of triangles in different meshes that need to share a common
 * tessellation.
 *
 */

#include "common.h"
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include "bu/str.h"
#include "bg/chull.h"
#include "bg/tri_pt.h"
#include "bg/tri_tri.h"
#include "./cdt.h"

void
ovlp_grp::list_tris()
{
    std::set<size_t>::iterator t_it;
    std::cout << "      " << om1->fmesh->name << " " << om1->fmesh->f_id << ": " << tris1.size() << " tris\n";
    for (t_it = tris1.begin(); t_it != tris1.end(); t_it++) {
	std::cout << "      " << *t_it << "\n";
    }
    std::cout << "      " << om2->fmesh->name << " " << om2->fmesh->f_id << ": " << tris2.size() << " tris\n";
    for (t_it = tris2.begin(); t_it != tris2.end(); t_it++) {
	std::cout << "      " << *t_it << "\n";
    }
}

void
ovlp_grp::list_overts()
{
    std::set<overt_t *>::iterator o_it;
    std::cout << "      " << om1->fmesh->name << " " << om1->fmesh->f_id << ": " << overts1.size() << " verts\n";
    for (o_it = overts1.begin(); o_it != overts1.end(); o_it++) {
	std::cout << "      " << (*o_it)->p_id << "\n";
    }
    std::cout << "      " << om2->fmesh->name << " " << om2->fmesh->f_id << ": " << overts2.size() << " verts\n";
    for (o_it = overts2.begin(); o_it != overts2.end(); o_it++) {
	std::cout << "      " << (*o_it)->p_id << "\n";
    }
}

void
ovlp_grp::fit_plane()
{
    std::set<ON_3dPoint> plane1_pnts;
    std::set<ON_3dPoint> plane2_pnts;
    std::set<ON_3dPoint> plane_pnts;
    std::set<long>::iterator vp_it;
    for (vp_it = verts1.begin(); vp_it != verts1.end(); vp_it++) {
	ON_3dPoint p = *om1->fmesh->pnts[*vp_it];
	plane1_pnts.insert(p);
	plane_pnts.insert(p);
    }
    for (vp_it = verts2.begin(); vp_it != verts2.end(); vp_it++) {
	ON_3dPoint p = *om2->fmesh->pnts[*vp_it];
	plane2_pnts.insert(p);
	plane_pnts.insert(p);
    }

    ON_Plane fplane1;
    {
	point_t pcenter;
	vect_t pnorm;
	point_t *fpnts = (point_t *)bu_calloc(plane1_pnts.size(), sizeof(point_t), "fitting points");
	std::set<ON_3dPoint>::iterator p_it;
	int pcnt = 0;
	for (p_it = plane1_pnts.begin(); p_it != plane1_pnts.end(); p_it++) {
	    fpnts[pcnt][X] = p_it->x;
	    fpnts[pcnt][Y] = p_it->y;
	    fpnts[pcnt][Z] = p_it->z;
	    pcnt++;
	}
	if (bn_fit_plane(&pcenter, &pnorm, plane1_pnts.size(), fpnts)) {
	    std::cout << "fitting plane failed!\n";
	}
	bu_free(fpnts, "fitting points");
	fplane1 = ON_Plane(pcenter, pnorm);
    }
    fp1 = fplane1;

    ON_Plane fplane2;
    {
	point_t pcenter;
	vect_t pnorm;
	point_t *fpnts = (point_t *)bu_calloc(plane2_pnts.size(), sizeof(point_t), "fitting points");
	std::set<ON_3dPoint>::iterator p_it;
	int pcnt = 0;
	for (p_it = plane2_pnts.begin(); p_it != plane2_pnts.end(); p_it++) {
	    fpnts[pcnt][X] = p_it->x;
	    fpnts[pcnt][Y] = p_it->y;
	    fpnts[pcnt][Z] = p_it->z;
	    pcnt++;
	}
	if (bn_fit_plane(&pcenter, &pnorm, plane2_pnts.size(), fpnts)) {
	    std::cout << "fitting plane failed!\n";
	}
	bu_free(fpnts, "fitting points");
	fplane2 = ON_Plane(pcenter, pnorm);
    }
    fp2 = fplane2;

    if (acos(ON_DotProduct(fplane1.Normal(), fplane2.Normal())) > 0.785398) {
	// If the planes are too far from parallel, we're done - there is no
	// sensible common plane for projections.
	return;
    }

    ON_Plane fplane;
    {
	point_t pcenter;
	vect_t pnorm;
	point_t *fpnts = (point_t *)bu_calloc(plane_pnts.size(), sizeof(point_t), "fitting points");
	std::set<ON_3dPoint>::iterator p_it;
	int pcnt = 0;
	for (p_it = plane_pnts.begin(); p_it != plane_pnts.end(); p_it++) {
	    fpnts[pcnt][X] = p_it->x;
	    fpnts[pcnt][Y] = p_it->y;
	    fpnts[pcnt][Z] = p_it->z;
	    pcnt++;
	}
	if (bn_fit_plane(&pcenter, &pnorm, plane_pnts.size(), fpnts)) {
	    std::cout << "fitting plane failed!\n";
	}
	bu_free(fpnts, "fitting points");
	fplane = ON_Plane(pcenter, pnorm);
    }

    fp1 = fplane;
    fp2 = fplane;
}

void
ovlp_grp::characterize_verts(int ind)
{
    omesh_t *other_m = (!ind) ? om2 : om1;
    std::set<overt_t *> &ov1 = (!ind) ? overts1 : overts2;
    std::set<overt_t *> &ov2 = (!ind) ? overts2 : overts1;

    std::map<overt_t *, overt_t *> &ovm1 = (!ind) ? om1_om2_verts : om2_om1_verts;
    std::map<overt_t *, overt_t *> &ovm2 = (!ind) ? om2_om1_verts : om1_om2_verts;
    std::set<overt_t *> &ovum = (!ind) ? om1_unmappable_rverts : om2_unmappable_rverts;

    std::set<overt_t *>::iterator ov_it;
    for (ov_it = ov1.begin(); ov_it != ov1.end(); ov_it++) {
	overt_t *ov = *ov_it;
	overt_t *nv = other_m->vert_closest(NULL, ov);

	if (ovm2.find(nv) != ovm2.end()) {
	    // We already have a mapping on this mesh for nv - closer one wins,
	    // further is added to unmappable_verts for later consideration
	    overt_t *ov_orig = ovm2[nv];
	    if (ov_orig == ov) continue;
	    double d_orig = ov_orig->vpnt().DistanceTo(nv->vpnt());
	    double d_new = ov->vpnt().DistanceTo(nv->vpnt());

	    if (d_orig < d_new) {
		ovum.insert(ov);
		std::cout << "Skipping(" << d_new << "): " << ov->vpnt().x << "," << ov->vpnt().y << "," << ov->vpnt().z << "\n";
	    } else {
		ovum.insert(ov_orig);
		ovm2[nv] = ov;
		std::cout << "Displacing(" << d_orig << "): " << ov_orig->vpnt().x << "," << ov_orig->vpnt().y << "," << ov_orig->vpnt().z << "\n";
		std::cout << "       New(" << d_new << "): " << ov->vpnt().x << "," << ov->vpnt().y << "," << ov->vpnt().z << "\n";
	    }
	    continue;
	}

	// Find any points whose matching closest surface point isn't a vertex
	// in the other mesh per the vtree.  That point is a refinement point.
	ON_3dPoint target_point = ov->vpnt();
	double pdist = ov->bb.Diagonal().Length() * 10;
	ON_3dPoint s_p;
	ON_3dVector s_n;
	bool feval = other_m->fmesh->closest_surf_pnt(s_p, s_n, &target_point, 2*pdist);
	if (!feval) {
	    std::cout << "Error - couldn't find closest point for unpaired vert\n";
	    continue;
	}
	ON_BoundingBox spbb(s_p, s_p);
	//std::cout << "ov " << ov->p_id << " closest vert " << nv->p_id << ", dist " << s_p.DistanceTo(nv->vpnt()) << "\n";

	// If the opposite vertex is close to the current vertex per their bounding boxes and is close
	// to the closet surface point, go with it
	std::set<long> &v2 = (!ind) ? verts2 : verts1;
	if (!nv->bb.IsDisjoint(spbb) && (s_p.DistanceTo(nv->vpnt()) < 2*ON_ZERO_TOLERANCE)) {
	    ovm1[ov] = nv;
	    ovm2[nv] = ov;
	    // Make sure both vert sets store all the required vertices
	    ov2.insert(nv);
	    v2.insert(nv->p_id);
	    continue;
	}

	//std::cout << "Need new vert paring(" << s_p.DistanceTo(nv->vpnt()) << "): " << target_point.x << "," << target_point.y << "," << target_point.z << "\n";

	uedge_t closest_edge = other_m->closest_uedge(s_p);
	if (other_m->fmesh->brep_edges.find(closest_edge) != other_m->fmesh->brep_edges.end()) {
	    bedge_seg_t *bseg = other_m->fmesh->ue2b_map[closest_edge];
	    if (!bseg) {
		std::cout << "couldn't find bseg pointer??\n";
	    } else {
		//std::cout << "Edge refinement point\n";
		std::set<overt_t *> &ovev = (!ind) ? om2_everts_from_om1 : om1_everts_from_om2;
		ovev.insert(ov);

		(*edge_verts)[bseg].insert(ov);
	    }
	} else {
	    //std::cout << "Non edge refinement point\n";
	    std::set<overt_t *> &ovrv = (!ind) ? om2_rverts_from_om1 : om1_rverts_from_om2;
	    ovrv.insert(ov);

	    std::set<size_t> uet = other_m->fmesh->uedges2tris[closest_edge];
	    std::set<size_t>::iterator u_it;
	    for (u_it = uet.begin(); u_it != uet.end(); u_it++) {
		other_m->ivert_ref_cnts[ov].insert(*u_it);
	    }
	}

	// Make sure both vert sets store all the required vertices - the
	// polygons need to encompass all active verts mapped in by both
	// meshes, not just those from the overlapping triangles original to
	// that mesh.
	ov2.insert(nv);
	v2.insert(nv->p_id);
    }

    // If we have unmappable vertices, we're going to have to force refinement of them
    // somehow...
    for (ov_it = ovum.begin(); ov_it != ovum.end(); ov_it++) {
	overt_t *uv = *ov_it;
	ON_3dPoint uvpnt = uv->vpnt();
	uedge_t closest_edge = other_m->closest_uedge(uvpnt);
	if (other_m->fmesh->brep_edges.find(closest_edge) != other_m->fmesh->brep_edges.end()) {
	    bedge_seg_t *bseg = other_m->fmesh->ue2b_map[closest_edge];
	    if (!bseg) {
		std::cout << "couldn't find bseg pointer??\n";
	    } else {
		//std::cout << "Edge refinement point\n";
		(*edge_verts)[bseg].insert(uv);
	    }
	} else {
	    //std::cout << "Non edge refinement point\n";
	    std::set<size_t> uet = other_m->fmesh->uedges2tris[closest_edge];
	    std::set<size_t>::iterator u_it;
	    for (u_it = uet.begin(); u_it != uet.end(); u_it++) {
		other_m->ivert_ref_cnts[uv].insert(*u_it);
	    }
	}
    }
}

size_t
ovlp_grp::characterize_all_verts()
{
    characterize_verts(0);
    characterize_verts(1);
    
    return (om1_rverts_from_om2.size() + om2_rverts_from_om1.size() + om1_everts_from_om2.size() + om2_everts_from_om1.size());
}

bool
ovlp_grp::proj_tri_ovlp(omesh_t *om, size_t ind)
{
    std::vector<ovlp_proj_tri> &ptris = (om == om1) ? planar_core_tris2: planar_core_tris1;
    triangle_t tri = om->fmesh->tris_vect[ind];

    ovlp_proj_tri ttri;
    for (int i = 0; i < 3; i++) {
	double u, v;
	ON_3dPoint p3d = *om->fmesh->pnts[tri.v[i]];
	if (!ind) {
	    fp1.ClosestPointTo(p3d, &u, &v);
	} else {
	    fp2.ClosestPointTo(p3d, &u, &v);
	}
	VSET(ttri.pts[i], u, v, 0);
    }

    for (size_t i = 0; i < ptris.size(); i++) {
	if (ptris[i].ovlps(ttri)) {
	    return true;
	}
    }

    return false;
}

void
ovlp_grp::plot(const char *fname, int ind)
{

    omesh_t *om = (!ind) ? om1 : om2;
    std::set<size_t> tris = (!ind) ? tris1 : tris2;

    std::set<overt_t *>::iterator os_it;
    std::set<overt_t *> oum = (!ind) ? om1_unmappable_rverts : om2_unmappable_rverts;
    std::set<overt_t *> orm = (!ind) ? om1_rverts_from_om2 : om2_rverts_from_om1;
    std::set<overt_t *> oem = (!ind) ? om1_everts_from_om2 : om2_everts_from_om1;

    std::map<overt_t *, overt_t *>::iterator om_it;
    std::map<overt_t *, overt_t *> onormverts = (!ind) ? om1_om2_verts : om2_om1_verts;

    struct bu_color c = BU_COLOR_INIT_ZERO;
    unsigned char rgb[3] = {0,0,255};

    // Randomize the blue so (usually) subsequent
    // draws of different meshes won't erase the
    // previous wireframe
    std::random_device rd;
    std::mt19937 reng(rd());
    std::uniform_int_distribution<> rrange(100,250);
    int brand = rrange(reng);
    rgb[2] = (unsigned char)brand;

    FILE *plot = fopen(fname, "w");

    RTree<size_t, double, 3>::Iterator tree_it;
    size_t t_ind;
    triangle_t tri;

    bu_color_from_rgb_chars(&c, (const unsigned char *)rgb);

    om->fmesh->tris_tree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	t_ind = *tree_it;
	tri = om->fmesh->tris_vect[t_ind];
	om->fmesh->plot_tri(tri, &c, plot, 255, 0, 0);
	++tree_it;
    }

    double tri_r = 0;

    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot, &c);
    std::set<size_t>::iterator ts_it;
    for (ts_it = tris.begin(); ts_it != tris.end(); ts_it++) {
	tri = om->fmesh->tris_vect[*ts_it];
	double tr = om->fmesh->tri_pnt_r(tri.ind);
	tri_r = (tr > tri_r) ? tr : tri_r;
	om->fmesh->plot_tri(tri, &c, plot, 255, 0, 0);
    }

    pl_color(plot, 255, 0, 0);
    for (os_it = oum.begin(); os_it != oum.end(); os_it++) {
	overt_t *iv = *os_it;
	ON_3dPoint vp = iv->vpnt();
	plot_pnt_3d(plot, &vp, tri_r, 0);
    }

    pl_color(plot, 0, 255, 0);
    for (os_it = oem.begin(); os_it != oem.end(); os_it++) {
	overt_t *iv = *os_it;
	ON_3dPoint vp = iv->vpnt();
	plot_pnt_3d(plot, &vp, tri_r, 0);
    }

    pl_color(plot, 255, 255, 0);
    for (os_it = orm.begin(); os_it != orm.end(); os_it++) {
	overt_t *iv = *os_it;
	ON_3dPoint vp = iv->vpnt();
	plot_pnt_3d(plot, &vp, tri_r, 0);
    }

    pl_color(plot, 255, 255, 255);
    for (om_it = onormverts.begin(); om_it != onormverts.end(); om_it++) {
	overt_t *iv = om_it->first;
	ON_3dPoint vp = iv->vpnt();
	plot_pnt_3d(plot, &vp, tri_r, 0);
	iv = om_it->second;
	vp = iv->vpnt();
	plot_pnt_3d(plot, &vp, tri_r, 0);
    }

    fclose(plot);
}

void
ovlp_grp::plot(const char *fname)
{
    struct bu_vls omname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&omname, "%s-0.plot3", fname);
    plot(bu_vls_cstr(&omname), 0);
    bu_vls_sprintf(&omname, "%s-1.plot3", fname);
    plot(bu_vls_cstr(&omname), 1);
    bu_vls_free(&omname);
}

void
ovlp_grp::print()
{
    replaceable = true;
    replaceable = (om1_unmappable_rverts.size() > 0) ? false : replaceable;
    replaceable = (om2_unmappable_rverts.size() > 0) ? false : replaceable;
    replaceable = (om1_rverts_from_om2.size() > 0) ? false : replaceable;
    replaceable = (om2_rverts_from_om1.size() > 0) ? false : replaceable;
    replaceable = (om1_everts_from_om2.size() > 0) ? false : replaceable;
    replaceable = (om2_everts_from_om1.size() > 0) ? false : replaceable;

    std::set<overt_t *>::iterator os_it;
    std::cout << "om1_unmappable_rverts: " << om1_unmappable_rverts.size() << "\n";
    for (os_it = om1_unmappable_rverts.begin(); os_it != om1_unmappable_rverts.end(); os_it++) {
	std::cout << "                       "  << (*os_it)->vpnt().x << "," << (*os_it)->vpnt().y << "," << (*os_it)->vpnt().z << "\n";
    }
    std::cout << "om2_unmappable_rverts: " << om2_unmappable_rverts.size() << "\n";
    for (os_it = om2_unmappable_rverts.begin(); os_it != om2_unmappable_rverts.end(); os_it++) {
	std::cout << "                       "  << (*os_it)->vpnt().x << "," << (*os_it)->vpnt().y << "," << (*os_it)->vpnt().z << "\n";
    }
    std::cout << "om1_rverts_from_om2  : " << om1_rverts_from_om2.size()   << "\n";
    for (os_it = om1_rverts_from_om2.begin(); os_it != om1_rverts_from_om2.end(); os_it++) {
	std::cout << "                       "  << (*os_it)->vpnt().x << "," << (*os_it)->vpnt().y << "," << (*os_it)->vpnt().z << "\n";
    }
    std::cout << "om2_rverts_from_om1  : " << om2_rverts_from_om1.size()   << "\n";
    for (os_it = om2_rverts_from_om1.begin(); os_it != om2_rverts_from_om1.end(); os_it++) {
	std::cout << "                       "  << (*os_it)->vpnt().x << "," << (*os_it)->vpnt().y << "," << (*os_it)->vpnt().z << "\n";
    }
    std::cout << "om1_everts_from_om2  : " << om1_everts_from_om2.size()   << "\n";
    for (os_it = om1_everts_from_om2.begin(); os_it != om1_everts_from_om2.end(); os_it++) {
	std::cout << "                       "  << (*os_it)->vpnt().x << "," << (*os_it)->vpnt().y << "," << (*os_it)->vpnt().z << "\n";
    }
    std::cout << "om2_everts_from_om1  : " << om2_everts_from_om1.size()   << "\n";
    for (os_it = om2_everts_from_om1.begin(); os_it != om2_everts_from_om1.end(); os_it++) {
	std::cout << "                       "  << (*os_it)->vpnt().x << "," << (*os_it)->vpnt().y << "," << (*os_it)->vpnt().z << "\n";
    }

    std::map<overt_t *, overt_t *>::iterator om_it;
    std::cout << "Clear mappings om1 -> om2: \n";
    for (om_it = om1_om2_verts.begin(); om_it != om1_om2_verts.end(); om_it++) {
	std::cout << "                       "  << om_it->first->vpnt().x << "," << om_it->first->vpnt().y << "," << om_it->first->vpnt().z << " -> "  << om_it->second->vpnt().x << "," << om_it->second->vpnt().y << "," << om_it->second->vpnt().z << "\n";
    }
    std::cout << "Clear mappings om2 -> om1: \n";
    for (om_it = om2_om1_verts.begin(); om_it != om2_om1_verts.end(); om_it++) {
	std::cout << "                       "  << om_it->first->vpnt().x << "," << om_it->first->vpnt().y << "," << om_it->first->vpnt().z << " -> "  << om_it->second->vpnt().x << "," << om_it->second->vpnt().y << "," << om_it->second->vpnt().z << "\n";
    }

    std::cout << "group is replaceable: " << replaceable << "\n";
}

/* A grouping of overlapping triangles is defined as the set of triangles from
 * two meshes that overlap and are surrounded in both meshes by either
 * non-involved triangles or face boundary edges.  Visually, these are "clusters"
 * of overlapping triangles on the mesh surfaces. */
std::vector<ovlp_grp>
find_ovlp_grps(
	std::map<std::pair<omesh_t *, size_t>, size_t> &bin_map,
	std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs
	)
{
    std::vector<ovlp_grp> bins;

    bin_map.clear();

    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	omesh_t *omesh1 = cp_it->first->omesh;
	omesh_t *omesh2 = cp_it->second->omesh;
	if (!omesh1->intruding_tris.size() || !omesh2->intruding_tris.size()) {
	    continue;
	}
	//std::cout << "Need to check " << omesh1->fmesh->name << " " << omesh1->fmesh->f_id << " vs " << omesh2->fmesh->name << " " << omesh2->fmesh->f_id << "\n";
	std::set<size_t> om1_tris = omesh1->intruding_tris;
	while (om1_tris.size()) {
	    size_t t1 = *om1_tris.begin();
	    std::pair<omesh_t *, size_t> ckey(omesh1, t1);
	    om1_tris.erase(t1);
	    //std::cout << "Processing triangle " << t1 << "\n";
	    ON_BoundingBox tri_bb = omesh1->fmesh->tri_bbox(t1);
	    std::set<size_t> ntris = omesh2->tris_search(tri_bb);
	    std::set<size_t>::iterator nt_it;
	    for (nt_it = ntris.begin(); nt_it != ntris.end(); nt_it++) {
		int real_ovlp = tri_isect(omesh1->fmesh->tris_vect[t1], omesh2->fmesh->tris_vect[*nt_it], 0);
		omesh1->fmesh->tri_plot(omesh1->fmesh->tris_vect[t1], "ot1.plot3");
		omesh2->fmesh->tri_plot(omesh2->fmesh->tris_vect[*nt_it], "ot2.plot3");
		if (!real_ovlp) continue;
		//std::cout << "real overlap with " << *nt_it << "\n";

		std::pair<omesh_t *, size_t> nkey(omesh2, *nt_it);
		size_t key_id;
		if (bin_map.find(ckey) == bin_map.end() && bin_map.find(nkey) == bin_map.end()) {
		    // New group
		    ovlp_grp ngrp(ckey.first, nkey.first);
		    ngrp.add_tri(ckey.first, ckey.second);
		    ngrp.add_tri(nkey.first, nkey.second);
		    bins.push_back(ngrp);
		    key_id = bins.size() - 1;
		    bin_map[ckey] = key_id;
		    bin_map[nkey] = key_id;
		} else {
		    if (bin_map.find(ckey) != bin_map.end()) {
			key_id = bin_map[ckey];
			bins[key_id].add_tri(nkey.first, nkey.second);
			bin_map[nkey] = key_id;
		    } else {
			key_id = bin_map[nkey];
			bins[key_id].add_tri(ckey.first, ckey.second);
			bin_map[ckey] = key_id;
		    }
		}
	    }
	}
    }

    return bins;
}

#if 0
// Validate that all triangles in tris are still in the fmesh - if
// not, this is a stale group.
std::set<size_t>::iterator tr_it;
for (tr_it = tris.begin(); tr_it != tris.end(); tr_it++) {
    if (!om->fmesh->tri_active(*tr_it)) {
	return NULL;
    }
}
#endif



#if 0
void
find_interior_edge_grps(
	std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs
	)
{
    std::vector<ovlp_grp> bins;

    // Do a more aggressive overlap check that will catch triangles aligned
    // on the edges but still interior to the mesh
    int face_ov_cnt = omesh_ovlps(check_pairs, 1);
    int nfcnt = 0;


    while (nfcnt != face_ov_cnt) {
	std::cout << "face_ov_cnt: " << face_ov_cnt << ", nfcnt: " << nfcnt << "\n";
	face_ov_cnt = nfcnt;
	std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator cp_it;
	for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	    omesh_t *omesh1 = cp_it->first->omesh;
	    omesh_t *omesh2 = cp_it->second->omesh;
	    if (omesh1->fmesh->p_cdt == omesh2->fmesh->p_cdt) {
		// For now, don't tangle with self intersections
		continue;
	    }
	    if (!omesh1->intruding_tris.size() || !omesh2->intruding_tris.size()) {
		continue;
	    }

	    std::cout << "Need to check " << omesh1->fmesh->name << " " << omesh1->fmesh->f_id << " vs " << omesh2->fmesh->name << " " << omesh2->fmesh->f_id << "\n";
	    bool r1 = omesh_repair(omesh1, omesh1->intruding_tris);
	    bool r2 = omesh_repair(omesh2, omesh2->intruding_tris);
	    if (!r1 || !r2) {
		std::cout << "edge aligned repair failed\n";
	    }
	}

	// Do a more aggressive overlap check that will catch triangles aligned
	// on the edges but still interior to the mesh
	nfcnt = omesh_ovlps(check_pairs, 1);
    }

    std::cout << "done with edge aligned\n";
}
#endif

void
shared_cdts(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs)
{
    std::vector<ovlp_grp> bins;
    std::map<std::pair<omesh_t *, size_t>, size_t> bin_map;
    std::set<size_t> aligned, non_aligned;
    std::set<size_t>::iterator r_it;

    std::map<bedge_seg_t *, std::set<overt_t *>> edge_verts;

    size_t bins_pcnt = 0;
    bool processed_all_bins = false;

    while (!processed_all_bins || (bins.size() && bins_pcnt < 3)) {
	size_t refinement_cnt = INT_MAX;
	bins_pcnt++;
	while (refinement_cnt) {
	    bins.clear();
	    aligned.clear();
	    non_aligned.clear();
	    edge_verts.clear();
	    bin_map.clear();

	    bins = find_ovlp_grps(bin_map, check_pairs);

	    if (!bins.size()) {
		// If we didn't find anything, we're done
		return;
	    }

	    // Have groupings - reset refinement info
	    size_t refinement_cnt_prev = (refinement_cnt < INT_MAX) ? refinement_cnt : 0;
	    refinement_cnt = 0;
	    //refinement_reset(check_pairs);
	    for (size_t i = 0; i < bins.size(); i++) {
		bins[i].edge_verts = &edge_verts;
		size_t bcnt = bins[i].characterize_all_verts();
		if (bcnt) {
		    non_aligned.insert(i);
		    bins[i].print();
		} else {
		    aligned.insert(i);
		}
		refinement_cnt += bcnt;
	    }

	    // TODO - before attempting localized cdt, we need to perform
	    // any necessary refinements so we have the proper points in place.
	    if (refinement_cnt > refinement_cnt_prev) {
		std::set<omesh_t *> a_omesh = refinement_omeshes(check_pairs);
		std::set<omesh_t *>::iterator a_it;

		for (a_it = a_omesh.begin(); a_it != a_omesh.end(); a_it++) {
		    omesh_t *am = *a_it;
		    std::map<bedge_seg_t *, std::set<overt_t *>>::iterator es_it;
		    for (es_it = edge_verts.begin(); es_it != edge_verts.end(); es_it++) {
			bedge_seg_t *eseg = es_it->first;
			std::vector<std::pair<cdt_mesh_t *,uedge_t>> uedges = eseg->uedges();
			int f_id1 = uedges[0].first->f_id;
			int f_id2 = uedges[1].first->f_id;
			if (f_id1 == am->fmesh->f_id || f_id2 == am->fmesh->f_id) {
			    // If this bedge_seg_t is topologically connected to the face, then the
			    // verts associated with that bedge are not interior overts and
			    // will be handled by the edge refinement.
			    std::set<overt_t *>::iterator e_it;
			    for (e_it = es_it->second.begin(); e_it != es_it->second.end(); e_it++) {
				//am->near_verts.erase(*e_it);
			    }
			}
		    }
		}

		std::set<overt_t *> nverts;
		bedge_split_near_verts(&nverts, edge_verts);
		omesh_interior_edge_verts(check_pairs);
		omesh_ovlps(check_pairs, 0);
		omesh_refinement_pnts(check_pairs, 0);
		refinement_cnt_prev = refinement_cnt;
	    } else {
		refinement_cnt = 0;
	    }
	}

#if 1
	for (r_it = aligned.begin(); r_it != aligned.end(); r_it++) {
	    std::cout << "Group " << *r_it << " aligned:\n";
	    bins[*r_it].list_tris();
	    bins[*r_it].list_overts();
	    struct bu_vls pname = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&pname, "group_aligned_%zu", *r_it);
	    bins[*r_it].plot(bu_vls_cstr(&pname));
	    bu_vls_free(&pname);
	}
	for (r_it = non_aligned.begin(); r_it != non_aligned.end(); r_it++) {
	    std::cout << "Group " << *r_it << " not fully aligned:\n";
	    bins[*r_it].list_tris();
	    bins[*r_it].list_overts();
	    struct bu_vls pname = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&pname, "group_nonaligned_%zu", *r_it);
	    bins[*r_it].plot(bu_vls_cstr(&pname));
	    bu_vls_free(&pname);
	}
    }
#endif
#if 0
	processed_all_bins = true;
	for (size_t i = 0; i < bins.size(); i++) {

	    bool v1 = bins[i].om1->fmesh->valid(1);
	    bool v2 = bins[i].om2->fmesh->valid(1);

	    if (!v1 || !v2) {
		std::cout << "Starting bin processing with invalid inputs!\n";
	    }

	    // TODO - if all vertices are mapped, may want to CDT only one of
	    // the polygons in the shared plane and map back to matching
	    // closest points in both meshes - not clear if this is necessary
	    // (CDTs ought to produce the same answer for both polygons, but
	    // might be better not to rely on that...)
	    if (bins[i].om1->fmesh->f_id == 869 || bins[i].om2->fmesh->f_id == 869) {
		std::cout << "problem\n";
	    }

	    cpolygon_t *polygon1 = group_polygon(bins[i], 0);
	    cpolygon_t *polygon2 = group_polygon(bins[i], 1);
	    if (!polygon1 || !polygon2) {
		// TODO - there is a potentially legitimate case for no polygon returned
		// here - if two groups are formed by different mesh pairs, but one of
		// them processes out a triangle that is in another group, that group
		// is now invalid and must be skipped.
		std::cerr << "group polygon generation failed!\n";
		processed_all_bins = false;
		continue;
	    }

	    polygon1->cdt();
	    polygon1->polygon_plot_in_plane("poly1.plot3");
	    bins[i].om1->fmesh->tris_plot("before1.plot3");
	    // Replace grp triangles with new polygon triangles
	    std::set<size_t>::iterator t_it;
	    for (t_it = bins[i].vtris1.begin(); t_it != bins[i].vtris1.end(); t_it++) {
		bins[i].om1->fmesh->tri_remove(bins[i].om1->fmesh->tris_vect[*t_it]);
	    }
	    std::set<triangle_t>::iterator tr_it;
	    for (tr_it = polygon1->tris.begin(); tr_it != polygon1->tris.end(); tr_it++) {
		triangle_t tri = *tr_it;
		orient_tri(*bins[i].om1->fmesh, tri);
		bins[i].om1->fmesh->tri_add(tri);
	    }

#if 1
	    //bins[i].om1->fmesh->tris_plot("after1.plot3");
	    v1 = bins[i].om1->fmesh->valid(1);
	    v2 = bins[i].om2->fmesh->valid(1);

	    if (!v1 || !v2) {
		std::cout << "Ending polygon1 processing with invalid inputs!\n";
	    }
#endif

	    polygon2->cdt();
	    polygon1->polygon_plot_in_plane("poly1.plot3");
	    bins[i].om2->fmesh->tris_plot("before2.plot3");

	    // Replace grp triangles with new polygon triangles
	    for (t_it = bins[i].vtris2.begin(); t_it != bins[i].vtris2.end(); t_it++) {
		bins[i].om2->fmesh->tri_remove(bins[i].om2->fmesh->tris_vect[*t_it]);
	    }
	    for (tr_it = polygon2->tris.begin(); tr_it != polygon2->tris.end(); tr_it++) {
		triangle_t tri = *tr_it;
		orient_tri(*bins[i].om2->fmesh, tri);
		bins[i].om2->fmesh->tri_add(tri);
	    }

#if 1
	    v1 = bins[i].om1->fmesh->valid(1);
	    v2 = bins[i].om2->fmesh->valid(1);

	    if (!v1 || !v2) {
		std::cout << "Ending polygon2 processing with invalid inputs!\n";
	    }
	    bins[i].om2->fmesh->tris_plot("after2.plot3");
	    std::cout << "cdts complete\n";
#endif
	}

	omesh_ovlps(check_pairs, 1);
	omesh_refinement_pnts(check_pairs, 0);

    }


    if (bins.size() && bins_pcnt == 3) {
	bins.clear();
	aligned.clear();
	non_aligned.clear();
	edge_verts.clear();
	bin_map.clear();

	bins = find_ovlp_grps(bin_map, check_pairs);

	if (!bins.size()) {
	    // If we didn't find anything, we're done
	    return;
	}

	for (size_t i = 0; i < bins.size(); i++) {

#if 1
	    bool v1 = bins[i].om1->fmesh->valid(1);
	    bool v2 = bins[i].om2->fmesh->valid(1);

	    if (!v1 || !v2) {
		std::cout << "Starting bin processing with invalid inputs!\n";
	    }
#endif

	    bool r1 = group_repair(bins[i], 0);
	    bool r2 = group_repair(bins[i], 1);
	    if (!r1 || !r2) {
		std::cerr << "group repair failed!\n";
		continue;
	    }

#if 1
	    v1 = bins[i].om1->fmesh->valid(1);
	    v2 = bins[i].om2->fmesh->valid(1);

	    if (!v1 || !v2) {
		std::cout << "Repair produced invalid outputs!\n";
	    }
#endif

	}
    }

    // After the above processing, we may still have individual triangle pairs
    // that intrude with all edges aligned relative to the other mesh (in
    // essence, these are shared polygons with different interior tessellations
    // in each mesh.)  To align them, we identify the intruding edges and
    // construct the simple polygons associated with those edges in both meshes
    // for retessellation.  Assuming consistent CDT behavior, the outputs
    // should align and clear the overlap.  (If necessary we can map the
    // triangles from one edge into the other mesh (in principle we could also
    // do that with the above case, although there the CDT result is likely to
    // be more reliably simple/clean than the existing triangles...) which
    // would be less work CPU wise, but the current logic leverages already
    // written code so it's easier as an immediate implementation path.)
    find_interior_edge_grps(check_pairs);
#endif
}


/** @} */

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

