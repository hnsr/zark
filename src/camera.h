#ifndef __CAMERA_H__
#define __CAMERA_H__

#include "zmath.h"


typedef struct ZCamera
{
    ZVec3 position;
    ZVec3 forward;
    ZVec3 up;

    float fov;

    float rotation[16];

} ZCamera;


void zCameraInit(ZCamera *camera);

void zCameraSetPosition(ZCamera *camera, float x, float y, float z);

void zCameraSetForward(ZCamera *camera, float x, float y, float z);

void zCameraSetUp(ZCamera *camera, float x, float y, float z);


void zCameraYaw(ZCamera *camera, float angle);

void zCameraPitch(ZCamera *camera, float angle);

void zCameraRoll(ZCamera *camera, float angle);


void zCameraUpdate(ZCamera *camera, float tdelta);

void zCameraApplyProjection(ZCamera *camera);

void zCameraApplyViewing(ZCamera *camera, int rotate_only);


#endif
