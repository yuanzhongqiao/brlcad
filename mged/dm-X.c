/*
 *			D M - X . C
 *
 *  An X Window System interface for MGED.
 *  X11R2.  Color support is yet to be implemented.
 *
 *  Author -
 *	Phillip Dykstra
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <math.h>
#include <sys/time.h>		/* for struct timeval */
#include <X11/X.h>
#if 0
#ifdef HAVE_XOSDEFS_H
#include <X11/Xfuncproto.h>
#include <X11/Xosdefs.h>
#endif
#endif
#if defined(linux)
#	undef   X_NOT_STDC_ENV
#	undef   X_NOT_POSIX
#endif
#define XLIB_ILLEGAL_ACCESS	/* necessary on facist SGI 5.0.1 */
#include "tk.h"
#include <X11/extensions/XI.h>
#include <X11/extensions/XInput.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#if IR_KNOBS
#include <gl/device.h>
#endif
#if 0
#include <X11/extensions/Xext.h>
#include <X11/extensions/multibuf.h>
#endif

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_dm.h"
#include "./sedit.h"
#include "dm-X.h"

#ifdef USE_FRAMEBUFFER
extern void X24_configureWindow();
#endif

#ifdef DO_RUBBER_BAND
extern void rt_rect_area(); /* from rect.c */
extern void zoom_rect_area();
#endif

extern void dm_var_init();
extern void mged_print_result();
extern point_t e_axes_pos;

static void	X_statechange();
static int     X_dm();
static void     dirty_hook();
static void     set_knob_offset();

#ifdef USE_PROTOTYPES
static Tk_GenericProc X_doevent;
#else
static int X_doevent();
#endif

extern int dm_pipe[];
extern Tcl_Interp *interp;
extern Tk_Window tkwin;

struct bu_structparse X_vparse[] = {
  {"%d",  1, "zclip",             X_MV_O(zclip),            dirty_hook },
  {"%d",  1, "debug",             X_MV_O(debug),            BU_STRUCTPARSE_FUNC_NULL },
  {"",    0, (char *)0,           0,                        BU_STRUCTPARSE_FUNC_NULL }
};

#if IR_KNOBS
void X_dbtext();
#endif

#ifdef IR_BUTTONS
/*
 * Map SGI Button numbers to MGED button functions.
 * The layout of this table is suggestive of the actual button box layout.
 */
#define SW_HELP_KEY	SW0
#define SW_ZERO_KEY	SW3
#define HELP_KEY	0
#define ZERO_KNOBS	0
static unsigned char bmap[IR_BUTTONS] = {
	HELP_KEY,    BV_ADCURSOR, BV_RESET,    ZERO_KNOBS,
	BE_O_SCALE,  BE_O_XSCALE, BE_O_YSCALE, BE_O_ZSCALE, 0,           BV_VSAVE,
	BE_O_X,      BE_O_Y,      BE_O_XY,     BE_O_ROTATE, 0,           BV_VRESTORE,
	BE_S_TRANS,  BE_S_ROTATE, BE_S_SCALE,  BE_MENU,     BE_O_ILLUMINATE, BE_S_ILLUMINATE,
	BE_REJECT,   BV_BOTTOM,   BV_TOP,      BV_REAR,     BV_45_45,    BE_ACCEPT,
	BV_RIGHT,    BV_FRONT,    BV_LEFT,     BV_35_25
};
#endif

#ifdef IR_KNOBS
/*
 *  Labels for knobs in help mode.
 */
static char	*kn1_knobs[] = {
	/* 0 */ "adc <1",	/* 1 */ "zoom", 
	/* 2 */ "adc <2",	/* 3 */ "adc dist",
	/* 4 */ "adc y",	/* 5 */ "y slew",
	/* 6 */ "adc x",	/* 7 */	"x slew"
};
static char	*kn2_knobs[] = {
	/* 0 */ "unused",	/* 1 */	"zoom",
	/* 2 */ "z rot",	/* 3 */ "z slew",
	/* 4 */ "y rot",	/* 5 */ "y slew",
	/* 6 */ "x rot",	/* 7 */	"x slew"
};
#endif

static int XdoMotion = 0;

int
X_dm_init(o_dm_list, argc, argv)
struct dm_list *o_dm_list;
int argc;
char *argv[];
{
  int i;
  char **av;
  struct bu_vls name_vls, value_vls;

  /* register application provided routines */
  cmd_hook = X_dm;
  state_hook = X_statechange;

  /* stuff in a default initialization script */
  av = (char **)bu_malloc(sizeof(char *)*(argc + 2), "X_dm_init: av");
  av[0] = "X_open";
  av[1] = "-i";
  av[2] = "mged_bind_dm";

  /* copy the rest except last */
  for(i = 1; i < argc-1; ++i)
    av[i+2] = argv[i];

  av[i+2] = (char *)NULL;

  dm_var_init(o_dm_list);
  Tk_DeleteGenericHandler(doEvent, (ClientData)NULL);
  if((dmp = dm_open(DM_TYPE_X, argc+1, av)) == DM_NULL){
    bu_free(av, "X_dm_init: av");
    return TCL_ERROR;
  }

  bu_free(av, "X_dm_init: av");
  zclip_ptr = &((struct x_vars *)dmp->dm_vars)->mvars.zclip;
  eventHandler = X_doevent;
  curr_dm_list->s_info->opp = &pathName;
  Tk_CreateGenericHandler(doEvent, (ClientData)NULL);
  dm_configureWindowShape(dmp);

  bu_vls_init(&value_vls);
#ifdef USE_FRAMEBUFFER
  {
    int status;
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "fb_open_existing /dev/X %lu %lu %lu %lu %d %d %lu",
		  (unsigned long)((struct x_vars *)dmp->dm_vars)->dpy,
		  (unsigned long)((struct x_vars *)dmp->dm_vars)->pix,
		  (unsigned long)((struct x_vars *)dmp->dm_vars)->cmap,
		  (unsigned long)((struct x_vars *)dmp->dm_vars)->vip,
		  dmp->dm_width, dmp->dm_height,
		  (unsigned long)((struct x_vars *)dmp->dm_vars)->gc);
    status = Tcl_Eval(interp, bu_vls_addr(&vls));

    if(status == TCL_OK){
      if(sscanf(interp->result, "%lu", (unsigned long *)&fbp) != 1){
	fbp = (FBIO *)0;   /* sanity */
	Tcl_AppendResult(interp, "X_dm_init: failed to get framebuffer pointer\n",
			 (char *)NULL);
      }else
	bu_vls_printf(&value_vls, "%s ", interp->result);
    }else{
      Tcl_AppendResult(interp, "X_dm_init: failed to get framebuffer\n",
		       (char *)NULL);
    }

    bu_vls_free(&vls);
  }
#endif

#if 1
  /*XXX Experimenting */
  bu_vls_init(&name_vls);
  bu_vls_printf(&name_vls, "dm_info(%s)", bu_vls_addr(&dmp->dm_pathName));
  bu_vls_printf(&value_vls, "%lu %lu %lu %lu %d %d %lu",
		(unsigned long)((struct x_vars *)dmp->dm_vars)->dpy,
		(unsigned long)((struct x_vars *)dmp->dm_vars)->win,
		(unsigned long)((struct x_vars *)dmp->dm_vars)->cmap,
		(unsigned long)((struct x_vars *)dmp->dm_vars)->vip,
		dmp->dm_width, dmp->dm_height,
		(unsigned long)((struct x_vars *)dmp->dm_vars)->gc);
  Tcl_SetVar(interp, bu_vls_addr(&name_vls), bu_vls_addr(&value_vls), TCL_GLOBAL_ONLY);
  bu_vls_free(&name_vls);
  bu_vls_free(&value_vls);
