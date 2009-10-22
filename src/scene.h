#ifndef __SCENE_H__
#define __SCENE_H__

#include "mesh.h"
// This needs some more brain-storming but for now a scene contains of a list of drawable objects
// (just ZMeshes for now), and a list of ZPosables, which are just small wrappers around the
// drawable objects that add information about how they are oriented in the scene. ZPosables also
// facilitate instancing of objects since more than one ZPosable can have pointers to the same
// drawable object.

// TODO: Another problem is trying to load a scene when there is no window open (= active opengl
// context). I could work around this by deferring actually loading the meshes etc. until I attempt
// to draw the scene. Not really sure if that is the best way to go though..


#define Z_MAX_LIGHTS 8 // XXX: Should probably check max lights supported by OpenGL instead.

#define Z_POSABLE_STATICMESH 1



typedef struct ZPosable
{
    float position[3];
    float rot_x;
    float rot_y;
    float rot_z;

    unsigned int type;

    // Pointer to the object being posed
    union
    {
        ZMesh *mesh; // STATICMESH

    } subject;

    struct ZPosable *next;

} ZPosable;



typedef struct ZLight
{
    float position[4];
    float ambient_color[4];
    float diffuse_color[4];
    float specular_color[4];

} ZLight;



typedef struct ZScene
{
    int is_resident;

    char name[Z_RESOURCE_NAME_SIZE];

    float background_color[4];
    float ambient_color[4];

    float sun_direction[3];
    float sun_color[3];

    ZLight *point_lights;

    ZCamera camera;

    ZPosable *posables;
    ZPosable *sky_posables;

} ZScene;


extern unsigned int sceneload_count;

ZScene *zLoadScene(const char *name);

void zSceneInfo(ZScene *scene);

void zUpdateScene(ZScene *scene, float frametime);

void zDrawScene(ZScene *scene);

void zAddPosableToScene(ZScene *scene, ZPosable *posable, int sky);

void zAddMeshToScene(ZScene *scene, char *name, int sky);

void zMakeSceneNonResident(ZScene *scene);

void zDeleteScene(ZScene *scene);


#endif // __SCENE_H__
