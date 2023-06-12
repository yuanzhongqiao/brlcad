/*                        C U R V E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2023 United States Government as represented by
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
/** @file libged/brep/curve.cpp
 *
 * NURBS curves editing support for brep objects
 *
 */

#include "common.h"

#include <set>

#include "opennurbs.h"
#include "wdb.h"
#include "bu/cmd.h"
#include "../ged_private.h"
#include "./ged_brep.h"

using namespace brlcad;

struct _ged_brep_icurve {
    struct _ged_brep_info *gb;
    struct bu_vls *vls;
    const struct bu_cmdtab *cmds;
};


static int
_brep_curve_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_brep_icurve *gb = (struct _ged_brep_icurve *)bs;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gb->vls, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gb->vls, "%s\n", ps);
	return 1;
    }
    return 0;
}

extern "C" int
_brep_cmd_curve_create(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> curve create <x> <y> <z>";
    const char *purpose_string = "create a new NURBS curve";
    if (_brep_curve_msgs(bs, argc, argv, usage_string, purpose_string))
    {
        return BRLCAD_OK;
    }

    struct _ged_brep_icurve *gib = (struct _ged_brep_icurve *)bs;
    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;

    argc--;argv++;
    // Create a template nurbs curve
    ON_NurbsCurve *curve = brep_make_curve(argc, argv);

    b_ip->brep->AddEdgeCurve(curve);

    // Delete the old object
    const char *av[3];
    char *ncpy = bu_strdup(gib->gb->solid_name.c_str());
    av[0] = "kill";
    av[1] = ncpy;
    av[2] = NULL;
    (void)ged_exec(gib->gb->gedp, 2, (const char **)av);
    bu_free(ncpy, "free name cpy");

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep))
    {
        return BRLCAD_ERROR;
    }
    bu_vls_printf(gib->gb->gedp->ged_result_str, "create C3 curve! id = %d", b_ip->brep->m_C3.Count() - 1);
    return BRLCAD_OK;
}