#endif

  return TCL_OK;
}

/*
   This routine is being called from doEvent().
   It does not handle mouse button or key events. The key
   events are being processed via the TCL/TK bind command or are being
   piped to ged.c/stdin_input(). Eventually, I'd also like to have the
   dials+buttons bindable. That would leave this routine to handle only
   events like Expose and ConfigureNotify.
*/
static int
X_doevent(clientData, eventPtr)
ClientData clientData;
XEvent *eventPtr;
{
  static int button0  = 0;   /* State of button 0 */
  static int knob_values[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  struct bu_vls cmd;
  int save_edflag = -1;

  if(mged_variables->send_key && eventPtr->type == KeyPress){
    char buffer[2];
    KeySym keysym;
    struct dm_char_queue *dcqp;

    XLookupString(&(eventPtr->xkey), buffer, 1,
		  &keysym, (XComposeStatus *)NULL);

    if(keysym == mged_variables->hot_key)
      return TCL_OK;

    BU_GETSTRUCT(dcqp, dm_char_queue);
    dcqp->dlp = curr_dm_list;
    BU_LIST_PUSH(&head_dm_char_queue.l, &dcqp->l);
    write(dm_pipe[1], buffer, 1);

    /* Use this so that these events won't propagate */
    return TCL_RETURN;
  }

  bu_vls_init(&cmd);
  if (eventPtr->type == Expose && eventPtr->xexpose.count == 0){
    dirty = 1;

    goto handled;
  }else if(eventPtr->type == ConfigureNotify){
    XConfigureEvent *conf = (XConfigureEvent *)eventPtr;

    dm_configureWindowShape(dmp);
    dirty = 1;

#ifdef USE_FRAMEBUFFER
    X24_configureWindow(fbp, conf->width, conf->height);
#endif

    goto handled;
  } else if( eventPtr->type == MapNotify ){
    mapped = 1;

    goto handled;
  } else if( eventPtr->type == UnmapNotify ){
    mapped = 0;

    goto handled;
  } else if( eventPtr->type == MotionNotify ) {
    int mx, my;
    int dx, dy;
    fastf_t f;
    fastf_t fx, fy;
    fastf_t td;

    mx = eventPtr->xmotion.x;
    my = eventPtr->xmotion.y;
    dx = mx - ((struct x_vars *)dmp->dm_vars)->omx;
    dy = my - ((struct x_vars *)dmp->dm_vars)->omy;

    switch(am_mode){
    case AMM_IDLE:
#if 1
      if(scroll_active)
#else
      if(scroll_active && eventPtr->xmotion.state & ((struct x_vars *)dmp->dm_vars)->mb_mask)
#endif
	bu_vls_printf( &cmd, "M 1 %d %d\n",
		       (int)(dm_Xx2Normal(dmp, mx, 0) * 2047.0),
		       (int)(dm_Xy2Normal(dmp, my) * 2047.0) );
#ifdef DO_RUBBER_BAND
#if 1
      else if(rubber_band_active){
#else
      else if(rubber_band_active && eventPtr->xmotion.state & ((struct x_vars *)dmp->dm_vars)->mb_mask){
#endif
	fastf_t x = dm_Xx2Normal(dmp, mx, 1);
	fastf_t y = dm_Xy2Normal(dmp, my);

	rect_width = x - rect_x;
	rect_height = y - rect_y;

	dirty = 1;
      }
#endif
      else if(XdoMotion)
	/* trackball not active so do the regular thing */
	/* Constant tracking (e.g. illuminate mode) bound to M mouse */
	bu_vls_printf( &cmd, "M 0 %d %d\n",
		       (int)(dm_Xx2Normal(dmp, mx, 1) * 2047.0),
		       (int)(dm_Xy2Normal(dmp, my) * 2047.0) );
      else
	goto handled;

      break;
    case AMM_ROT:
      if((state == ST_S_EDIT || state == ST_O_EDIT) &&
	 mged_variables->transform == 'e'){
	char save_ecoords;

	save_ecoords = mged_variables->ecoords;
	mged_variables->ecoords = 'v';

	if(state == ST_S_EDIT){
	  save_edflag = es_edflag;
	  if(!SEDIT_ROTATE)
	    es_edflag = SROT;
	}else{
	  save_edflag = edobj;
	  edobj = BE_O_ROTATE;
	}

	if(mged_variables->rateknobs)
	  bu_vls_printf(&cmd, "knob -i x %lf y %lf\n",
			dy / (fastf_t)dmp->dm_height * RATE_ROT_FACTOR * 2.0,
			dx / (fastf_t)dmp->dm_width * RATE_ROT_FACTOR * 2.0);
	else
	  bu_vls_printf(&cmd, "knob -i ax %lf ay %lf\n",
			dy * 0.25, dx * 0.25);

	(void)Tcl_Eval(interp, bu_vls_addr(&cmd));

	mged_variables->ecoords = save_ecoords;
	if(state == ST_S_EDIT)
	  es_edflag = save_edflag;
	else
	  edobj = save_edflag;

	((struct x_vars *)dmp->dm_vars)->omx = mx;
	((struct x_vars *)dmp->dm_vars)->omy = my;

	goto handled;
      }

      if(mged_variables->rateknobs)
	bu_vls_printf(&cmd, "knob -i -v x %lf y %lf\n",
		      dy / (fastf_t)dmp->dm_height * RATE_ROT_FACTOR * 2.0,
		      dx / (fastf_t)dmp->dm_width * RATE_ROT_FACTOR * 2.0);
      else
	bu_vls_printf(&cmd, "knob -i -v ax %lf ay %lf\n",
		      dy * 0.25, dx * 0.25);

      break;
    case AMM_TRAN:
      fx = dx / (fastf_t)dmp->dm_width / dmp->dm_aspect * 2.0;
      fy = -dy / (fastf_t)dmp->dm_height * 2.0;
      
      if((state == ST_S_EDIT || state == ST_O_EDIT) &&
	 mged_variables->transform == 'e'){
	char save_ecoords;

	save_ecoords = mged_variables->ecoords;
	mged_variables->ecoords = 'v';

	if(state == ST_S_EDIT){
	  save_edflag = es_edflag;
	  if(!SEDIT_TRAN)
	    es_edflag = STRANS;
	}else{
	  save_edflag = edobj;
	  edobj = BE_O_XY;
	}

	if(mged_variables->rateknobs)
	  bu_vls_printf(&cmd, "knob -i X %lf Y %lf\n", fx, fy);
	else
	  bu_vls_printf(&cmd, "knob -i aX %lf aY %lf\n",
			fx*Viewscale*base2local, fy*Viewscale*base2local);

	(void)Tcl_Eval(interp, bu_vls_addr(&cmd));

	mged_variables->ecoords = save_ecoords;
	if(state == ST_S_EDIT)
	  es_edflag = save_edflag;
	else
	  edobj = save_edflag;

	((struct x_vars *)dmp->dm_vars)->omx = mx;
	((struct x_vars *)dmp->dm_vars)->omy = my;

	goto handled;
      }

      /* otherwise, drag to translate the view */
      if(mged_variables->rateknobs)
	bu_vls_printf( &cmd, "knob -i -v X %lf Y %lf\n", fx, fy );
      else
	bu_vls_printf( &cmd, "knob -i -v aX %lf aY %lf\n",
		       fx*Viewscale*base2local, fy*Viewscale*base2local );

      break;
    case AMM_SCALE:
      if((state == ST_S_EDIT || state == ST_O_EDIT) &&
	 mged_variables->transform == 'e'){
	if(state == ST_S_EDIT && !SEDIT_SCALE){
	  save_edflag = es_edflag;
	  es_edflag = SSCALE;
	}else if(state == ST_O_EDIT && !OEDIT_SCALE){
	  save_edflag = edobj;
	  edobj = BE_O_SCALE;
	}
      }

      if(abs(dx) >= abs(dy))
	f = dx;
      else
	f = -dy;

      if(mged_variables->rateknobs)
	bu_vls_printf( &cmd, "knob -i S %f\n", f / (fastf_t)dmp->dm_height );
      else
	bu_vls_printf( &cmd, "knob -i aS %f\n", f / (fastf_t)dmp->dm_height );

      break;
    case AMM_ADC_ANG1:
      fx = dm_Xx2Normal(dmp, mx, 1) * 2047.0 - dv_xadc;
      fy = dm_Xy2Normal(dmp, my) * 2047.0 - dv_yadc;
      bu_vls_printf(&cmd, "adc a1 %lf\n", DEGRAD*atan2(fy, fx));

      break;
    case AMM_ADC_ANG2:
      fx = dm_Xx2Normal(dmp, mx, 1) * 2047.0 - dv_xadc;
      fy = dm_Xy2Normal(dmp, my) * 2047.0 - dv_yadc;
      bu_vls_printf(&cmd, "adc a2 %lf\n", DEGRAD*atan2(fy, fx));

      break;
    case AMM_ADC_TRAN:
      bu_vls_printf(&cmd, "adc hv %lf %lf\n",
		    dm_Xx2Normal(dmp, mx, 1) * Viewscale * base2local,
		    dm_Xy2Normal(dmp, my) * Viewscale * base2local);

      break;
    case AMM_ADC_DIST:
      fx = (dm_Xx2Normal(dmp, mx, 1) * 2047.0 - dv_xadc) * Viewscale * base2local / 2047.0;
      fy = (dm_Xy2Normal(dmp, my) * 2047.0 - dv_yadc) * Viewscale * base2local / 2047.0;
      td = sqrt(fx * fx + fy * fy);
      bu_vls_printf(&cmd, "adc dst %lf\n", td);

      break;
    case AMM_CON_ROT_X:
      if(abs(dx) >= abs(dy))
	f = dx;
      else
	f = -dy;

      if(mged_variables->rateknobs)
	bu_vls_printf( &cmd, "knob -i x %f\n",
		       f / (fastf_t)dmp->dm_width * RATE_ROT_FACTOR * 2.0 );
      else
	bu_vls_printf( &cmd, "knob -i ax %f\n", f * 0.25 );

      if((state == ST_S_EDIT || state == ST_O_EDIT) &&
	 mged_variables->transform == 'e'){
	if(state == ST_S_EDIT){
	  save_edflag = es_edflag;
	  if(!SEDIT_ROTATE)
	    es_edflag = SROT;
	}else{
	  save_edflag = edobj;
	  edobj = BE_O_ROTATE;
	}

	Tcl_Eval(interp, bu_vls_addr(&cmd));

	if(state == ST_S_EDIT)
	  es_edflag = save_edflag;
	else
	  edobj = save_edflag;

	((struct x_vars *)dmp->dm_vars)->omx = mx;
	((struct x_vars *)dmp->dm_vars)->omy = my;

	goto handled;
      }

      break;
    case AMM_CON_ROT_Y:
      if(abs(dx) >= abs(dy))
	f = dx;
      else
	f = -dy;

      if(mged_variables->rateknobs)
	bu_vls_printf( &cmd, "knob -i y %f\n",
		       f / (fastf_t)dmp->dm_width * RATE_ROT_FACTOR * 2.0 );
      else
	bu_vls_printf( &cmd, "knob -i ay %f\n", f * 0.25 );

      if((state == ST_S_EDIT || state == ST_O_EDIT) &&
	 mged_variables->transform == 'e'){
	if(state == ST_S_EDIT){
	  save_edflag = es_edflag;
	  if(!SEDIT_ROTATE)
	    es_edflag = SROT;
	}else{
	  save_edflag = edobj;
	  edobj = BE_O_ROTATE;
	}

	Tcl_Eval(interp, bu_vls_addr(&cmd));

	if(state == ST_S_EDIT)
	  es_edflag = save_edflag;
	else
	  edobj = save_edflag;

	((struct x_vars *)dmp->dm_vars)->omx = mx;
	((struct x_vars *)dmp->dm_vars)->omy = my;

	goto handled;
      }

      break;
    case AMM_CON_ROT_Z:
      if(abs(dx) >= abs(dy))
	f = dx;
      else
	f = -dy;

      if(mged_variables->rateknobs)
	bu_vls_printf( &cmd, "knob -i z %f\n",
		       f / (fastf_t)dmp->dm_width * RATE_ROT_FACTOR * 2.0 );
      else
	bu_vls_printf( &cmd, "knob -i az %f\n", f * 0.25 );

      if((state == ST_S_EDIT || state == ST_O_EDIT) &&
	 mged_variables->transform == 'e'){
	if(state == ST_S_EDIT){
	  save_edflag = es_edflag;
	  if(!SEDIT_ROTATE)
	    es_edflag = SROT;
	}else{
	  save_edflag = edobj;
	  edobj = BE_O_ROTATE;
	}

	Tcl_Eval(interp, bu_vls_addr(&cmd));

	if(state == ST_S_EDIT)
	  es_edflag = save_edflag;
	else
	  edobj = save_edflag;

	((struct x_vars *)dmp->dm_vars)->omx = mx;
	((struct x_vars *)dmp->dm_vars)->omy = my;

	goto handled;
      }

      break;
    case AMM_CON_TRAN_X:
      if((state == ST_S_EDIT || state == ST_O_EDIT) &&
	 mged_variables->transform == 'e'){
	if(state == ST_S_EDIT && !SEDIT_TRAN){
	  save_edflag = es_edflag;
	  es_edflag = STRANS;
	}else if(state == ST_O_EDIT && !OEDIT_TRAN){
	  save_edflag = edobj;
	  edobj = BE_O_X;
	}
      }

      if(abs(dx) >= abs(dy))
	f = dx / (fastf_t)dmp->dm_width / dmp->dm_aspect * 2.0;
      else
	f = -dy / (fastf_t)dmp->dm_height * 2.0;

      if(mged_variables->rateknobs)
	bu_vls_printf( &cmd, "knob -i X %f\n", f);
      else
	bu_vls_printf( &cmd, "knob -i aX %f\n", f*Viewscale*base2local);

      break;
    case AMM_CON_TRAN_Y:
      if((state == ST_S_EDIT || state == ST_O_EDIT) &&
	 mged_variables->transform == 'e'){
	if(state == ST_S_EDIT && !SEDIT_TRAN){
	  save_edflag = es_edflag;
	  es_edflag = STRANS;
	}else if(state == ST_O_EDIT && !OEDIT_TRAN){
	  save_edflag = edobj;
	  edobj = BE_O_Y;
	}
      }

      if(abs(dx) >= abs(dy))
	f = dx / (fastf_t)dmp->dm_width / dmp->dm_aspect * 2.0;
      else
	f = -dy / (fastf_t)dmp->dm_height * 2.0;

      if(mged_variables->rateknobs)
	bu_vls_printf( &cmd, "knob -i Y %f\n", f);
      else
	bu_vls_printf( &cmd, "knob -i aY %f\n", f*Viewscale*base2local);

      break;
    case AMM_CON_TRAN_Z:
      if((state == ST_S_EDIT || state == ST_O_EDIT) &&
	 mged_variables->transform == 'e'){
	if(state == ST_S_EDIT && !SEDIT_TRAN){
	  save_edflag = es_edflag;
	  es_edflag = STRANS;
	}else if(state == ST_O_EDIT && !OEDIT_TRAN){
	  save_edflag = edobj;
	  edobj = BE_O_XY;
	}
      }

      if(abs(dx) >= abs(dy))
	f = dx / (fastf_t)dmp->dm_width / dmp->dm_aspect * 2.0;
      else
	f = -dy / (fastf_t)dmp->dm_height * 2.0;

      if(mged_variables->rateknobs)
	bu_vls_printf( &cmd, "knob -i Z %f\n", f);
      else
	bu_vls_printf( &cmd, "knob -i aZ %f\n", f*Viewscale*base2local);

      break;
    case AMM_CON_SCALE_X:
      if((state == ST_S_EDIT || state == ST_O_EDIT) &&
	 mged_variables->transform == 'e'){
	if(state == ST_S_EDIT && !SEDIT_SCALE){
	  save_edflag = es_edflag;
	  es_edflag = SSCALE;
	}else if(state == ST_O_EDIT && !OEDIT_SCALE){
	  save_edflag = edobj;
	  edobj = BE_O_XSCALE;
	}
      }

      if(abs(dx) >= abs(dy))
	f = dx;
      else
	f = -dy;

      if(mged_variables->rateknobs)
	bu_vls_printf( &cmd, "knob -i S %f\n", f / (fastf_t)dmp->dm_height );
      else
	bu_vls_printf( &cmd, "knob -i aS %f\n", f / (fastf_t)dmp->dm_height );

      break;
    case AMM_CON_SCALE_Y:
      if((state == ST_S_EDIT || state == ST_O_EDIT) &&
	 mged_variables->transform == 'e'){
	if(state == ST_S_EDIT && !SEDIT_SCALE){
	  save_edflag = es_edflag;
	  es_edflag = SSCALE;
	}else if(state == ST_O_EDIT && !OEDIT_SCALE){
	  save_edflag = edobj;
	  edobj = BE_O_YSCALE;
	}
      }

      if(abs(dx) >= abs(dy))
	f = dx;
      else
	f = -dy;

      if(mged_variables->rateknobs)
	bu_vls_printf( &cmd, "knob -i S %f\n", f / (fastf_t)dmp->dm_height );
      else
	bu_vls_printf( &cmd, "knob -i aS %f\n", f / (fastf_t)dmp->dm_height );

      break;
    case AMM_CON_SCALE_Z:
      if((state == ST_S_EDIT || state == ST_O_EDIT) &&
	 mged_variables->transform == 'e'){
	if(state == ST_S_EDIT && !SEDIT_SCALE){
	  save_edflag = es_edflag;
	  es_edflag = SSCALE;
	}else if(state == ST_O_EDIT && !OEDIT_SCALE){
	  save_edflag = edobj;
	  edobj = BE_O_ZSCALE;
	}
      }

      if(abs(dx) >= abs(dy))
	f = dx;
      else
	f = -dy;

      if(mged_variables->rateknobs)
	bu_vls_printf( &cmd, "knob -i S %f\n", f / (fastf_t)dmp->dm_height );
      else
	bu_vls_printf( &cmd, "knob -i aS %f\n", f / (fastf_t)dmp->dm_height );

      break;
    case AMM_CON_XADC:
      if(abs(dx) >= abs(dy))
	f = dx;
      else
	f = -dy;

      bu_vls_printf( &cmd, "knob -i xadc %f\n",
		     f / (fastf_t)dmp->dm_width / dmp->dm_aspect * 4095.0 );
      break;
    case AMM_CON_YADC:
      if(abs(dx) >= abs(dy))
	f = dx;
      else
	f = -dy;

      bu_vls_printf( &cmd, "knob -i yadc %f\n",
		     f / (fastf_t)dmp->dm_height * 4095.0 );

      break;
    case AMM_CON_ANG1:
      if(abs(dx) >= abs(dy))
	f = dx;
      else
	f = -dy;

      bu_vls_printf( &cmd, "knob -i ang1 %f\n",
		     f / (fastf_t)dmp->dm_width * 90.0 );

      break;
    case AMM_CON_ANG2:
      if(abs(dx) >= abs(dy))
	f = dx;
      else
	f = -dy;

      bu_vls_printf( &cmd, "knob -i ang2 %f\n",
		     f / (fastf_t)dmp->dm_width * 90.0 );

      break;
    case AMM_CON_DIST:
      if(abs(dx) >= abs(dy))
	f = dx;
      else
	f = -dy;

      bu_vls_printf( &cmd, "knob -i distadc %f\n",
		     f / (fastf_t)dmp->dm_width / dmp->dm_aspect * 4095.0 );

      break;
    }

    ((struct x_vars *)dmp->dm_vars)->omx = mx;
    ((struct x_vars *)dmp->dm_vars)->omy = my;
  }
#if IR_KNOBS
  else if( eventPtr->type == ((struct x_vars *)dmp->dm_vars)->devmotionnotify ){
    XDeviceMotionEvent *M;
    int setting;
    fastf_t f;

    M = (XDeviceMotionEvent * ) eventPtr;

    if(button0){
      X_dbtext(
		(mged_variables->adcflag ? kn1_knobs:kn2_knobs)[M->first_axis]);
      goto handled;
    }

    switch(DIAL0 + M->first_axis){
    case DIAL0:
      if(mged_variables->adcflag) {
	if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	   ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	   !dv_1adc )
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	    M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	    dm_unlimit(dv_1adc) + M->axis_data[0] - knob_values[M->first_axis];

	setting = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd, "knob ang1 %f\n",
		       45.0 - 45.0*((double)setting)/2047.0);
      }else{
	if(mged_variables->rateknobs){
	  f = rate_model_rotate[Z];

	  if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	     !f )
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	      M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(512.5 * f)) +
	       M->axis_data[0] - knob_values[M->first_axis];

	  setting = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd, "knob -m z %f\n", setting / 512.0 );
	}else{
	  f = absolute_model_rotate[Z];

	  if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	     !f )
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	      M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(2.847 * f)) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  f = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]) / 512.0;
	  bu_vls_printf( &cmd, "knob -m az %f\n", dm_wrap(f) * 180.0);
	}
      }
      break;
    case DIAL1:
      if(mged_variables->rateknobs){
	if(EDIT_SCALE && mged_variables->transform == 'e')
	  f = edit_rate_scale;
	else
	  f = rate_scale;

	if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	   ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	   !f )
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	    M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	    dm_unlimit((int)(512.5 * f)) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd, "knob S %f\n", setting / 512.0 );
      }else{
	if(EDIT_SCALE && mged_variables->transform == 'e')
	  f = edit_absolute_scale;
	else
	  f = absolute_scale;

	if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	   ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	   !f )
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	    M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	    dm_unlimit((int)(512.5 * f)) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd, "knob aS %f\n", setting / 512.0 );
      }
      break;
    case DIAL2:
      if(mged_variables->adcflag){
	if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	   ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	   !dv_2adc )
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	    M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	    dm_unlimit(dv_2adc) + M->axis_data[0] - knob_values[M->first_axis];

	setting = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd, "knob ang2 %f\n",
		       45.0 - 45.0*((double)setting)/2047.0);
      }else {
	if(mged_variables->rateknobs){
	  if((state == ST_S_EDIT || state == ST_O_EDIT)
	     && mged_variables->transform == 'e'){
	    switch(mged_variables->ecoords){
	    case 'm':
	      f = edit_rate_model_rotate[Z];
	      break;
	    case 'o':
	      f = edit_rate_object_rotate[Z];
	      break;
	    case 'v':
	    default:
	      f = edit_rate_view_rotate[Z];
	      break;
	    }

	    if(state == ST_S_EDIT && !SEDIT_ROTATE){
	      save_edflag = es_edflag;
	      es_edflag = SROT;
	    }else if(state == ST_O_EDIT && !OEDIT_ROTATE){
	      save_edflag = edobj;
	      edobj = BE_O_ROTATE;
	    }
	  }else if(mged_variables->coords == 'm')
	    f = rate_model_rotate[Z];
	  else
	    f = rate_rotate[Z];

	  if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	     !f )
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	      M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(512.5 * f)) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd, "knob z %f\n", setting / 512.0 );
	}else{
	  if((state == ST_S_EDIT || state == ST_O_EDIT)
	     && mged_variables->transform == 'e'){
	    switch(mged_variables->ecoords){
	    case 'm':
	      f = edit_absolute_model_rotate[Z];
	      break;
	    case 'o':
	      f = edit_absolute_object_rotate[Z];
	      break;
	    case 'v':
	    default:
	      f = edit_absolute_view_rotate[Z];
	      break;
	    }

	    if(state == ST_S_EDIT && !SEDIT_ROTATE){
	      save_edflag = es_edflag;
	      es_edflag = SROT;
	    }else if(state == ST_O_EDIT && !OEDIT_ROTATE){
	      save_edflag = edobj;
	      edobj = BE_O_ROTATE;
	    }
	  }else if(mged_variables->coords == 'm')
	    f = absolute_model_rotate[Z];
	  else
	    f = absolute_rotate[Z];

	  if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	     !f )
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	      M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(2.847 * f)) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  f = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]) / 512.0;
	  bu_vls_printf( &cmd, "knob az %f\n", dm_wrap(f) * 180.0);
	}
      }
      break;
    case DIAL3:
      if(mged_variables->adcflag){
	if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	   ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	   !dv_distadc)
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	    M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	    dm_unlimit(dv_distadc) + M->axis_data[0] - knob_values[M->first_axis];

	setting = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd, "knob distadc %d\n", setting );
      }else {
	if(mged_variables->rateknobs){
	  if((state == ST_S_EDIT || state == ST_O_EDIT)
	     && mged_variables->transform == 'e'){
	    switch(mged_variables->ecoords){
	    case 'm':
	    case 'o':
	      f = edit_rate_model_tran[Z];
	      break;
	    case 'v':
	    default:
	      f = edit_rate_view_tran[Z];
	      break;
	    }

	    if(state == ST_S_EDIT && !SEDIT_TRAN){
	      save_edflag = es_edflag;
	      es_edflag = STRANS;
	    }else if(state == ST_O_EDIT && !OEDIT_TRAN){
	      save_edflag = edobj;
	      edobj = BE_O_XY;
	    }
	  }else if(mged_variables->coords == 'm')
	    f = rate_model_tran[Z];
	  else
	    f = rate_tran[Z];

	  if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	     !f )
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	      M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(512.5 * f)) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd, "knob Z %f\n", setting / 512.0 );
	}else{
	  if((state == ST_S_EDIT || state == ST_O_EDIT)
	     && mged_variables->transform == 'e'){
	    switch(mged_variables->ecoords){
	    case 'm':
	    case 'o':
	      f = edit_absolute_model_tran[Z];
	      break;
	    case 'v':
	    default:
	      f = edit_absolute_view_tran[Z];
	      break;
	    }

	    if(state == ST_S_EDIT && !SEDIT_TRAN){
	      save_edflag = es_edflag;
	      es_edflag = STRANS;
	    }else if(state == ST_O_EDIT && !OEDIT_TRAN){
	      save_edflag = edobj;
	      edobj = BE_O_XY;
	    }
	  }else if(mged_variables->coords == 'm')
	    f = absolute_model_tran[Z];
	  else
	    f = absolute_tran[Z];

	  if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	     !f )
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	      M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(512.5 * f)) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf(&cmd, "knob aZ %f\n", setting / 512.0 * Viewscale * base2local);
	}
      }
      break;
    case DIAL4:
      if(mged_variables->adcflag){
	if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	   ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	   !dv_yadc)
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	    M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	    dm_unlimit(dv_yadc) + M->axis_data[0] - knob_values[M->first_axis];

	setting = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd, "knob yadc %d\n", setting );
      }else{
	if(mged_variables->rateknobs){
	  if((state == ST_S_EDIT || state == ST_O_EDIT)
	     && mged_variables->transform == 'e'){
	    switch(mged_variables->ecoords){
	    case 'm':
	      f = edit_rate_model_rotate[Y];
	      break;
	    case 'o':
	      f = edit_rate_object_rotate[Y];
	      break;
	    case 'v':
	    default:
	      f = edit_rate_view_rotate[Y];
	      break;
	    }

	    if(state == ST_S_EDIT && !SEDIT_ROTATE){
	      save_edflag = es_edflag;
	      es_edflag = SROT;
	    }else if(state == ST_O_EDIT && !OEDIT_ROTATE){
	      save_edflag = edobj;
	      edobj = BE_O_ROTATE;
	    }
	  }else if(mged_variables->coords == 'm')
	    f = rate_model_rotate[Y];
	  else
	    f = rate_rotate[Y];

	  if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	     !f )
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	      M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(512.5 * f)) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd, "knob y %f\n", setting / 512.0 );
	}else{
	  if((state == ST_S_EDIT || state == ST_O_EDIT)
	     && mged_variables->transform == 'e'){
	    switch(mged_variables->ecoords){
	    case 'm':
	      f = edit_absolute_model_rotate[Y];
	      break;
	    case 'o':
	      f = edit_absolute_object_rotate[Y];
	      break;
	    case 'v':
	    default:
	      f = edit_absolute_view_rotate[Y];
	      break;
	    }

	    if(state == ST_S_EDIT && !SEDIT_ROTATE){
	      save_edflag = es_edflag;
	      es_edflag = SROT;
	    }else if(state == ST_O_EDIT && !OEDIT_ROTATE){
	      save_edflag = edobj;
	      edobj = BE_O_ROTATE;
	    }
	  }else if(mged_variables->coords == 'm')
	    f = absolute_model_rotate[Y];
	  else
	    f = absolute_rotate[Y];

	  if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	     !f )
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	      M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(2.847 * f)) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  f = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]) / 512.0;
	  bu_vls_printf( &cmd, "knob ay %f\n", dm_wrap(f) * 180.0);
	}
      }
      break;
    case DIAL5:
      if(mged_variables->rateknobs){
	  if((state == ST_S_EDIT || state == ST_O_EDIT)
	     && mged_variables->transform == 'e'){
	    switch(mged_variables->ecoords){
	    case 'm':
	    case 'o':
	      f = edit_rate_model_tran[Y];
	      break;
	    case 'v':
	    default:
	      f = edit_rate_view_tran[Y];
	      break;
	    }

	    if(state == ST_S_EDIT && !SEDIT_TRAN){
	      save_edflag = es_edflag;
	      es_edflag = STRANS;
	    }else if(state == ST_O_EDIT && !OEDIT_TRAN){
	      save_edflag = edobj;
	      edobj = BE_O_XY;
	    }
	  }else if(mged_variables->coords == 'm')
	    f = rate_model_tran[Y];
	  else
	    f = rate_tran[Y];

	  if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	     !f )
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	      M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(512.5 * f)) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd, "knob Y %f\n", setting / 512.0 );
      }else{
	if((state == ST_S_EDIT || state == ST_O_EDIT)
	   && mged_variables->transform == 'e'){
	  switch(mged_variables->ecoords){
	  case 'm':
	  case 'o':
	    f = edit_absolute_model_tran[Y];
	    break;
	  case 'v':
	  default:
	    f = edit_absolute_view_tran[Y];
	    break;
	  }

	  if(state == ST_S_EDIT && !SEDIT_TRAN){
	    save_edflag = es_edflag;
	    es_edflag = STRANS;
	  }else if(state == ST_O_EDIT && !OEDIT_TRAN){
	    save_edflag = edobj;
	    edobj = BE_O_XY;
	  }
	}else if(mged_variables->coords == 'm')
	  f = absolute_model_tran[Y];
	else
	  f = absolute_tran[Y];

	if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	   ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	   !f )
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	    M->axis_data[0] -
	    knob_values[M->first_axis];
	else
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	    dm_unlimit((int)(512.5 * f)) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf(&cmd, "knob aY %f\n", setting / 512.0 * Viewscale * base2local);
      }
      break;
    case DIAL6:
      if(mged_variables->adcflag){
	if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	   ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	   !dv_xadc)
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	    M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	    dm_unlimit(dv_xadc) + M->axis_data[0] - knob_values[M->first_axis];

	setting = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd, "knob xadc %d\n", setting );
      }else{
	if(mged_variables->rateknobs){
	  if((state == ST_S_EDIT || state == ST_O_EDIT)
	     && mged_variables->transform == 'e'){
	    switch(mged_variables->ecoords){
	    case 'm':
	      f = edit_rate_model_rotate[X];
	      break;
	    case 'o':
	      f = edit_rate_object_rotate[X];
	      break;
	    case 'v':
	    default:
	      f = edit_rate_view_rotate[X];
	      break;
	    }

	    if(state == ST_S_EDIT && !SEDIT_ROTATE){
	      save_edflag = es_edflag;
	      es_edflag = SROT;
	    }else if(state == ST_O_EDIT && !OEDIT_ROTATE){
	      save_edflag = edobj;
	      edobj = BE_O_ROTATE;
	    }
	  }else if(mged_variables->coords == 'm')
	    f = rate_model_rotate[X];
	  else
	    f = rate_rotate[X];

	  if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	     !f )
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	      M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(512.5 * f)) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd, "knob x %f\n", setting / 512.0);
	}else{
	  if((state == ST_S_EDIT || state == ST_O_EDIT)
	     && mged_variables->transform == 'e'){
	    switch(mged_variables->ecoords){
	    case 'm':
	      f = edit_absolute_model_rotate[X];
	      break;
	    case 'o':
	      f = edit_absolute_object_rotate[X];
	      break;
	    case 'v':
	    default:
	      f = edit_absolute_view_rotate[X];
	      break;
	    }

	    if(state == ST_S_EDIT && !SEDIT_ROTATE){
	      save_edflag = es_edflag;
	      es_edflag = SROT;
	    }else if(state == ST_O_EDIT && !OEDIT_ROTATE){
	      save_edflag = edobj;
	      edobj = BE_O_ROTATE;
	    }
	  }else if(mged_variables->coords == 'm')
	    f = absolute_model_rotate[X];
	  else
	    f = absolute_rotate[X];

	  if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	     !f )
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	      M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(2.847 * f)) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  f = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]) / 512.0;
	  bu_vls_printf( &cmd, "knob ax %f\n", dm_wrap(f) * 180.0);
	}
      }
      break;
    case DIAL7:
      if(mged_variables->rateknobs){
	if((state == ST_S_EDIT || state == ST_O_EDIT)
	   && mged_variables->transform == 'e'){
	  switch(mged_variables->ecoords){
	  case 'm':
	  case 'o':
	    f = edit_rate_model_tran[X];
	    break;
	  case 'v':
	  default:
	    f = edit_rate_view_tran[X];
	    break;
	  }

	  if(state == ST_S_EDIT && !SEDIT_TRAN){
	    save_edflag = es_edflag;
	    es_edflag = STRANS;
	  }else if(state == ST_O_EDIT && !OEDIT_TRAN){
	    save_edflag = edobj;
	    edobj = BE_O_XY;
	  }
	}else if(mged_variables->coords == 'm')
	  f = rate_model_tran[X];
	else
	  f = rate_tran[X];

	if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	   ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	   !f )
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	    M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	    dm_unlimit((int)(512.5 * f)) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd, "knob X %f\n", setting / 512.0 );
      }else{
	if((state == ST_S_EDIT || state == ST_O_EDIT)
	   && mged_variables->transform == 'e'){
	  switch(mged_variables->ecoords){
	  case 'm':
	  case 'o':
	    f = edit_absolute_model_tran[X];
	    break;
	  case 'v':
	  default:
	    f = edit_absolute_view_tran[X];
	    break;
	  }

	  if(state == ST_S_EDIT && !SEDIT_TRAN){
	    save_edflag = es_edflag;
	    es_edflag = STRANS;
	  }else if(state == ST_O_EDIT && !OEDIT_TRAN){
	    save_edflag = edobj;
	    edobj = BE_O_XY;
	  }
	}else if(mged_variables->coords == 'm')
	  f = absolute_model_tran[X];
	else
	  f = absolute_tran[X];

	if(-NOISE <= ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	   ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] <= NOISE &&
	   !f )
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	    M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	    dm_unlimit((int)(512.5 * f)) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = dm_limit(((struct x_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf(&cmd, "knob aX %f\n", setting / 512.0 * Viewscale * base2local);
      }
      break;
    default:
      break;
    }

    /* Keep track of the knob values */
    knob_values[M->first_axis] = M->axis_data[0];
  }
