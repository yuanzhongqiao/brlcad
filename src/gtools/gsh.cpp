/*                           G S H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file gtools/gsh.cpp
 *
 * This program (the BRL-CAD Geometry SHell) is a low level way to
 * interact with BRL-CAD geometry (.g) files.
 *
 * TODO - right now this program can't handle async output from commands like
 * rtcheck.  We need something similar to (but more cross platform than)
 * https://github.com/antirez/linenoise/pull/95/ but that's not enough by
 * itself - we need to know when input is arriving from the subprocess and
 * a mechanism to trigger the printing action..
 *
 * In Qt we're using QSocketNotifier + signals and slots to make this work with
 * a single text widget, which is similar in some ways to what needs to happen
 * here.  (Indeed, it's most likely essential - in Qt we could use multiple
 * text widgets, but that's not really an option for gsh.)  We don't want to
 * pull in Qt as a gsh dependency... could we make use of
 * https://github.com/fr00b0/nod here?  Perhaps trying timed I/O reading from a
 * thread and then sending back any results to linenoise via signals, and then
 * have linenoise do something similar to the 95 logic to get it on the screen?
 * nod (and a number of other libs) do stand-alone signals and slots, but so
 * far I've not found a cross-platform wrapper equivalent to QSocketNotifier...
 *
 * libuv may be worth investigating - I think what we need to do is a subset of
 * libuv's capabilities, at least at a glance.  It does also have a CMake build
 * and Windows is supported, is liberally licensed, and it looks like it isn't
 * a huge dependency:
 * https://github.com/libuv/libuv
 *
 */

#include "common.h"

#include "bio.h"
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "linenoise.h"
}

#include "bu.h"
#include "bv.h"
#define DM_WITH_RT
#include "dm.h"
#include "ged.h"

#define DEFAULT_GSH_PROMPT "g> "

struct gsh_state {
    void *libged;
    struct ged *gedp;
    struct bview *view;
    const char *pmpt;
    struct bu_vls gfile;

    /* linenoise */
    char *line;
    struct bu_vls iline;

    /* Hash values for state tracking in the interactive loop */
    unsigned long long prev_dhash = 0;
    unsigned long long prev_vhash = 0;
    unsigned long long prev_lhash = 0;
    unsigned long long prev_ghash = 0;
};

int
gsh_state_init(struct gsh_state *s)
{
    s->libged = bu_dlopen(NULL, BU_RTLD_LAZY);

    if (!s->libged)
	return BRLCAD_ERROR;

    s->gedp = GED_NULL;
    s->pmpt = NULL;
    s->line = NULL;
    bu_vls_init(&s->gfile);
    bu_vls_init(&s->iline);

    BU_GET(s->view, struct bview);
    bv_init(s->view);
    bu_vls_sprintf(&s->view->gv_name, "default");

    return BRLCAD_OK;
}
void
gsh_state_free(struct gsh_state *s)
{
    if (!s)
	return;

    bv_free(s->view);
    BU_PUT(s->view, struct bv);
    ged_close(s->gedp);
    bu_dlclose(s->libged);
    bu_vls_free(&s->gfile);
    bu_vls_free(&s->iline);
}

/* Handle the non-interactive execution of commands here. */
int
geval(struct gsh_state *s, int argc, const char **argv)
{
    int (*func)(struct ged *, int, char *[]);
    char gedfunc[MAXPATHLEN] = {0};

    bu_strlcat(gedfunc, "ged_", MAXPATHLEN);
    bu_strlcat(gedfunc, argv[0], MAXPATHLEN - strlen(gedfunc));
    if (strlen(gedfunc) < 1 || gedfunc[0] != 'g') {
	bu_log("Couldn't get GED command name from [%s]\n", argv[0]);
	return -1;
    }

    /* Note - doing the command execution this way limits us to the "hardwired"
     * functions built into the libged API.  That may or may not be what we
     * want, depending on the application - this in effect restricts us to a
     * "safe" command set, but also hides any user-provided plugins.  If we
     * want to expose the latter we should be using the "ged_exec" approach. */
    *(void **)(&func) = bu_dlsym(s->libged, gedfunc);
    if (!func) {
	bu_log("Unrecognized command [%s]\n", argv[0]);
	return -1;
    }

    /* TODO - is it ever unsafe to do this cast?  Does ged mess with argv somehow? */
    int ret = func(s->gedp, argc, (char **)argv);

    fprintf(stdout, "%s", bu_vls_cstr(s->gedp->ged_result_str));

    return ret;
}

