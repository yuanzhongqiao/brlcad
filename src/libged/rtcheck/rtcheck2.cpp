/*                         R T C H E C K . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file libged/rtcheck.c
 *
 * The rtcheck command.
 *
 */

#include "common.h"

#include <stdlib.h>

#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif
#include "bio.h"
#include "bresource.h"

#include <set>

#include "bu/app.h"

#include "../ged_private.h"

#define GET_BV_SCENE_OBJ(p, fp) { \
    if (BU_LIST_IS_EMPTY(fp)) { \
       BU_ALLOC((p), struct bv_scene_obj); \
    } else { \
       p = BU_LIST_NEXT(bv_scene_obj, fp); \
       BU_LIST_DEQUEUE(&((p)->l)); \
    } \
    BU_LIST_INIT( &((p)->s_vlist) ); }

struct ged_rtcheck {
    struct ged_subprocess *rrtp;
    FILE *fp;
    struct bv_vlblock *vbp;
    struct bu_list *vhead;
    double csize;
    void *chan;
    int read_failed;
    int draw_read_failed;
};

static void
rtcheck_handler_cleanup(struct ged_rtcheck *rtcp, int type)
{
    if (type != 2)
	return;
    struct ged_subprocess *rrtp = rtcp->rrtp;
    struct ged *gedp = rrtp->gedp;

    if (gedp->ged_delete_io_handler) {
	(*gedp->ged_delete_io_handler)(rrtp, BU_PROCESS_STDOUT);
	(*gedp->ged_delete_io_handler)(rrtp, BU_PROCESS_STDERR);
    }

    bu_log("called cleanup: %d\n", type);

    bu_process_close(rrtp->p, BU_PROCESS_STDOUT);
    /* wait for the forked process */
    int retcode = bu_process_wait(NULL, rrtp->p, 0);
    if (retcode != 0) {
	_ged_wait_status(gedp->ged_result_str, retcode);
    }
    bu_ptbl_rm(&gedp->ged_subp, (long *)rrtp);
    BU_PUT(rrtp, struct ged_subprocess);
    BU_PUT(rtcp, struct ged_rtcheck);
}

static void
rtcheck_vector_handler(void *clientData, int type)
{
    int value = 0;
    struct ged_rtcheck *rtcp = (struct ged_rtcheck *)clientData;
    struct ged_subprocess *rrtp = rtcp->rrtp;
    BU_CKMAG(rrtp, GED_CMD_MAGIC, "ged subprocess");
    struct ged *gedp = rrtp->gedp;

    /* Get vector output from rtcheck */
    if (!rtcp->draw_read_failed && (feof(rtcp->fp) || (value = getc(rtcp->fp)) == EOF)) {
	size_t i;
	rtcp->draw_read_failed = 1;

	// Clear any prior rtcheck outputs - whether or not we have new
	// overlaps to draw, we're eliminating all the old objects
	const char *sname = "rtcheck::";
	struct bu_ptbl *vobjs = gedp->ged_gvp->gv_view_objs;
	std::set<struct bv_scene_obj *> robjs;
	for (i = 0; i < BU_PTBL_LEN(vobjs); i++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(vobjs, i);
	    if (!bu_strncmp(sname, bu_vls_cstr(&s->s_name), strlen(sname))) {
		robjs.insert(s);
	    }
	}
	std::set<struct bv_scene_obj *>::iterator r_it;
	for (r_it = robjs.begin(); r_it != robjs.end(); r_it++) {
	    struct bv_scene_obj *s = *r_it;
	    bv_scene_obj_free(s, gedp->free_scene_obj);
	    bu_ptbl_rm(vobjs, (long *)s);
	}

	/* Add any visual output generated by rtcheck */
	if (rtcp->vbp) {
	    bu_log("final nused: %ld\n", rtcp->vbp->nused);
	    for (i = 0; i < rtcp->vbp->nused; i++) {
		if (!BU_LIST_IS_EMPTY(&(rtcp->vbp->head[i]))) {
		    continue;
		struct bv_scene_obj *s;
		GET_BV_SCENE_OBJ(s, &gedp->free_scene_obj->l);
		bu_vls_sprintf(&s->s_name, "%sobj%zd", sname, i);
		bu_vls_sprintf(&s->s_uuid, "%sobj%zd", sname, i);
		struct bv_vlist *bvl = (struct bv_vlist *)&rtcp->vbp->head[i];
		long int rgb = rtcp->vbp->rgb[i];
		s->s_vlen = bv_vlist_cmd_cnt(bvl);
		BU_LIST_APPEND_LIST(&(s->s_vlist), &(bvl->l));
		BU_LIST_INIT(&(bvl->l));
		s->s_color[0] = (rgb>>16);
		s->s_color[1] = (rgb>>8);
		s->s_color[2] = (rgb) & 0xFF;
		bu_ptbl_ins(vobjs, (long *)s);
	    }
	    bv_vlblock_free(rtcp->vbp);
	}

	rtcp->vbp = NULL;
	}
    }

    if (rtcp->read_failed && rtcp->draw_read_failed) {
	rtcheck_handler_cleanup(rtcp, type);
	return;
    }

    if (!rtcp->draw_read_failed) {
	(void)rt_process_uplot_value(&rtcp->vhead,
		rtcp->vbp,
		rtcp->fp,
		value,
		rtcp->csize,
		gedp->ged_gdp->gd_uplotOutputMode);
    }
    bu_log("nused: %ld\n", rtcp->vbp->nused);
}