#endif
#if IR_BUTTONS
  else if( eventPtr->type == ((struct x_vars *)dmp->dm_vars)->devbuttonpress ){
    XDeviceButtonEvent *B;

    B = (XDeviceButtonEvent * ) eventPtr;

    if(B->button == 1){
      button0 = 1;
      goto handled;
    }

    if(button0){
      X_dbtext(label_button(bmap[B->button - 1]));
    }else if(B->button == 4){
      bu_vls_strcat(&cmd, "knob zero\n");
      set_knob_offset();
    }else
      bu_vls_printf(&cmd, "press %s\n",
		    label_button(bmap[B->button - 1]));
  }else if( eventPtr->type == ((struct x_vars *)dmp->dm_vars)->devbuttonrelease ){
    XDeviceButtonEvent *B;

    B = (XDeviceButtonEvent * ) eventPtr;

    if(B->button == 1)
      button0 = 0;

    goto handled;
  }
#endif
  else if(eventPtr->type == KeyPress){
    /*XXX Hack to prevent Tk from choking on certain control sequences */
    if(eventPtr->xkey.state & ControlMask){
      char buffer[1];
      KeySym keysym;

      XLookupString(&(eventPtr->xkey), buffer, 1,
		    &keysym, (XComposeStatus *)NULL);

      if(keysym == XK_c || keysym == XK_t || keysym == XK_v ||
	 keysym == XK_w || keysym == XK_x || keysym == XK_y)
	goto handled;
    }

    /* let other KeyPress events get processed by Tcl/Tk */
    goto not_handled;
  }else{
    /* allow all other events to be handled by Tcl/Tk */
    goto not_handled;
  }

  (void)Tcl_Eval(interp, bu_vls_addr(&cmd));

  if(save_edflag != -1){
    if(SEDIT_TRAN || SEDIT_ROTATE || SEDIT_SCALE)
      es_edflag = save_edflag;
    else if(OEDIT_TRAN || OEDIT_ROTATE || OEDIT_SCALE)
      edobj = save_edflag;
  }

