/*                           E N V . C
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
/** @file libged/env.c
 *
 * The env command.
 *
 * Interface for discovering and managing environment variables relevant
 * to the BRL-CAD libraries and programs.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/env.h"
#include "bu/path.h"
#include "../ged_private.h"

/* Generated by the env2c command in misc/tools at build time */
#include "./env_cmd.c"


extern int env_cmd(struct bu_vls *s_out, int argc, const char **argv);


/**
 * Reports on and manipulates environment variables relevant to BRL-CAD.
 */
int
ged_env_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret = BRLCAD_OK;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    ret = env_cmd(gedp->ged_result_str, argc, argv);

    if (ret == 2) {
	return GED_HELP;
    }

    ret = (!ret) ? BRLCAD_OK : BRLCAD_ERROR;

    return ret;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl env_cmd_impl = {
    "env",
    ged_env_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd env_pcmd = { &env_cmd_impl };
const struct ged_cmd *env_cmds[] = { &env_pcmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  env_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
