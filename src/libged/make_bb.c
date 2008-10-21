/*                         M A K E _ B B . C
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
/** @file make_bb.c
 *
 * The make_bb command.
 *
 */

#include "common.h"

#include <string.h>

#include "bio.h"
#include "cmd.h"
#include "rtgeom.h"
#include "ged_private.h"

int
ged_make_bb(struct ged *gedp, int argc, const char *argv[])
{
    register int		i;
    point_t			rpp_min, rpp_max;
    struct directory	*dp;
    struct rt_arb_internal	*arb;
    struct rt_db_internal	new_intern;
    char			*new_name;
    int			use_air = 0;
    static const char *usage = "bbname object(s)";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc < 3 || MAXARGS < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    i = 1;

    /* look for a USEAIR option */
    if ( ! strcmp(argv[i], "-u") ) {
	use_air = 1;
	i++;
    }

    /* Since arguments may be paths, make sure first argument isn't */
    if (strchr(argv[i], '/')) {
	bu_vls_printf(&gedp->ged_result_str, "Do not use '/' in solid names: %s\n", argv[i]);
	return BRLCAD_ERROR;
    }

    new_name = (char *)argv[i++];
    if (db_lookup(gedp->ged_wdbp->dbip, new_name, LOOKUP_QUIET) != DIR_NULL) {
	bu_vls_printf(&gedp->ged_result_str, "%s already exists\n", new_name);
	return BRLCAD_ERROR;
    }

    if (ged_get_obj_bounds(gedp, argc-2, (const char **)argv+2, use_air, rpp_min, rpp_max) == BRLCAD_ERROR)
	return BRLCAD_ERROR;

    /* build bounding RPP */
    arb = (struct rt_arb_internal *)bu_malloc(sizeof(struct rt_arb_internal), "arb");
    VMOVE(arb->pt[0], rpp_min);
    VSET(arb->pt[1], rpp_min[X], rpp_min[Y], rpp_max[Z]);
    VSET(arb->pt[2], rpp_min[X], rpp_max[Y], rpp_max[Z]);
    VSET(arb->pt[3], rpp_min[X], rpp_max[Y], rpp_min[Z]);
    VSET(arb->pt[4], rpp_max[X], rpp_min[Y], rpp_min[Z]);
    VSET(arb->pt[5], rpp_max[X], rpp_min[Y], rpp_max[Z]);
    VMOVE(arb->pt[6], rpp_max);
    VSET(arb->pt[7], rpp_max[X], rpp_max[Y], rpp_min[Z]);
    arb->magic = RT_ARB_INTERNAL_MAGIC;

    /* set up internal structure */
    RT_INIT_DB_INTERNAL(&new_intern);
    new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    new_intern.idb_type = ID_ARB8;
    new_intern.idb_meth = &rt_functab[ID_ARB8];
    new_intern.idb_ptr = (genptr_t)arb;

    if ((dp=db_diradd(gedp->ged_wdbp->dbip, new_name, -1L, 0, DIR_SOLID, (genptr_t)&new_intern.idb_type)) == DIR_NULL) {
	bu_vls_printf(&gedp->ged_result_str, "Cannot add %s to directory\n", new_name);
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &new_intern, gedp->ged_wdbp->wdb_resp) < 0) {
	rt_db_free_internal(&new_intern, gedp->ged_wdbp->wdb_resp);
	bu_vls_printf(&gedp->ged_result_str, "Database write error, aborting.\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
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
