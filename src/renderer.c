#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common.h"


// Current dimensions of the OpenGL viewport.
int viewport_width;
int viewport_height;

int renderer_active;



// Print out extension names on a seperate line.
static void zPrintExtensions(const unsigned char *s)
{
    zPrint("    ");

    // Copy chars to output, if a space is encountered, output a newling and 4 spaces.
    while (*s != '\0') {

        if (*s == ' ' && *(s+1) != '\0') {
            zPrint("\n    ");
        } else {
            putchar(*s);
        }
        s++;
    }
    zPrint("\n");

    return;
}




// Handy macro for printing a bunch of OpenGL limits..
#define zPrintGLInteger(name) glGetIntegerv((name), i);\
                              zPrint("    "#name": %d\n", *i);

// Print some information on the OpenGL implementation.
void zRendererInfo(void)
{
    const unsigned char *s;
    GLint i[4];

    if (!renderer_active) return;

    zPrint("Dumping renderer info:\n");

    s = glGetString(GL_VENDOR);
    zPrint("  vendor:           %s\n", s);
    s = glGetString(GL_RENDERER);
    zPrint("  renderer:         %s\n", s);
    s = glGetString(GL_VERSION);
    zPrint("  version:          %s\n", s);
    s = glGetString(GL_SHADING_LANGUAGE_VERSION);
    zPrint("  shading language: %s\n", s);
    s = glGetString(GL_EXTENSIONS);
    zPrint("  extensions: \n");
    zPrintExtensions(s);

    zPrint("  limits:\n");
    zPrintGLInteger(GL_MAX_3D_TEXTURE_SIZE);
    zPrintGLInteger(GL_MAX_CUBE_MAP_TEXTURE_SIZE);
    zPrintGLInteger(GL_MAX_ELEMENTS_INDICES);
    zPrintGLInteger(GL_MAX_ELEMENTS_VERTICES);
    zPrintGLInteger(GL_MAX_LIGHTS);
    zPrintGLInteger(GL_MAX_TEXTURE_SIZE);
    zPrintGLInteger(GL_MAX_TEXTURE_UNITS);

    zPrint("  shader limits:\n");
    zPrintGLInteger(GL_MAX_DRAW_BUFFERS);
    zPrintGLInteger(GL_MAX_TEXTURE_COORDS);
    zPrintGLInteger(GL_MAX_TEXTURE_IMAGE_UNITS);
    zPrintGLInteger(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
    zPrintGLInteger(GL_MAX_VARYING_FLOATS);
    zPrintGLInteger(GL_MAX_VERTEX_ATTRIBS);
    zPrintGLInteger(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS);
    zPrintGLInteger(GL_MAX_VERTEX_UNIFORM_COMPONENTS);
    zPrintGLInteger(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS);
}



// Set initial renderer state and do any other renderer-dependent initialization.
void zRendererInit(void)
{
    assert(!renderer_active);

    zSwapInterval(r_swapinterval);
    zReshapeViewport();
    glEnable(GL_CULL_FACE);

    zMeshInit();
    zMaterialInit();
    zShaderInit();
    zTextRenderInit();

    renderer_active = 1;
}



// Misc cleanup to be done before the window is closed and the render context destroyed.
void zRendererDeinit(void)
{
    assert(renderer_active);

    // Mark current scene non-resident.
    if (scene) zMakeSceneNonResident(scene);

    zMeshDeinit();
    zMaterialDeinit();
    zShaderDeinit();
    zTextRenderDeinit();

    // Release currently pressed keys. I used to skip running key bindings here for some reason (I
    // forgot why :/), but this borked mouse handling, so I don't skip them anymore.
    zReleaseKeys(0);

    renderer_active = 0;
}



// Check wether if we have the right OpenGL version and extensions, in which case TRUE is returned,
// else FALSE.
int zCheckOpenGLSupport(void)
{
    // FIXME: Should probably check for OpenGL 2.0 or maybe 2.1 now that I am using shaders?
    if (!GLEW_VERSION_1_4) {
        zError("OpenGL 1.4 is not supported");
        return FALSE;
    }

    if (!GLEW_ARB_vertex_buffer_object) {
        zError("ARB_vertex_buffer_object is not supported.");
        return FALSE;
    }

    return TRUE;
}



// See if there were any rendering errors.
void zCheckRendererError(void)
{
    GLenum error;
    const GLubyte *error_str;

    if ( (error = glGetError()) != GL_NO_ERROR ) {
        error_str = gluErrorString(error);
        zWarning("An OpenGL error occured, \"%s\".", error_str);
    }
}



void zReshapeViewport(void)
{
    glViewport(0, 0, (GLsizei) viewport_width, (GLsizei) viewport_height);
}



// Setup projection/viewing transformations for GUI.
void zApplyGUITransforms(void)
{
    // First set modelview transform to identity (I assume current matrix mode is modelview).
    glLoadIdentity();

    // Setup orthographic projection transformation
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0f, viewport_width, 0.0f, viewport_height, 0.0f, 10.0f);
    glMatrixMode(GL_MODELVIEW);
}

