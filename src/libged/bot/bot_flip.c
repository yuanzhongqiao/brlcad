/*                         B O T _ F L I P . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file libged/bot_flip.c
 *
 * The bot_flip command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/path.h"
#include "rt/geom.h"

#include "../ged_private.h"


int
ged_bot_flip_core(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    static const char *usage = "bot [bot2 bot3 ...]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    for (i = 1; i < argc; ++i) {
	/* Skip past any path elements */
	char *obj = (char *)bu_calloc(strlen(argv[i]), sizeof(char), "ged_bot_flip obj");
	bu_path_basename(argv[i], obj);

	if (BU_STR_EQUAL(obj, ".")) {
	    /* malformed path, lookup using exactly what was provided */
	    bu_free(obj, "free bu_path_basename");
	    obj = bu_strdup(argv[i]);
	}

	if ((dp = db_lookup(gedp->dbip, obj, LOOKUP_QUIET)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s: db_lookup(%s) error\n", argv[0], obj);
	} else {
	    GED_DB_GET_INTERNAL(gedp, &intern, dp, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);

	    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
		bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT solid!\n", argv[0], obj);
	    } else {
		bot = (struct rt_bot_internal *)intern.idb_ptr;
		rt_bot_flip(bot);

		GED_DB_PUT_INTERNAL(gedp, dp, &intern, gedp->dbip->db_resp, BRLCAD_ERROR);
	    }
	}
	bu_free(obj, "free obj");
    }

    return BRLCAD_OK;
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
