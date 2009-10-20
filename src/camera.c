#ifdef WIN32
#define _USE_MATH_DEFINES
#endif

#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <GL/glew.h>
#include <string.h>

#include "common.h"

// FIXME: Calling CameraUpdateViewTransform all over the place isn't really a good idea, I will have
// to redo the camera code at some point.. Maybe I can set a dirty flag instead and check for it in
// zCameraApplyViewing?

// Update view_transform matrix.
static void zCameraUpdateViewTransform(ZCamera *cam)
{
    GLfloat *m = cam->view_transform;
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



// Initialize camera (set default position/orientation).
void zCameraInit(ZCamera *camera)
{
    memset(camera, '\0', sizeof(camera));
    zCameraSetPosition(camera, 0.0f, 0.0f, 0.0f);
    zCameraSetForward(camera, 0.0f, 0.0f, -1.0f);
    zCameraSetUp(camera, 0.0f, 1.0f, 0.0f);
    camera->fov = 60.0f;
    zCameraUpdateViewTransform(camera);
}



// Explicitly set camera position.
void zCameraSetPosition(ZCamera *camera, float x, float y, float z)
{
    camera->position.x = x;
    camera->position.y = y;
    camera->position.z = z;
    zCameraUpdateViewTransform(camera);
}



// Explicitly set forward vector.
void zCameraSetForward(ZCamera *camera, float x, float y, float z)
{
    camera->forward.x = x;
    camera->forward.y = y;
    camera->forward.z = z;
    zNormalize3(&(camera->forward));
    zCameraUpdateViewTransform(camera);
}



// Explicitly set up vector.
void zCameraSetUp(ZCamera *camera, float x, float y, float z)
{
    camera->up.x = x;
    camera->up.y = y;
    camera->up.z = z;
    zNormalize3(&(camera->up));
    zCameraUpdateViewTransform(camera);
}



// Rotate camera forward/up vectors around world-up vector.
void zCameraYaw(ZCamera *camera, float angle)
{
    float m[9];
    float r = DEG_TO_RAD(-angle); // Invert angle since positive yaw is to the right.

    m[0] = cosf(r);   m[3] = 0.0f;  m[6] = sinf(r);
    m[1] = 0.0f;      m[4] = 1.0f;  m[7] = 0.0f;
    m[2] = -sinf(r);  m[5] = 0.0f;  m[8] = cosf(r);

    zTransform3Vec3(m, &(camera->forward));
    zTransform3Vec3(m, &(camera->up));
    zCameraUpdateViewTransform(camera);
}



// Rotate camera forward/up vectors around right vector.
void zCameraPitch(ZCamera *camera, float angle)
{
    // Need to rotate up/forward vectors about an arbritary axis (the right vector). Actually the
    // 'right' vector is on the y=0 plane so I can simplify this a little..
    //
    // For a truly arbritary axis:      For an axis on plane y=0:
    // ( tx^2+c txy-sz txz+sy )         ( tx^2+c  -sz     txz )
    // | txy+sz ty^2+c tyz-sx |         |     sz    c     -sx |
    // ( txz-sy tyz+sx tz^2+c )         (    txz   sx  tz^2+c )
    //
    // (x,y,z) = axis of rotation
    // c       = cos(r)
    // s       = sin(r)
    // t       = 1 - cos(r)

    ZVec3 right = zCross3(&(camera->forward), &(camera->up));

    float m[9], c, s, t, x, z;
    float r = DEG_TO_RAD(angle);

    if (angle == 0.0f) return;

    c = cosf(r);
    s = sinf(r);
    t = 1.0f - cosf(r);
    x = right.x;
    z = right.z;

    // Unsimplified version.. keeping this around for if I ever want to allow any right vector.
    //m[0] = (t*x*x)+c;   m[3] = (t*x*y)-s*z; m[6] = (t*x*z)+s*y;
    //m[1] = (t*x*y)+s*z; m[4] = (t*y*y)+c;   m[7] = (t*y*z)-s*x;
    //m[2] = (t*x*z)-s*y; m[5] = (t*y*z)+s*x; m[8] = (t*z*z)+c;

    m[0] = (t*x*x)+c; m[3] = -s*z; m[6] = (t*x*z);
    m[1] = s*z;       m[4] = c;    m[7] = -s*x;
    m[2] = (t*x*z);   m[5] = s*x;  m[8] = (t*z*z)+c;

    zTransform3Vec3(m, &(camera->forward));
    zTransform3Vec3(m, &(camera->up));

    // Make sure the world never turns up-side down: If the up vector points downward, project it to
    // the y=0 plane, make forward vector perpendicular again and re-normalize.

    //assert(angle < 90.0f); // This code would barf if the rotation was bigger than 90 degrees.

    if (camera->up.y < 0.0f) {
        camera->up.y = 0.0f;
        camera->forward = zCross3(&(camera->up), &right);
        zNormalize3(&(camera->up));
        zNormalize3(&(camera->forward));
    }
    zCameraUpdateViewTransform(camera);
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

            // Up/down is along world y axis.
            if (flags & Z_CONTROL_UP)      disp.y += 1.0f;
            if (flags & Z_CONTROL_DOWN)    disp.y -= 1.0f;

            if (flags & Z_CONTROL_FORWARD) zAddVec3(&disp, &(camera->forward));
            if (flags & Z_CONTROL_BACK)    zSubtractVec3(&disp, &(camera->forward));
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

        if (flags & Z_CONTROL_ZOOM ) {

            camera->fov += tmp*controller.pitch_delta;

            if ( camera->fov > 170.0f ) camera->fov = 170.0f;
            else if ( camera->fov < 0.001f ) camera->fov = 0.001f;
        }

        zCameraUpdateViewTransform(camera);
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
    glMultMatrixf(cam->view_transform);

    // Translate
    if (!rotate_only) glTranslatef(-(cam->position.x), -(cam->position.y), -(cam->position.z));
}