handled:
  bu_vls_free(&cmd);

  /* event handled here; prevent someone else from handling the event */
  return TCL_RETURN;

not_handled:
  bu_vls_free(&cmd);

  /* let someone else handle the event */
  return TCL_OK;
}
	    
static void
X_statechange( a, b )
int	a, b;
{
	/*
	 *  Based upon new state, possibly do extra stuff,
	 *  including enabling continuous tablet tracking,
	 *  object highlighting
	 */
	switch( b )  {
	case ST_VIEW:
	    /* constant tracking OFF */
	    XdoMotion = 0;
	    break;
	case ST_S_PICK:
	case ST_O_PICK:
	case ST_O_PATH:
	case ST_S_VPICK:
	    /* constant tracking ON */
	    XdoMotion = 1;
	    break;
	case ST_O_EDIT:
	case ST_S_EDIT:
	    /* constant tracking OFF */
	    XdoMotion = 0;
	    break;
	default:
	  Tcl_AppendResult(interp, "X_statechange: unknown state ",
			   state_str[b], "\n", (char *)NULL);
	  break;
	}

	/*X_viewchange( DM_CHGV_REDO, SOLID_NULL );*/
	++dmaflag;
}

static int
X_dm(argc, argv)
int argc;
char *argv[];
{
  struct bu_vls	vls;
  int status;
  char *av[6];
  char xstr[32];
  char ystr[32];

  if( !strcmp( argv[0], "set" ) )  {
    struct bu_vls tmp_vls;

    bu_vls_init(&vls);
    bu_vls_init(&tmp_vls);
    start_catching_output(&tmp_vls);

    if( argc < 2 )  {
      /* Bare set command, print out current settings */
      bu_struct_print("dm_X internal variables", X_vparse, (CONST char *)&((struct x_vars *)dmp->dm_vars)->mvars );
    } else if( argc == 2 ) {
      bu_vls_struct_item_named( &vls, X_vparse, argv[1], (CONST char *)&((struct x_vars *)dmp->dm_vars)->mvars, ',');
      bu_log( "%s\n", bu_vls_addr(&vls) );
    } else {
      bu_vls_printf( &vls, "%s=\"", argv[1] );
      bu_vls_from_argv( &vls, argc-2, argv+2 );
      bu_vls_putc( &vls, '\"' );
      bu_struct_parse( &vls, X_vparse, (char *)&((struct x_vars *)dmp->dm_vars)->mvars );
    }

    bu_vls_free(&vls);

    stop_catching_output(&tmp_vls);
    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
    return TCL_OK;
  }

  if(!strcmp(argv[0], "idle")){
    am_mode = AMM_IDLE;
    scroll_active = 0;
#ifdef DO_RUBBER_BAND
    if(rubber_band_active){
      rubber_band_active = 0;
      dirty = 1;

      if(mged_variables->mouse_behavior == 'r')
	rt_rect_area();
      else if(mged_variables->mouse_behavior == 'z')
	zoom_rect_area();
    }
#endif    

    return TCL_OK;
  }

  if( !strcmp( argv[0], "m" )){
    if( argc < 4){
      Tcl_AppendResult(interp, "dm m: need more parameters\n",
		       "dm m button xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    /* This assumes a 3-button mouse */
    switch(*argv[1]){
    case '1':
      ((struct x_vars *)dmp->dm_vars)->mb_mask = Button1Mask;
      break;
    case '2':
      ((struct x_vars *)dmp->dm_vars)->mb_mask = Button2Mask;
      break;
    case '3':
      ((struct x_vars *)dmp->dm_vars)->mb_mask = Button3Mask;
      break;
    default:
      Tcl_AppendResult(interp, "dm m: bad button value - ", argv[1], "\n", (char *)NULL);
      return TCL_ERROR;
    }

    {
      int x;
      int y;
      int old_orig_gui;
      int stolen = 0;
      fastf_t fx, fy;

      old_orig_gui = mged_variables->orig_gui;

      fx = dm_Xx2Normal(dmp, atoi(argv[2]), 0);
      fy = dm_Xy2Normal(dmp, atoi(argv[3]));
      x = fx * 2047.0;
      y = fy * 2047.0;

      if(mged_variables->faceplate &&
	 mged_variables->orig_gui){
#define        MENUXLIM        (-1250)

#if 0
	if(scroll_active){
	  stolen = 1;
	  goto end;
	}
#endif

	if(x >= MENUXLIM && scroll_select( x, y, 0 )){
	  stolen = 1;
	  goto end;
	}

	if(x < MENUXLIM && mmenu_select( y, 0)){
	  stolen = 1;
	  goto end;
	}
      }

      mged_variables->orig_gui = 0;
      fx = dm_Xx2Normal(dmp, atoi(argv[2]), 1);
      x = fx * 2047.0;

end:
      bu_vls_init(&vls);
      if(mged_variables->mouse_behavior == 'q' && !stolen){
	point_t view_pt;
	point_t model_pt;

	VSET(view_pt, fx, fy, 1.0);
	MAT4X3PNT(model_pt, view2model, view_pt);
	VSCALE(model_pt, model_pt, base2local);
	if(*zclip_ptr)
	  bu_vls_printf(&vls, "nirt %lf %lf %lf",
			model_pt[X], model_pt[Y], model_pt[Z]);
	else
	  bu_vls_printf(&vls, "nirt -b %lf %lf %lf",
			model_pt[X], model_pt[Y], model_pt[Z]);
#ifdef DO_RUBBER_BAND
      }else if((mged_variables->mouse_behavior == 'p' ||
		mged_variables->mouse_behavior == 'r' ||
		mged_variables->mouse_behavior == 'z') && !stolen){
	rubber_band_active = 1;
	rect_x = fx;
	rect_y = fy;
	rect_width = 0.0;
	rect_height = 0.0;

	dirty = 1;
#endif
      }else
	bu_vls_printf(&vls, "M 1 %d %d\n", x, y);

      status = Tcl_Eval(interp, bu_vls_addr(&vls));
      mged_variables->orig_gui = old_orig_gui;
      bu_vls_free(&vls);

      return status;
    }
  }

  if(!strcmp(argv[0], "am")){
#if 0
    scroll_active = 0;
#endif

    if( argc < 4){
      Tcl_AppendResult(interp, "dm am: need more parameters\n",
		       "dm am <r|t|s> xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    ((struct x_vars *)dmp->dm_vars)->omx = atoi(argv[2]);
    ((struct x_vars *)dmp->dm_vars)->omy = atoi(argv[3]);

    switch(*argv[1]){
    case 'r':
      am_mode = AMM_ROT;
      break;
    case 't':
      am_mode = AMM_TRAN;

      if(EDIT_TRAN && mged_variables->transform == 'e'){
	char save_ecoords;
	point_t mouse_view_pos;
	point_t ea_view_pos;
	point_t diff;

	save_ecoords = mged_variables->ecoords;
	mged_variables->ecoords = 'v';

	MAT4X3PNT(ea_view_pos, model2view, e_axes_pos);
	mouse_view_pos[X] = dm_Xx2Normal(dmp, ((struct x_vars *)dmp->dm_vars)->omx, 1);
	mouse_view_pos[Y] = dm_Xy2Normal(dmp, ((struct x_vars *)dmp->dm_vars)->omy);
	mouse_view_pos[Z] = ea_view_pos[Z];
	VSUB2(diff, mouse_view_pos, ea_view_pos);
	VSCALE(diff, diff, Viewscale * base2local);

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "knob aX %lf aY %lf\n", diff[X], diff[Y]);
	(void)Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	mged_variables->ecoords = save_ecoords;
      }

      break;
    case 's':
      if(state == ST_S_EDIT && mged_variables->transform == 'e' &&
	 NEAR_ZERO(acc_sc_sol, (fastf_t)SMALL_FASTF))
	acc_sc_sol = 1.0;
      else if(state == ST_O_EDIT && mged_variables->transform == 'e'){
	edit_absolute_scale = acc_sc_obj - 1.0;
	if(edit_absolute_scale > 0.0)
	  edit_absolute_scale /= 3.0;
      }

      am_mode = AMM_SCALE;
      break;
    default:
      Tcl_AppendResult(interp, "dm am: need more parameters\n",
		       "dm am <r|t|s> xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    return TCL_OK;
  }

  if(!strcmp(argv[0], "adc")){
    fastf_t fx, fy;
    fastf_t td; /* tick distance */
#if 0
    scroll_active = 0;
#endif

    if(argc < 4){
      Tcl_AppendResult(interp, "dm adc: need more parameters\n",
		       "dm adc 1|2|t|d xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    ((struct x_vars *)dmp->dm_vars)->omx = atoi(argv[2]);
    ((struct x_vars *)dmp->dm_vars)->omy = atoi(argv[3]);

    switch(*argv[1]){
    case '1':
      fx = dm_Xx2Normal(dmp, ((struct x_vars *)dmp->dm_vars)->omx, 1) * 2047.0 - dv_xadc;
      fy = dm_Xy2Normal(dmp, ((struct x_vars *)dmp->dm_vars)->omy) * 2047.0 - dv_yadc;
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "adc a1 %lf\n", DEGRAD*atan2(fy, fx));
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);

      am_mode = AMM_ADC_ANG1;
      break;
    case '2':
      fx = dm_Xx2Normal(dmp, ((struct x_vars *)dmp->dm_vars)->omx, 1) * 2047.0 - dv_xadc;
      fy = dm_Xy2Normal(dmp, ((struct x_vars *)dmp->dm_vars)->omy) * 2047.0 - dv_yadc;
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "adc a2 %lf\n", DEGRAD*atan2(fy, fx));
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);

      am_mode = AMM_ADC_ANG2;
      break;
    case 't':
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "adc hv %lf %lf\n",
		    dm_Xx2Normal(dmp, ((struct x_vars *)dmp->dm_vars)->omx, 1) *
		    Viewscale * base2local,
		    dm_Xy2Normal(dmp, ((struct x_vars *)dmp->dm_vars)->omy) *
		    Viewscale * base2local);
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);

      am_mode = AMM_ADC_TRAN;
      break;
    case 'd':
      fx = (dm_Xx2Normal(dmp, ((struct x_vars *)dmp->dm_vars)->omx, 1) * 2047.0 -
	    dv_xadc) * Viewscale * base2local / 2047.0;
      fy = (dm_Xy2Normal(dmp, ((struct x_vars *)dmp->dm_vars)->omy) * 2047.0 -
	    dv_yadc) * Viewscale * base2local / 2047.0;

      td = sqrt(fx * fx + fy * fy);
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "adc dst %lf\n", td);
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);

      am_mode = AMM_ADC_DIST;
      break;
    default:
      Tcl_AppendResult(interp, "dm adc: unrecognized parameter - ", argv[1],
		       "\ndm adc 1|2|t|d xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    return TCL_OK;
  }

  if(!strcmp(argv[0], "con")){
#if 0
    scroll_active = 0;
#endif

    if(argc < 5){
      Tcl_AppendResult(interp, "dm con: need more parameters\n",
		       "dm con r|t|s x|y|z xpos ypos\n",
		       "dm con a x|y|1|2|d xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    ((struct x_vars *)dmp->dm_vars)->omx = atoi(argv[3]);
    ((struct x_vars *)dmp->dm_vars)->omy = atoi(argv[4]);

    switch(*argv[1]){
    case 'a':
      switch(*argv[2]){
      case 'x':
	am_mode = AMM_CON_XADC;
	break;
      case 'y':
	am_mode = AMM_CON_YADC;
	break;
      case '1':
	am_mode = AMM_CON_ANG1;
	break;
      case '2':
	am_mode = AMM_CON_ANG2;
	break;
      case 'd':
	am_mode = AMM_CON_DIST;
	break;
      default:
	Tcl_AppendResult(interp, "dm con: unrecognized parameter - ", argv[2],
			 "\ndm con a x|y|1|2|d xpos ypos\n", (char *)NULL);
      }
      break;
    case 'r':
      switch(*argv[2]){
      case 'x':
	am_mode = AMM_CON_ROT_X;
	break;
      case 'y':
	am_mode = AMM_CON_ROT_Y;
	break;
      case 'z':
	am_mode = AMM_CON_ROT_Z;
	break;
      default:
	Tcl_AppendResult(interp, "dm con: unrecognized parameter - ", argv[2],
			 "\ndm con r|t|s x|y|z xpos ypos\n", (char *)NULL);
	return TCL_ERROR;
      }
      break;
    case 't':
      switch(*argv[2]){
      case 'x':
	am_mode = AMM_CON_TRAN_X;
	break;
      case 'y':
	am_mode = AMM_CON_TRAN_Y;
	break;
      case 'z':
	am_mode = AMM_CON_TRAN_Z;
	break;
      default:
	Tcl_AppendResult(interp, "dm con: unrecognized parameter - ", argv[2],
			 "\ndm con r|t|s x|y|z xpos ypos\n", (char *)NULL);
	return TCL_ERROR;
      }
      break;
    case 's':
      switch(*argv[2]){
      case 'x':
	if(state == ST_S_EDIT && mged_variables->transform == 'e' &&
	   NEAR_ZERO(acc_sc_sol, (fastf_t)SMALL_FASTF))
	  acc_sc_sol = 1.0;
	else if(state == ST_O_EDIT && mged_variables->transform == 'e'){
	  edit_absolute_scale = acc_sc[0] - 1.0;
	  if(edit_absolute_scale > 0.0)
	    edit_absolute_scale /= 3.0;
	}

	am_mode = AMM_CON_SCALE_X;
	break;
      case 'y':
	if(state == ST_S_EDIT && mged_variables->transform == 'e' &&
	   NEAR_ZERO(acc_sc_sol, (fastf_t)SMALL_FASTF))
	  acc_sc_sol = 1.0;
	else if(state == ST_O_EDIT && mged_variables->transform == 'e'){
	  edit_absolute_scale = acc_sc[1] - 1.0;
	  if(edit_absolute_scale > 0.0)
	    edit_absolute_scale /= 3.0;
	}

	am_mode = AMM_CON_SCALE_Y;
	break;
      case 'z':
	if(state == ST_S_EDIT && mged_variables->transform == 'e' &&
	   NEAR_ZERO(acc_sc_sol, (fastf_t)SMALL_FASTF))
	  acc_sc_sol = 1.0;
	else if(state == ST_O_EDIT && mged_variables->transform == 'e'){
	  edit_absolute_scale = acc_sc[2] - 1.0;
	  if(edit_absolute_scale > 0.0)
	    edit_absolute_scale /= 3.0;
	}

	am_mode = AMM_CON_SCALE_Z;
	break;
      default:
	Tcl_AppendResult(interp, "dm con: unrecognized parameter - ", argv[2],
			 "\ndm con r|t|s x|y|z xpos ypos\n", (char *)NULL);
	return TCL_ERROR;
      }

      break;
    default:
      Tcl_AppendResult(interp, "dm con: unrecognized parameter - ", argv[1],
		       "\ndm con r|t|s x|y|z xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    return TCL_OK;
  }

  if( !strcmp( argv[0], "size" )){
    int width, height;

    /* get the window size */
    if( argc == 1 ){
      struct bu_vls vls;

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d %d", dmp->dm_width, dmp->dm_height);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }

    /* set the window size */
    if( argc == 3 ){
      width = atoi( argv[1] );
      height = atoi( argv[2] );

      Tk_GeometryRequest(((struct x_vars *)dmp->dm_vars)->xtkwin, width, height);

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "Usage: dm size [width height]\n", (char *)NULL);
    return TCL_ERROR;
  }

  Tcl_AppendResult(interp, "dm: bad command - ", argv[0], "\n", (char *)NULL);
  return TCL_ERROR;
}

#if IR_KNOBS
void
X_dbtext(str)
{
  Tcl_AppendResult(interp, "dm-X: You pressed Help key and '",
		   str, "'\n", (char *)NULL);
}
#endif

static void
dirty_hook()
{
  dirty = 1;
}

static void
set_knob_offset()
{
  int i;

  for(i = 0; i < 8; ++i)
    ((struct x_vars *)dmp->dm_vars)->knobs[i] = 0;
}

