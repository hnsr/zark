#ifdef WIN32
#define _USE_MATH_DEFINES
#endif

#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <GL/glew.h>
#include <string.h>

#include "common.h"


// Update camera rotation matrix from forward/up vectors.
static void zCameraUpdateRotation(ZCamera *cam)
{
    GLfloat *m = cam->rotation;
    ZVec3 right, back;

    // The camera position can be used to derive the translation by inverting it (since the position
    // as-is would be the World->Camera translation, while I need the Camera->World transform).
    //
    // The camera's up/forward vectors can be used to derive the transformed basis vectors for the
    // rotation matrix, which is then transposed to get the inverse (same reason as bove).

    // I need the back vector for the Z axis, because the OpenGL camera defaults to looking down
    // the negative Z-axis.
    back.x = -(cam->forward.x);
    back.y = -(cam->forward.y);
    back.z = -(cam->forward.z);

    // Get the right (X-axis) vector.
    right = zCross3(&(cam->up), &back);

    // Set up the Camera->World rotation matrix.
    m[0] = right.x;   m[4] = right.y;   m[8]  = right.z;   m[12] = 0.0f;
    m[1] = cam->up.x; m[5] = cam->up.y; m[9]  = cam->up.z; m[13] = 0.0f;
    m[2] = back.x;    m[6] = back.y;    m[10] = back.z;    m[14] = 0.0f;
    m[3] = 0.0f;      m[7] = 0.0f;      m[11] = 0.0f;      m[15] = 1.0f;
}


// Rotate one or two (if v2 is not NULL) vectors about another vector..
static void zRotateVAV(ZVec3 *v1, ZVec3 *v2, ZVec3 *vrot, float angle)
{
    // For a truly arbritary axis:      For an axis on plane y=0:
    // ( tx^2+c txy-sz txz+sy )         ( tx^2+c  -sz     txz )
    // | txy+sz ty^2+c tyz-sx |         |     sz    c     -sx |
    // ( txz-sy tyz+sx tz^2+c )         (    txz   sx  tz^2+c )
    //
    // (x,y,z) = axis of rotation
    // c       = cos(r)
    // s       = sin(r)
    // t       = 1 - cos(r)

    float m[9], c, s, t, x, y, z;
    float r = DEG_TO_RAD(angle);

    if (angle == 0.0f) return;

    c = cosf(r);
    s = sinf(r);
    t = 1.0f - cosf(r);

    x = vrot->x;
    y = vrot->y;
    z = vrot->z;

    m[0] = (t*x*x)+c;   m[3] = (t*x*y)-s*z; m[6] = (t*x*z)+s*y;
    m[1] = (t*x*y)+s*z; m[4] = (t*y*y)+c;   m[7] = (t*y*z)-s*x;
    m[2] = (t*x*z)-s*y; m[5] = (t*y*z)+s*x; m[8] = (t*z*z)+c;

    if (v1) zTransform3Vec3(m, v1);
    if (v2) zTransform3Vec3(m, v2);
}


// Initialize camera (set default position/orientation).
void zCameraInit(ZCamera *camera)
{
    memset(camera, '\0', sizeof(camera));
    zCameraSetPosition(camera, 0.0f, 0.0f, 0.0f);
    zCameraSetForward(camera, 0.0f, 0.0f, -1.0f);
    zCameraSetUp(camera, 0.0f, 1.0f, 0.0f);
    camera->fov = 60.0f;
    zCameraUpdateRotation(camera);
}



// Explicitly set camera position.
void zCameraSetPosition(ZCamera *camera, float x, float y, float z)
{
    camera->position.x = x;
    camera->position.y = y;
    camera->position.z = z;
}



// Explicitly set forward vector.
void zCameraSetForward(ZCamera *camera, float x, float y, float z)
{
    camera->forward.x = x;
    camera->forward.y = y;
    camera->forward.z = z;
    zNormalize3(&(camera->forward));
    zCameraUpdateRotation(camera);
}



// Explicitly set up vector.
void zCameraSetUp(ZCamera *camera, float x, float y, float z)
{
    camera->up.x = x;
    camera->up.y = y;
    camera->up.z = z;
    zNormalize3(&(camera->up));
    zCameraUpdateRotation(camera);
}



// Rotate camera forward vector around up vector.
void zCameraYaw(ZCamera *camera, float angle)
{
    zRotateVAV(&camera->forward, NULL, &camera->up, -angle);
    zCameraUpdateRotation(camera);
}



// Rotate camera forward/up vectors around right vector.
void zCameraPitch(ZCamera *camera, float angle)
{
    ZVec3 right = zCross3(&camera->forward, &camera->up);
    zRotateVAV(&camera->up, &camera->forward, &right, angle);
    zCameraUpdateRotation(camera);
}