static void
rtcheck_output_handler(void *clientData, int type)
{
    int count;
    char line[RT_MAXLINE] = {0};
    struct ged_rtcheck *rtcp = (struct ged_rtcheck *)clientData;
    struct ged_subprocess *rrtp = rtcp->rrtp;
    BU_CKMAG(rrtp, GED_CMD_MAGIC, "ged subprocess");
    struct ged *gedp = rrtp->gedp;

    /* Get textual output from rtcheck */
    if (bu_process_read((char *)line, &count, rrtp->p, BU_PROCESS_STDERR, RT_MAXLINE) <= 0) {
	rtcp->read_failed = 1;
	if (gedp->ged_gdp->gd_rtCmdNotify != (void (*)(int))0)
	    gedp->ged_gdp->gd_rtCmdNotify(0);
    }


    if (rtcp->read_failed && rtcp->draw_read_failed) {
	rtcheck_handler_cleanup(rtcp, type);
	return;
    }

    line[count] = '\0';
    if (gedp->ged_output_handler != (void (*)(struct ged *, char *))0) {
	ged_output_handler_cb(gedp, line);
    }	else {
	bu_vls_printf(gedp->ged_result_str, "%s", line);
    }
}

/*
 * Check for overlaps in the current view.
 *
 * Usage:
 * rtcheck options
 *
 */
