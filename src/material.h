#ifndef __MATERIAL_H__
#define __MATERIAL_H__

#include <GL/glew.h>

// Material flags.
#define Z_MTL_FRESNEL 1 // Use fresnel term.

// Material blending types.
#define Z_MTL_BLEND_NONE  0
#define Z_MTL_BLEND_ALPHA 1
#define Z_MTL_BLEND_ADD   2

// Texture parameters.
#define Z_TEX_WRAP_REPEAT       1
#define Z_TEX_WRAP_CLAMP        2
#define Z_TEX_WRAP_CLAMPEDGE    3

#define Z_TEX_FILTER_NEAREST    1
#define Z_TEX_FILTER_LINEAR     2


typedef struct ZTexture
{
    char name[Z_RESOURCE_NAME_SIZE];

    GLuint gltexname; // If this is 0, the texture was not yet uploaded.

    // Currently set texture parameters, these are used to determine wether texture parameters need
    // to be changed when a material is made active.
    unsigned char wrap_mode;
    unsigned char min_filter;
    unsigned char mag_filter;

    struct ZTexture *next;

} ZTexture;



typedef struct ZMaterial
{
    char name[Z_RESOURCE_NAME_SIZE];

    int is_resident;

    // Material flags
    unsigned int flags;

    // Blending type
    unsigned int blend_type;

    float ambient_color[4];
    float diffuse_color[4];
    float specular_color[4];
    float emission_color[4];
    float shininess;

    // Texture maps attributes
    unsigned char wrap_mode;
    unsigned char min_filter;
    unsigned char mag_filter;
    char diffuse_map_name[Z_RESOURCE_NAME_SIZE];
    char normal_map_name[Z_RESOURCE_NAME_SIZE];
    char specular_map_name[Z_RESOURCE_NAME_SIZE];

    // Pointers to loaded texture maps.
    ZTexture *diffuse_map;
    ZTexture *normal_map;
    ZTexture *specular_map;

    // Shaders
    char vertex_shader[Z_RESOURCE_NAME_SIZE];
    char fragment_shader[Z_RESOURCE_NAME_SIZE];
    ZShaderProgram *program;

    GLuint list_id;

    struct ZMaterial *next;

} ZMaterial;



extern ZMaterial default_material;



void zLoadMaterials(void);

void zMaterialInit(void);

void zMaterialDeinit(void);

void zMaterialInfo(ZMaterial *mtl);

ZMaterial *zLookupMaterial(const char *name);

void zIterMaterials(void (*iter)(ZMaterial *, void *), void *data);

void zMakeMaterialResident(ZMaterial *mat);

void zMakeMaterialNonResident(ZMaterial *mat, void *ignored);

void zMakeMaterialActive(ZMaterial *mat);

ZMaterial *zNewMaterial(void);

ZMaterial *zCopyMaterial(ZMaterial *mat);

void zDeleteMaterial(ZMaterial *mat);

void zResetMaterialState(void);



ZTexture *zLookupTexture(const char *name);

void zDeleteTexture(ZTexture *tex);

void zDeleteTextures(void);

void zIterTextures(void (*iter)(ZTexture *, void *), void *data);

#endif