void
view_update(struct gsh_state *s)
{
    if (!s || !s->gedp || !s->gedp->ged_dmp)
	return;

    struct ged *gedp = s->gedp;
    struct dm *dmp = (struct dm *)gedp->ged_dmp;
    struct bview *v = gedp->ged_gvp;
    unsigned long long dhash = dm_hash(dmp);
    unsigned long long vhash = bv_hash(gedp->ged_gvp);
    unsigned long long lhash = dl_name_hash(gedp);
    unsigned long long ghash = ged_dl_hash((struct display_list *)gedp->ged_gdp->gd_headDisplay);
    unsigned long long lhash_edit = lhash;
    if (dhash != s->prev_dhash) {
	dm_set_dirty(dmp, 1);
    }
    if (vhash != s->prev_vhash) {
	dm_set_dirty(dmp, 1);
    }
    if (lhash_edit != s->prev_lhash) {
	dm_set_dirty(dmp, 1);
    }
    if (ghash != s->prev_ghash) {
	dm_set_dirty(dmp, 1);
    }
    if (dm_get_dirty(dmp)) {
	matp_t mat = gedp->ged_gvp->gv_model2view;
	dm_loadmatrix(dmp, mat, 0);
	unsigned char geometry_default_color[] = { 255, 0, 0 };
	dm_draw_begin(dmp);
	dm_draw_display_list(dmp, gedp->ged_gdp->gd_headDisplay,
		1.0, gedp->ged_gvp->gv_isize, -1, -1, -1, 1,
		0, 0, geometry_default_color, 1, 0);

	// Faceplate drawing
	dm_draw_viewobjs(gedp->ged_wdbp, v, NULL, gedp->dbip->dbi_base2local, gedp->dbip->dbi_local2base);

	dm_draw_end(dmp);
    }
}