extern "C" int
ged_rtcheck2_core(struct ged *gedp, int argc, const char *argv[])
{
    char **vp;
    int i;
    FILE *fp;
    struct ged_rtcheck *rtcp;
    struct bu_process *p = NULL;
    char ** gd_rt_cmd = NULL;
    int gd_rt_cmd_len = 0;

    vect_t eye_model;

    const char *bin;
    char rtcheck[256] = {0};
    size_t args = 0;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    bin = bu_dir(NULL, 0, BU_DIR_BIN, NULL);
    if (bin) {
	snprintf(rtcheck, 256, "%s/%s", bin, argv[0]);
    }

    args = argc + 7 + 2 + ged_who_argc(gedp);
    gd_rt_cmd = (char **)bu_calloc(args, sizeof(char *), "alloc gd_rt_cmd");

    vp = &gd_rt_cmd[0];
    *vp++ = rtcheck;
    *vp++ = (char *)"-M";
    for (i = 1; i < argc; i++)
	*vp++ = (char *)argv[i];

    *vp++ = gedp->ged_wdbp->dbip->dbi_filename;

    /*
     * Now that we've grabbed all the options, if no args remain,
     * append the names of all stuff currently displayed.
     * Otherwise, simply append the remaining args.
     */
    if (i == argc) {
	gd_rt_cmd_len = vp - gd_rt_cmd;
	int cmd_prev_len = gd_rt_cmd_len;
	gd_rt_cmd_len += ged_who_argv(gedp, vp, (const char **)&gd_rt_cmd[args]);
	if (gd_rt_cmd_len == cmd_prev_len) {
	    // Nothing specified, nothing displayed
	    bu_vls_printf(gedp->ged_result_str, "no objects displayed\n");
	    bu_free(gd_rt_cmd, "free gd_rt_cmd");
	    return GED_ERROR;
	}
    } else {
	while (i < argc)
	    *vp++ = (char *)argv[i++];
	*vp = 0;
	vp = &gd_rt_cmd[0];
	while (*vp) {
	    bu_vls_printf(gedp->ged_result_str, "%s ", *vp);
	    vp++;
	}
	bu_vls_printf(gedp->ged_result_str, "\n");
    }


    bu_process_exec(&p, gd_rt_cmd[0], gd_rt_cmd_len, (const char **)gd_rt_cmd, 0, 0);

    if (bu_process_pid(p) == -1) {
	bu_vls_printf(gedp->ged_result_str, "\nunable to successfully launch subprocess: ");
	for (int pi = 0; pi < gd_rt_cmd_len; pi++) {
	    bu_vls_printf(gedp->ged_result_str, "%s ", gd_rt_cmd[pi]);
	}
	bu_vls_printf(gedp->ged_result_str, "\n");
	bu_free(gd_rt_cmd, "free gd_rt_cmd");
	return GED_ERROR;
    }

    fp = bu_process_open(p, BU_PROCESS_STDIN);

    _ged_rt_set_eye_model(gedp, eye_model);
    _ged_rt_write(gedp, fp, eye_model, -1, NULL);

    bu_process_close(p, BU_PROCESS_STDIN);

    /* create the rtcheck struct */
    BU_GET(rtcp, struct ged_rtcheck);
    rtcp->fp = bu_process_open(p, BU_PROCESS_STDOUT);
    /* Needed on Windows for successful rtcheck drawing data communication */
    setmode(fileno(rtcp->fp), O_BINARY);
    rtcp->vbp = rt_vlblock_init();
    rtcp->vhead = bv_vlblock_find(rtcp->vbp, 0xFF, 0xFF, 0x00);
    rtcp->csize = gedp->ged_gvp->gv_scale * 0.01;
    rtcp->read_failed = 0;
    rtcp->draw_read_failed = 0;

    /* create the ged_subprocess container */
    BU_GET(rtcp->rrtp, struct ged_subprocess);
    rtcp->rrtp->magic = GED_CMD_MAGIC;
    rtcp->rrtp->p = p;
    rtcp->rrtp->aborted = 0;
    rtcp->rrtp->gedp = gedp;
    rtcp->rrtp->stdin_active = 0;
    rtcp->rrtp->stdout_active = 0;
    rtcp->rrtp->stderr_active = 0;

    if (gedp->ged_create_io_handler) {
	(*gedp->ged_create_io_handler)(rtcp->rrtp, BU_PROCESS_STDOUT, rtcheck_vector_handler, (void *)rtcp);
    }
    bu_ptbl_ins(&gedp->ged_subp, (long *)rtcp->rrtp);

    if (gedp->ged_create_io_handler) {
	(*gedp->ged_create_io_handler)(rtcp->rrtp, BU_PROCESS_STDERR, rtcheck_output_handler, (void *)rtcp);
    }

    bu_free(gd_rt_cmd, "free gd_rt_cmd");

    return GED_OK;
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
