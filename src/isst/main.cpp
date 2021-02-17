/*                        M A I N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file main.cpp
 *
 * ISST
 *
 */

#include <iostream>

#include "main_window.h"
#include "isstapp.h"

#include "bu/app.h"
#include "bu/log.h"
#include "brlcad_version.h"

int main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    ISSTApp app(argc, argv);
    ISST_MainWindow mainWin;

    argc--; argv++;

    if (argc <= 1) {
	bu_exit(1, "isst file.g obj1 [obj2 ...]");
    }

    if (argc > 1) {
	const char *filename = argv[0];
	argc--; argv++;
	if (app.load_g(filename, argc, (const char **)argv)) {
	    bu_exit(1, "%s%s%s\n", "Error: opening ", filename, " failed.");
	}
    }

    // TODO - this needs to be a setting that is saved and restored
    mainWin.resize(1100, 800);
    mainWin.canvas->camera.w = 1100;
    mainWin.canvas->camera.h = 800;
    mainWin.canvas->tie = app.tie;

    mainWin.show();


    mainWin.canvas->tile.size_x = 1100;
    mainWin.canvas->tile.size_y = 800;



    //mainWin.canvas->makeCurrent();

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    if (!f)
	bu_log("no current functions\n");

    TIENET_BUFFER_SIZE(mainWin.canvas->buffer_image, (uint32_t)(3 * mainWin.canvas->camera.w * mainWin.canvas->camera.h));

    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    mainWin.canvas->texdata = malloc(mainWin.canvas->camera.w * mainWin.canvas->camera.h * 3);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, mainWin.canvas->camera.w, mainWin.canvas->camera.h, 0, GL_RGB, GL_UNSIGNED_BYTE, mainWin.canvas->texdata);
    glDisable(GL_LIGHTING);
    glViewport(0,0, mainWin.canvas->camera.w, mainWin.canvas->camera.h);
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho(0, mainWin.canvas->camera.w, mainWin.canvas->camera.h, 0, -1, 1);
    glMatrixMode (GL_MODELVIEW);

    glClear(GL_COLOR_BUFFER_BIT);

    mainWin.canvas->buffer_image.ind = 0;

    VSETALL(mainWin.canvas->camera.pos, app.tie->radius);
    VMOVE(mainWin.canvas->camera.focus, app.tie->mid);

    render_phong_init(&mainWin.canvas->camera.render, NULL);

    render_camera_prep(&mainWin.canvas->camera);
    render_camera_render(&mainWin.canvas->camera, app.tie, &mainWin.canvas->tile, &mainWin.canvas->buffer_image);

    glClear(GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glColor3f(1,1,1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, mainWin.canvas->texid);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mainWin.canvas->camera.w, mainWin.canvas->camera.h, GL_RGB, GL_UNSIGNED_BYTE, mainWin.canvas->buffer_image.data + sizeof(camera_tile_t));
    glBegin(GL_TRIANGLE_STRIP);

    glTexCoord2d(0, 0); glVertex3f(0, 0, 0);
    glTexCoord2d(0, 1); glVertex3f(0, mainWin.canvas->camera.h, 0);
    glTexCoord2d(1, 0); glVertex3f(mainWin.canvas->camera.w, 0, 0);
    glTexCoord2d(1, 1); glVertex3f(mainWin.canvas->camera.w, mainWin.canvas->camera.h, 0);

    glEnd();
    glFlush();

    mainWin.canvas->update();
    return app.exec();
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