int
main(int argc, const char **argv)
{
    if (argc == 0 || !argv)
	return EXIT_FAILURE;

    /* Misc */
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    const char *gpmpt = DEFAULT_GSH_PROMPT;
    const char *emptypmpt = "";
    int ret = EXIT_SUCCESS;

    /* Application state */
    struct gsh_state s;

    /* Options */
    int print_help = 0;
    int console_mode = 0;  // MGED compatibility var - currently a no-op
    struct bu_opt_desc d[3];
    BU_OPT(d[0],  "h", "help", "",       NULL,              &print_help,     "print help and exit");
    BU_OPT(d[1],  "c", "",     "",       NULL,              &console_mode,   "console mode - MGED compatibility");
    BU_OPT_NULL(d[2]);

    /* Let libbu know where we are */
    bu_setprogname(argv[0]);

    /* Done with program name */
    argv++; argc--;

    /* Parse options, fail if anything goes wrong */
    if ((argc = bu_opt_parse(&msg, argc, argv, d)) == -1) {
	fputs(bu_vls_addr(&msg), stderr);
	bu_vls_free(&msg);
	bu_exit(EXIT_FAILURE, NULL);
    }
    bu_vls_trunc(&msg, 0);

    if (print_help) {
	const char *help = bu_opt_describe(d, NULL);
	bu_vls_sprintf(&msg, "Usage: 'gsh [options] model.g'\n\nOptions:\n%s\n", help);
	bu_free((char *)help, "done with help string");
	fputs(bu_vls_addr(&msg), stderr);
	bu_vls_free(&msg);
	bu_exit(EXIT_SUCCESS, NULL);
    }

    /* If we can't load libged there's no point in continuing */
    if (gsh_state_init(&s) != BRLCAD_OK) {
	bu_vls_free(&msg);
	bu_exit(EXIT_FAILURE, "ERROR, could not load libged: %s\n", bu_dlerror());
    }

    const char *ged_init_str = ged_init_msgs();
    if (strlen(ged_init_str)) {
	fprintf(stderr, "%s", ged_init_str);
    }

    /* FIXME: To draw, we need to init this LIBRT global */
    BU_LIST_INIT(&RTG.rtg_vlfree);

    /* See if we've been told to pre-load a specific .g file. */
    if (argc) {
	if (!bu_file_exists(argv[0], NULL)) {
	    gsh_state_free(&s);
	    bu_vls_free(&msg);
	    bu_exit(EXIT_FAILURE, "File %s does not exist, expecting .g file\n", argv[0]) ;
	}
	s.gedp = ged_open("db", argv[0], 1);
	if (!s.gedp) {
	    gsh_state_free(&s);
	    bu_vls_free(&msg);
	    bu_exit(EXIT_FAILURE, "Could not open %s as a .g file\n", argv[0]) ;
	} else {
	    s.gedp->ged_gvp = s.view;
	    bu_vls_sprintf(&s.gfile, "%s", argv[0]);
	}
    }

    /* If we have been given more than a .g filename execute the provided argv
     * commands and exit. Deliberately making this case a very simple execution
     * path rather than having the linenoise interactive block deal with it, to
     * minimize the possibility of any unforeseen complications. */
    if (argc > 1) {
	/* If we reach this part of the code, argv[0] is a .g file and
	 * has been handled - skip ahead to the commands. */
	argv++; argc--;
	ret = geval(&s, argc, argv);
	goto done;
    }

    /* We can handle stdin commands, but we don't want to interject the prompt
     * into the output if that's the mode we're in.  Check. */
    s.pmpt = (bu_interactive()) ? gpmpt : emptypmpt;

    /* Start the loop */

    while ((s.line = linenoise(s.pmpt)) != NULL) {

	bu_vls_sprintf(&s.iline, "%s", s.line);
	free(s.line);
	bu_vls_trimspace(&s.iline);
	if (!bu_vls_strlen(&s.iline)) continue;
	linenoiseHistoryAdd(bu_vls_addr(&s.iline));

	/* The "clear" command is only for the shell, not
	 * for libged */
	if (BU_STR_EQUAL(bu_vls_addr(&s.iline), "clear")) {
	    linenoiseClearScreen();
	    bu_vls_trunc(&s.iline, 0);
	    continue;
	}

	/* The "quit" command is also not for libged */
	if (BU_STR_EQUAL(bu_vls_addr(&s.iline), "q")) goto done;
	if (BU_STR_EQUAL(bu_vls_addr(&s.iline), "quit")) goto done;
	if (BU_STR_EQUAL(bu_vls_addr(&s.iline), "exit")) goto done;

	/* Looks like we'll be running a GED command - stash the state
	 * of the view info */
	if (s.gedp && s.gedp->ged_dmp) {
	    s.prev_dhash = (s.gedp->ged_dmp) ? dm_hash((struct dm *)s.gedp->ged_dmp) : 0;
	    s.prev_vhash = bv_hash(s.gedp->ged_gvp);
	    s.prev_lhash = dl_name_hash(s.gedp);
	    s.prev_ghash = ged_dl_hash((struct display_list *)s.gedp->ged_gdp->gd_headDisplay);
	}

	/* OK, try a GED command - make an argv array from the input line */
	char *input = bu_strdup(bu_vls_cstr(&s.iline));
	char **av = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
	int ac = bu_argv_from_string(av, strlen(input), input);

	/* The "open" and close commands require a bit of
	 * awareness at this level, since the gedp pointer
	 * must respond to them. */
	if (BU_STR_EQUAL(av[0], "open")) {
	    if (ac > 1) {
		if (s.gedp) ged_close(s.gedp);
		s.gedp = ged_open("db", av[1], 0);
		if (!s.gedp) {
		    bu_vls_free(&msg);
		    gsh_state_free(&s);
		    bu_exit(EXIT_FAILURE, "Could not open %s as a .g file\n", av[1]) ;
		} else {
		    s.gedp->ged_gvp = s.view;
		    bu_vls_sprintf(&s.gfile, "%s", av[1]);
		    printf("Opened file %s\n", bu_vls_cstr(&s.gfile));
		}
	    } else {
		printf("Error: invalid ged_open call\n");
	    }
	    bu_free(input, "input copy");
	    bu_free(av, "input argv");
	    bu_vls_trunc(&s.iline, 0);
	    continue;
	}

	if (!s.gedp) {
	    printf("Error: no database is currently open.\n");
	    bu_free(input, "input copy");
	    bu_free(av, "input argv");
	    bu_vls_trunc(&s.iline, 0);
	    continue;
	}


	if (BU_STR_EQUAL(av[0], "close")) {
	    ged_close(s.gedp);
	    s.gedp = NULL;
	    printf("closed database %s\n", bu_vls_cstr(&s.gfile));
	    bu_vls_trunc(&s.gfile, 0);
	    bu_free(input, "input copy");
	    bu_free(av, "input argv");
	    bu_vls_trunc(&s.iline, 0);
	    continue;
	}

	/* If we're not opening or closing, and we have an active gedp,
	 * make a standard libged call */
	if (ged_cmd_valid(av[0], NULL)) {
	    const char *ccmd = NULL;
	    int edist = ged_cmd_lookup(&ccmd, av[0]);
	    if (edist) {
		printf("Command %s not found, did you mean %s (edit distance %d)?\n", av[0], ccmd, edist);
	    }
	} else {
	    ged_exec(s.gedp, ac, (const char **)av);
	    printf("%s", bu_vls_cstr(s.gedp->ged_result_str));
	    bu_vls_trunc(s.gedp->ged_result_str, 0);

	    // The command ran, see if the display needs updating
	    view_update(&s);
	}

	bu_free(input, "input copy");
	bu_free(av, "input argv");
	bu_vls_trunc(&s.iline, 0);
    }

done:
    gsh_state_free(&s);
    linenoiseHistoryFree();
    bu_vls_free(&msg);
    return ret;
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
