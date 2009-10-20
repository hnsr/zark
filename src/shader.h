#ifndef __SHADER_H__
#define __SHADER_H__

#include <GL/glew.h>

#define Z_SHADER_NORMALMAP   1
#define Z_SHADER_SPECULARMAP 2
#define Z_SHADER_FRESNEL     4

typedef enum ZShaderUniform
{
    Z_UNIFORM_TIME,
    Z_UNIFORM_SAMPLER_D,
    Z_UNIFORM_SAMPLER_S,
    Z_UNIFORM_SAMPLER_N,
    Z_UNIFORM_SUN_DIRECTION,
    Z_UNIFORM_SUN_COLOR,
    Z_UNIFORM_NUM
} ZShaderUniform;


typedef enum ZShaderAttrib
{
    Z_ATTRIB_TANGENT,
    Z_ATTRIB_BITANGENT,
    Z_ATTRIB_NUM
} ZShaderAttrib;


typedef struct ZShader
{
    char name[Z_RESOURCE_NAME_SIZE];

    unsigned int flags;

    GLuint handle;

    struct ZShader *next;

} ZShader;


typedef struct ZShaderProgram
{
    char vertex_shader[Z_RESOURCE_NAME_SIZE];
    char fragment_shader[Z_RESOURCE_NAME_SIZE];

    unsigned int flags;

    // OpenGL handle for linked program object.
    GLuint handle;

    unsigned int frame_updated; // The frame number for which this shader's uniforms were most
                                // recently updated. Since frame-counting starts at 1, 0 means that
                                // they have enver been updated..
    unsigned int scene_updated; // Same as above but for scene loads.

    // Uniform locations that are looked up when the shader program is linked. There is a
    // pre-defined set of uniforms (see ZShaderUniform enum), all of which get looked up, since most
    // shaders will not use all of these uniforms, some will be set to -1.
    GLint uniforms[Z_UNIFORM_NUM];
    GLint attributes[Z_ATTRIB_NUM];

    struct ZShaderProgram *next;

} ZShaderProgram;



void zShaderInit(void);

void zShaderDeinit(void);

void zUpdateShaderProgram(ZShaderProgram *program);

ZShaderProgram *zLookupShaderProgram(unsigned int flags, const char *vshader, const char *fshader);

void zIterShaderPrograms(void (*iter)(ZShaderProgram *, void *), void *data);

void zIterShaders(void (*iter)(ZShader *, void *), void *data);

#endif