extern "C" int
_brep_cmd_curve_in(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> curve in <is_rational> <order> <cv_count> <cv1_x> <cv1_y> <cv1_z> <cv_w>(if rational) ...";
    const char *purpose_string = "create a new NURBS curve given detailed description";
    if (_brep_curve_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_icurve *gib = (struct _ged_brep_icurve *)bs;
    if (argc < 4)
    {
    bu_vls_printf(gib->gb->gedp->ged_result_str, " not enough arguments\n");
    bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
    return BRLCAD_ERROR;
    }

    bool is_rational = atoi(argv[1]);
    int order = atoi(argv[2]);
    int cv_count = atoi(argv[3]);

    if(order <= 0 || cv_count <= 0 || cv_count < order)
    {
    bu_vls_printf(gib->gb->gedp->ged_result_str, " invalid order or cv_count\n");
    return BRLCAD_ERROR;
    }

    if (argc < 4 + cv_count * (3 + (is_rational ? 1 : 0)))
    {
    bu_vls_printf(gib->gb->gedp->ged_result_str, " not enough arguments, you need to input %d more args about control vertices\n", 4 + cv_count * (3 + (is_rational ? 1 : 0)) - argc);
    bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
    return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;

    argc--;argv++;
    ON_NurbsCurve *curve = brep_in_curve(argc, argv);
    if(curve == NULL)
    {
        bu_vls_printf(gib->gb->gedp->ged_result_str, " failed to create curve\n");
        return BRLCAD_ERROR;
    }

    // add the curve to the brep
    b_ip->brep->AddEdgeCurve(curve);

    // Delete the old object
    const char *av[3];
    char *ncpy = bu_strdup(gib->gb->solid_name.c_str());
    av[0] = "kill";
    av[1] = ncpy;
    av[2] = NULL;
    (void)ged_exec(gib->gb->gedp, 2, (const char **)av);
    bu_free(ncpy, "free name cpy");

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

extern "C" int
_brep_cmd_curve_move_cv(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> curve move_cv <curve_id> <CV_id> <x> <y> <z> [<w>]";
    const char *purpose_string = "move the control vertex of a NURBS curve";
    if (_brep_curve_msgs(bs, argc, argv, usage_string, purpose_string))
    {
    return BRLCAD_OK;
    }

    struct _ged_brep_icurve *gib = (struct _ged_brep_icurve *)bs;

    if (argc < 6)
    {
    bu_vls_printf(gib->gb->gedp->ged_result_str, " not enough arguments\n");
    bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
    return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;

    ON_4dPoint p = ON_4dPoint(atof(argv[3]), atof(argv[4]), atof(argv[5]), argc == 7 ? atof(argv[6]) : 1.0);
    bool flag = brep_curve_move_cv(b_ip->brep, atoi(argv[1]), atoi(argv[2]), p);
    if (!flag)
    {
    bu_vls_printf(gib->gb->gedp->ged_result_str, ": failed to move control vertex %s of curve %s\n", argv[2], argv[1]);
    return BRLCAD_ERROR;
    }

    // Delete the old object
    const char *av[3];
    char *ncpy = bu_strdup(gib->gb->solid_name.c_str());
    av[0] = "kill";
    av[1] = ncpy;
    av[2] = NULL;
    (void)ged_exec(gib->gb->gedp, 2, (const char **)av);
    bu_free(ncpy, "free name cpy");

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep))
    {
    return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}


extern "C" int
_brep_cmd_curve_flip(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> curve flip <curve_id>";
    const char *purpose_string = "Flip the direction of a NURBS curve";
    if (_brep_curve_msgs(bs, argc, argv, usage_string, purpose_string))
    {
    return BRLCAD_OK;
    }

    struct _ged_brep_icurve *gib = (struct _ged_brep_icurve *)bs;
    if(argc < 2)
    {
    bu_vls_printf(gib->gb->gedp->ged_result_str, " not enough arguments\n");
    bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
    return BRLCAD_ERROR;
    }

    int curve_id = 0;
    try
    {
    curve_id = std::stoi(argv[1]);
    }
    catch(const std::exception& e)
    {
    bu_vls_printf(gib->gb->gedp->ged_result_str, " invalid curve id\n");
    bu_vls_printf(gib->gb->gedp->ged_result_str, " %s\n", e.what());
    return BRLCAD_ERROR;
    }
    
    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;
    
    bool flag = brep_curve_reverse(b_ip->brep, curve_id);
    if (!flag)
    {
    bu_vls_printf(gib->gb->gedp->ged_result_str, ": failed to reverse curve %s\n", argv[1]);
    return BRLCAD_ERROR;
    }

    // Delete the old object
    const char *av[3];
    char *ncpy = bu_strdup(gib->gb->solid_name.c_str());
    av[0] = "kill";
    av[1] = ncpy;
    av[2] = NULL;
    (void)ged_exec(gib->gb->gedp, 2, (const char **)av);
    bu_free(ncpy, "free name cpy");

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep))
    {
    return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}


extern "C" int
_brep_cmd_curve_insert_knot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> insert_knot <curve_id> <knot_value> <multiplicity>";
    const char *purpose_string = "Insert a knot into a NURBS curve";
    if (_brep_curve_msgs(bs, argc, argv, usage_string, purpose_string))
    {
    return BRLCAD_OK;
    }
    argc--;argv++;
    struct _ged_brep_icurve *gib = (struct _ged_brep_icurve *)bs;
    if(argc < 3)
    {
    bu_vls_printf(gib->gb->gedp->ged_result_str, " not enough arguments\n");
    bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
    return BRLCAD_ERROR;
    }

    int curve_id = 0;
    try
    {
    curve_id = std::stoi(argv[0]);
    }
    catch(const std::exception& e)
    {
    bu_vls_printf(gib->gb->gedp->ged_result_str, " invalid curve id\n");
    bu_vls_printf(gib->gb->gedp->ged_result_str, " %s\n", e.what());
    return BRLCAD_ERROR;
    }

    double knot_value = 0;
    try
    {
    knot_value = std::stod(argv[1]);
    }
    catch(const std::exception& e)
    {
    bu_vls_printf(gib->gb->gedp->ged_result_str, " invalid knot value\n");
    bu_vls_printf(gib->gb->gedp->ged_result_str, " %s\n", e.what());
    return BRLCAD_ERROR;
    }

    int multiplicity = 0;
    try
    {
    multiplicity = std::stoi(argv[2]);
    }
    catch(const std::exception& e)
    {
    bu_vls_printf(gib->gb->gedp->ged_result_str, " invalid multiplicity\n");
    bu_vls_printf(gib->gb->gedp->ged_result_str, " %s\n", e.what());
    return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;
    bool flag = brep_curve_insert_knot(b_ip->brep, curve_id, knot_value, multiplicity);
    if (!flag)
    {
    bu_vls_printf(gib->gb->gedp->ged_result_str, ": failed to insert knot into curve %s\n", argv[0]);
    return BRLCAD_ERROR;
    }

    // Delete the old object

    const char *av[3];
    char *ncpy = bu_strdup(gib->gb->solid_name.c_str());
    av[0] = "kill";
    av[1] = ncpy;
    av[2] = NULL;
    (void)ged_exec(gib->gb->gedp, 2, (const char **)av);
    bu_free(ncpy, "free name cpy");

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep))
    {
    return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}
extern "C" int
_brep_cmd_curve_trim(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> trim <curve_id> <start_param> <end_param>";
    const char *purpose_string = "trim a NURBS curve using start and end parameters";
    if (_brep_curve_msgs(bs, argc, argv, usage_string, purpose_string))
    {
    return BRLCAD_OK;
    }
    argc--;argv++;
    struct _ged_brep_icurve *gib = (struct _ged_brep_icurve *)bs;
    if(argc < 3)
    {
    bu_vls_printf(gib->gb->gedp->ged_result_str, " not enough arguments\n");
    bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
    return BRLCAD_ERROR;
    }

    int curve_id = 0;
    try
    {
    curve_id = std::stoi(argv[0]);
    }
    catch(const std::exception& e)
    {
    bu_vls_printf(gib->gb->gedp->ged_result_str, " invalid curve id\n");
    bu_vls_printf(gib->gb->gedp->ged_result_str, " %s\n", e.what());
    return BRLCAD_ERROR;
    }
    
    double start_param = 0;
    try
    {
    start_param = std::stod(argv[1]);
    }
    catch(const std::exception& e)
    {
    bu_vls_printf(gib->gb->gedp->ged_result_str, " invalid start parameter\n");
    bu_vls_printf(gib->gb->gedp->ged_result_str, " %s\n", e.what());
    return BRLCAD_ERROR;
    }

    double end_param = 0;
    try
    {
    end_param = std::stod(argv[2]);
    }
    catch(const std::exception& e)
    {
    bu_vls_printf(gib->gb->gedp->ged_result_str, " invalid end parameter\n");
    bu_vls_printf(gib->gb->gedp->ged_result_str, " %s\n", e.what());
    return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;
    bool flag = brep_curve_trim(b_ip->brep, curve_id, start_param, end_param);
    if (!flag)
    {
    bu_vls_printf(gib->gb->gedp->ged_result_str, ": failed to trim curve %s\n", argv[0]);
    return BRLCAD_ERROR;
    }

    // Delete the old object
    const char *av[3];
    char *ncpy = bu_strdup(gib->gb->solid_name.c_str());
    av[0] = "kill";
    av[1] = ncpy;
    av[2] = NULL;
    (void)ged_exec(gib->gb->gedp, 2, (const char **)av);
    bu_free(ncpy, "free name cpy");

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep))
    {
    return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static void
_brep_curve_help(struct _ged_brep_icurve *bs, int argc, const char **argv)
{
    struct _ged_brep_icurve *gb = (struct _ged_brep_icurve *)bs;
    if (!argc || !argv)
    {
    bu_vls_printf(gb->vls, "brep [options] <objname> curve <subcommand> [args]\n");
    bu_vls_printf(gb->vls, "Available subcommands:\n");
    const struct bu_cmdtab *ctp = NULL;
    int ret;
    const char *helpflag[2];
    helpflag[1] = PURPOSEFLAG;
    for (ctp = gb->cmds; ctp->ct_name != (char *)NULL; ctp++)
    {
    bu_vls_printf(gb->vls, "  %s\t\t", ctp->ct_name);
    helpflag[0] = ctp->ct_name;
    bu_cmd(gb->cmds, 2, helpflag, 0, (void *)gb, &ret);
    }
    }
    else
    {
    int ret;
    const char *helpflag[2];
    helpflag[0] = argv[0];
    helpflag[1] = HELPFLAG;
    bu_cmd(gb->cmds, 2, helpflag, 0, (void *)gb, &ret);
    }
}

const struct bu_cmdtab _brep_curve_cmds[] = {
    {"create", _brep_cmd_curve_create},
    {"in", _brep_cmd_curve_in},
    {"move_cv", _brep_cmd_curve_move_cv},
    {"flip", _brep_cmd_curve_flip},
    {"insert_knot", _brep_cmd_curve_insert_knot},
    {"trim", _brep_cmd_curve_trim},
    {(char *)NULL, NULL}
};

int
brep_curve(struct _ged_brep_info *gb, int argc, const char **argv)
{
    struct _ged_brep_icurve gib;
    gib.gb = gb;
    gib.vls = gb->gedp->ged_result_str;
    gib.cmds = _brep_curve_cmds;

    const ON_Brep *brep = ((struct rt_brep_internal *)(gb->intern.idb_ptr))->brep;
    if (brep == NULL)
    {
    bu_vls_printf(gib.vls, "not a brep object\n");
    return BRLCAD_ERROR;
    }

    if (!argc)
    {
    _brep_curve_help(&gib, 0, NULL);
    return BRLCAD_OK;
    }

    // TODO: add help flag
    if (argc > 1 && BU_STR_EQUAL(argv[1], HELPFLAG))
    {
    argc--;argv++;
    argc--;argv++;
    _brep_curve_help(&gib, argc, argv);
    return BRLCAD_OK;
    }

    // Must have valid subcommand to process
    if (bu_cmd_valid(_brep_curve_cmds, argv[0]) != BRLCAD_OK) {
	bu_vls_printf(gib.vls, "invalid subcommand \"%s\" specified\n", argv[0]);
	_brep_curve_help(&gib, 0, NULL);
	return BRLCAD_ERROR;
    }

    int ret;
    if (bu_cmd(_brep_curve_cmds, argc, argv, 0, (void *)&gib, &ret) == BRLCAD_OK) {
	return ret;
    }
    return BRLCAD_ERROR;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
