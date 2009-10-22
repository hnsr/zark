#include <GL/glew.h>
#include <IL/il.h>
#include <IL/ilu.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "common.h"


int running = 1;

unsigned int frame_count;

ZScene *scene;



// TODO: Move to new gui.c at some point
static void zDrawGUI(void)
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    if (glUseProgram) glUseProgram(0);

    zApplyGUITransforms();
    zDrawConsole();
}



// Clear buffers, draw scene, draw GUI.
static void zDrawFrame(void)
{
    frame_count++;

    // Clear buffers, only clear color buffer if r_clear is set.
    glDepthMask(GL_TRUE);
    if (r_clearcolor)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
    else
        glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Draw active scene if there is one
    if (scene) zDrawScene(scene);

    // And finally draw the GUI.
    zDrawGUI();

    // Periodic check for OpenGL errors, just in-case..
    zCheckRendererError();

    // Swap front/back buffers.
    zSwapBuffers();
}



int zMain(int argc, char** argv)
{
    float frame_time;

    zPrint("%s starting up...\n", PACKAGE_STRING);

    ilInit();
    iluInit();

    zLoadMaterials();
    zLoadConfig();
    zLoadKeyBindings();
    // FIXME FIXME zLoadCommands(Z_FILE_STARTUP);

    zOpenWindow(r_winwidth, r_winheight);

    while (running) {

        frame_time = zFrameTime()*0.001f;

        zProcessEvents();

        if (scene) zUpdateScene(scene, frame_time);

        zDrawFrame();
    }

    zPrint("Average FPS was %.2f.\n", ((float)frame_count) / (zGetTimeMS()*0.001f));

    if (scene) {
        zDeleteScene(scene);
        scene = NULL;
    }

    zSaveConfig();
    zSaveKeyBindings();

    zCloseWindow();
    zShutdown();

    return EXIT_SUCCESS;
}

