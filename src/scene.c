#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


#include "common.h"

unsigned int sceneload_count;

// Making a scene resident involves setting OpenGL state (i.e. set up the OpenGL lights), usually
// done once after load or after the OpenGL context has been destroyed.
static void zMakeSceneResident(ZScene *scene)
{
    scene->is_resident = 1;

    // OpenGL state for scene.
    glEnable(GL_LIGHTING);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, scene->ambient_color);

    /* Variable lights disabled for now until I figure out how to organise them in a scene.
    glEnable(GL_LIGHT0+i);
    glLightfv(GL_LIGHT0+i, GL_AMBIENT,  scene->lights[i].ambient_color);
    glLightfv(GL_LIGHT0+i, GL_DIFFUSE,  scene->lights[i].diffuse_color);
    glLightfv(GL_LIGHT0+i, GL_SPECULAR, scene->lights[i].specular_color);
    */

    glClearColor(scene->background_color[0], scene->background_color[1],
                 scene->background_color[2], scene->background_color[3]);
}



// Load a scene, this should be loaded from file at some point.
ZScene *zLoadScene(const char *name)
{
    ZScene *scene = malloc(sizeof(ZScene));

    if (!scene) {
        zError("Failed to allocate memory for scene.");
        return NULL;
    }

    memset(scene, '\0', sizeof(ZScene));

    strcat(scene->name, name);

    zSetFloat3(scene->sun_direction, 1.0f, 0.0f, -0.7f);
    zSetFloat4(scene->background_color,  0.0f,  0.0f,  0.0f, 1.0f);

    zSetFloat4(scene->ambient_color, 0.14f, 0.2f, 0.06f, 1.0f);
    zSetFloat3(scene->sun_color,     0.7f,  1.0f, 0.3f);
    //zSetFloat4(scene->ambient_color, 0.1f, 0.1f, 0.1f, 1.0f);
    //zSetFloat3(scene->sun_color,     1.0f, 1.0f, 1.0f);

    zCameraInit(&scene->camera);

    sceneload_count++;

    return scene;
}




// Return pointer to statically allocated string with a short one-line description for pos.
static const char *zPosableInfo(ZPosable *pos)
{
    static char posinfo[Z_RESOURCE_NAME_SIZE+200];
    posinfo[0] = '\0';

    switch (pos->type) {
        case Z_POSABLE_STATICMESH:
            snprintf(posinfo, Z_RESOURCE_NAME_SIZE+199, "static mesh \"%s\"",
                pos->subject.mesh->name);
            break;
        default:
            strcat(posinfo, "unknown posable type");
    }

    return posinfo;
}



// Print some info on scene.
void zSceneInfo(ZScene *scene)
{
    int i;
    ZPosable *postmp;

    assert(scene);

    zPrint("Dumping info on scene \"%s\":\n", scene->name);

    // Display posables.
    postmp = scene->posables;
    for (i = 0; postmp; postmp = postmp->next, i++) {
        zPrint("  posable %d: %s\n", i, zPosableInfo(postmp));
    }

    zPrint("\n");
}



// Update scene data.
void zUpdateScene(ZScene *scene, float frametime)
{
    zCameraUpdate(&scene->camera, frametime);
}



// Draw XYZ axis, not sure where this really belongs..
static void zDrawAxis(void)
{
    glColor3f(1.0f, 0.0f, 0.0f);
    zDrawVec3f(1.0f, 0.0f, 0.0f);
    glColor3f(0.0f, 1.0f, 0.0f);
    zDrawVec3f(0.0f, 1.0f, 0.0f);
    glColor3f(0.0f, 0.0f, 1.0f);
    zDrawVec3f(0.0f, 0.0f, 1.0f);
}



// Draw the entire scene.
void zDrawScene(ZScene *scene)
{
    //unsigned int i;
    ZPosable *cur_pos;

    if (!scene->is_resident) zMakeSceneResident(scene);

    // Set initial state for drawing the scene.
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);

    zResetMaterialState(); // In case OpenGL material state was clobbered

    glDepthRange(0.0, 1.0-r_skydepthsize);
    zCameraApplyViewing(&scene->camera, 0);
    zCameraApplyProjection(&scene->camera);

    /*
    // Update light positions.
    for (i = 0; i < scene->num_lights; i++) {
        glLightfv(GL_LIGHT0+i, GL_POSITION, scene->lights[i].position);
    }
    */

    // Draw XYZ axis if enabled.
    if (r_drawaxis) {
        if (glUseProgram) glUseProgram(0);
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        zDrawAxis();
    }

    // Draw normal posables.
    cur_pos = scene->posables;

    while (cur_pos) {
        // TODO: apply posable transormations.
        switch (cur_pos->type) {
            case Z_POSABLE_STATICMESH:
                //glPushMatrix();
                //glRotatef(23.4f, 1.0f, 0.0f, 0.0f);
                //glRotatef(time_elapsed*10.0f, 0.0f, 1.0f, 0.0f);
                zDrawMesh(cur_pos->subject.mesh);
                //zDrawMesh(cur_pos->subject.mesh);
                //glPopMatrix();
                break;
        }
        cur_pos = cur_pos->next;
    }

    // Draw sky posables.
    if (!r_nosky) {
        glDepthRange(1.0-r_skydepthsize, 1.0);
        zCameraApplyViewing(&scene->camera, 1);
        cur_pos = scene->sky_posables;

        while (cur_pos) {
            switch (cur_pos->type) {
                case Z_POSABLE_STATICMESH:
                    zDrawMesh(cur_pos->subject.mesh);
                    break;
            }
            cur_pos = cur_pos->next;
        }
    }



}



// Add posable to scene.
void zAddPosableToScene(ZScene *scene, ZPosable *posable, int sky)
{
    // Check initial next pointer is NULL, if not it may not have been initialized, it would be nice
    // to catch this early..
    assert(!posable->next);

    if (sky) {
        posable->next = scene->sky_posables;
        scene->sky_posables = posable;
    } else {
        posable->next = scene->posables;
        scene->posables = posable;
    }
}



void zAddMeshToScene(ZScene *scene, const char *name, int sky)
{
    ZPosable *pos;
    ZMesh *mesh;

    assert(name && strlen(name));

    mesh = zLookupMesh(name);

    if (!mesh) {
        zError("Failed to add mesh \"%s\" to scene.", name);
        return;
    }

    if ( !(pos = malloc(sizeof(ZPosable))) ) {
        zError("Failed to allocate ZPosable while trying to add mesh to scene.");
        return;
    }

    memset(pos, '\0', sizeof(ZPosable));

    pos->type = Z_POSABLE_STATICMESH;
    pos->subject.mesh = mesh;
    pos->next = NULL;

    // Add posable to posables list.
    zAddPosableToScene(scene, pos, sky);
}



// Mark scene non-resident.
void zMakeSceneNonResident(ZScene *scene)
{
    // For now only some OpenGL state is set when a scene is made resident, so there's nothing to
    // delete.
    scene->is_resident = 0;
}



// Delete scene and all objects in it.
void zDeleteScene(ZScene *scene)
{
    ZPosable *pos_tmp;

    assert(scene);

    // Delete posables
    while (scene->posables) {
        pos_tmp = scene->posables->next;
        free(scene->posables);
        scene->posables = pos_tmp;
    }

    // TODO: Turn off the lights! :p

    // TODO: Nuke all resources. It would be best if I can find a way to delete only those
    // meshes/textures/etc that are not references by the new scene, but implementing that is
    // probably more trouble than it's worth (i'd probably have to do that in a seperate function,
    // i.e. 'zChangeScene').

    free(scene);
}



