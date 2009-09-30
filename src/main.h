#ifndef __MAIN_H__
#define __MAIN_H__

#include "camera.h"
#include "scene.h"

extern ZCamera camera;

extern int running;

extern unsigned int frame_count;

extern ZScene *scene;

int zMain(int argc, char **argv);


#endif