// Rotate camera up vectors around forward vector.
void zCameraRoll(ZCamera *camera, float angle)
{
    zRotateVAV(&camera->up, NULL, &camera->forward, angle);
    zCameraUpdateRotation(camera);
}


// Update camera position and orientation based on controller state.
void zCameraUpdate(ZCamera *camera, float tdelta)
{
    unsigned int flags = controller.update_flags;

    // Only update if a flags flag has been set,
    if (flags) {

        float tmp;

        // Lots of stuff can be skipped if tdelta == 0.0f (which can be very often with an FPS above
        // 1000).
        if (tdelta > 0.0f) {

            ZVec3 disp;
            ZVec3 left = zCross3(&(camera->up), &(camera->forward));

            // Keep adding the camera's right/forward vectors to disp, then normalize and add to
            // position at the end.
            disp.x = disp.y = disp.z = 0.0f;

            //if (flags & Z_CONTROL_UP)      disp.y += 1.0f;
            //if (flags & Z_CONTROL_DOWN)    disp.y -= 1.0f;
            if (flags & Z_CONTROL_UP)      zAddVec3(&disp, &camera->up);
            if (flags & Z_CONTROL_DOWN)    zSubtractVec3(&disp, &camera->up);
            if (flags & Z_CONTROL_FORWARD) zAddVec3(&disp, &camera->forward);
            if (flags & Z_CONTROL_BACK)    zSubtractVec3(&disp, &camera->forward);
            if (flags & Z_CONTROL_LEFT)    zAddVec3(&disp, &left);
            if (flags & Z_CONTROL_RIGHT)   zSubtractVec3(&disp, &left);

            // Clamp displacement length to 1.0f..
            if (zLength3(&disp) > 1.0f) zNormalize3(&disp);

            zScaleVec3(&disp, tdelta*movespeed);
            zAddVec3(&(camera->position), &disp);

            if (flags & Z_CONTROL_AIM_UP)    zCameraPitch(camera, 90.0f*tdelta);
            if (flags & Z_CONTROL_AIM_DOWN)  zCameraPitch(camera, -90.0f*tdelta);
            if (flags & Z_CONTROL_AIM_LEFT)  zCameraYaw(camera, -90.0f*tdelta);
            if (flags & Z_CONTROL_AIM_RIGHT) zCameraYaw(camera, 90.0f*tdelta);
            if (flags & Z_CONTROL_ROLL_LEFT)  zCameraRoll(camera, -90.0f*tdelta);
            if (flags & Z_CONTROL_ROLL_RIGHT) zCameraRoll(camera, 90.0f*tdelta);
        }

        // Quick hack to adjust pitch/yaw sensitivity depending on field of view, may need
        // improvement..
        tmp = camera->fov/180.0f;

        if (flags & Z_CONTROL_AIM) {

            // No need to take into account tdelta - the faster the framerate, the smaller the mouse
            // motions..
            zCameraPitch(camera, tmp*controller.pitch_delta);
            zCameraYaw(camera, tmp*controller.yaw_delta);
        }

        if (flags & Z_CONTROL_ROLL) {

            zCameraRoll(camera, tmp*controller.yaw_delta);
        }

        if (flags & Z_CONTROL_ZOOM ) {

            camera->fov += tmp*controller.pitch_delta;

            if ( camera->fov > 170.0f ) camera->fov = 170.0f;
            else if ( camera->fov < 0.001f ) camera->fov = 0.001f;
        }

        // FIXME: without this the vectors get borked after a number of transforms... maybe
        // orthogonalize instead? anyway this seems to work for now...
        zNormalize3(&(camera->up));
        zNormalize3(&(camera->forward));

        zCameraUpdateRotation(camera);
    }
}



// Apply camera projection matrix.
void zCameraApplyProjection(ZCamera *camera)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();


    //ratio = (float) viewport_width / (float) viewport_height;
    //glOrtho(-1.0*ratio, 1.0*ratio, -1.0, 1.0, 0.1, 100.0);
    gluPerspective(camera->fov, (float) viewport_width / (float) viewport_height,
        r_nearplane, r_farplane);
    glMatrixMode(GL_MODELVIEW);
}



// Apply camera viewing matrix.
void zCameraApplyViewing(ZCamera *cam, int rotate_only)
{
    glLoadIdentity();

    // I need to translate first, and then rotate, but since OpenGL post-multiplies, I specify in
    // reverse order.
    // Rotate
    glMultMatrixf(cam->rotation);

    // Translate
    if (!rotate_only) glTranslatef(-(cam->position.x), -(cam->position.y), -(cam->position.z));
}


