#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common.h"

#define Z_SHADER_VERTEX   1
#define Z_SHADER_FRAGMENT 2

#define Z_SHADER_HASH_SIZE 128

// XXX: These arrays need to match the enums!
static char *uniform_names[Z_UNIFORM_NUM] = {
    "z_time",
    "z_tex_d",
    "z_tex_s",
    "z_tex_n",
    "z_sun_direction",
    "z_sun_color"
};

static char *attrib_names[Z_ATTRIB_NUM] = {
    "tangent",
    "bitangent",
};


static ZShader *shaders[Z_SHADER_HASH_SIZE];
static ZShaderProgram *programs[Z_SHADER_HASH_SIZE];



void zShaderInit(void)
{
}



static void zDeleteShaderPrograms(void)
{
    int i;
    ZShaderProgram *cur, *tmp;

    for (i = 0; i < Z_SHADER_HASH_SIZE; i++) {

        cur = programs[i];

        while (cur != NULL) {
            tmp = cur->next;
            glDeleteProgram(cur->handle);
            free(cur);
            cur = tmp;
        }

        programs[i] = NULL;
    }
}



static void zDeleteShaders(void)
{
    int i;
    ZShader *cur, *tmp;

    for (i = 0; i < Z_SHADER_HASH_SIZE; i++) {

        cur = shaders[i];

        while (cur != NULL) {
            tmp = cur->next;
            glDeleteShader(cur->handle);
            free(cur);
            cur = tmp;
        }

        shaders[i] = NULL;
    }
}



void zShaderDeinit(void)
{
    zDeleteShaderPrograms();
    zDeleteShaders();
}



// Set shader uniform values. Since not all uniforms need to be updated everytime I make a material
// active, I minimize redundant updating by keeping track of frame/scene load counts for uniforms
// that need to updated online once a frame (z_time), or once after a new scene is loaded (z_sun_*).
void zUpdateShaderProgram(ZShaderProgram *program)
{
#if 1
    // Only update if this shader program wasn't updated yet this frame.
    if (program->frame_updated != frame_count) {
        if (program->uniforms[Z_UNIFORM_TIME] >= 0)
            glUniform1f(program->uniforms[Z_UNIFORM_TIME], time_elapsed);
    }

    // If this shader has never been updated, set samplers as well
    if (program->frame_updated == 0) {

        // XXX: This causes a illegal operation on Intel HW.. why?
        if (program->uniforms[Z_UNIFORM_SAMPLER_D] >= 0)
            glUniform1i(program->uniforms[Z_UNIFORM_SAMPLER_D], 0);

        if (program->uniforms[Z_UNIFORM_SAMPLER_N] >= 0)
            glUniform1i(program->uniforms[Z_UNIFORM_SAMPLER_N], 1);

        if (program->uniforms[Z_UNIFORM_SAMPLER_S] >= 0)
            glUniform1i(program->uniforms[Z_UNIFORM_SAMPLER_S], 2);

    }

    // Update stuff for newly loaded scenes.
    if (program->scene_updated != sceneload_count && scene) {
        if (program->uniforms[Z_UNIFORM_SUN_DIRECTION] >= 0)
            glUniform3fv(program->uniforms[Z_UNIFORM_SUN_DIRECTION], 1, scene->sun_direction);
        if (program->uniforms[Z_UNIFORM_SUN_COLOR] >= 0)
            glUniform3fv(program->uniforms[Z_UNIFORM_SUN_COLOR], 1, scene->sun_color);
    }

    program->frame_updated = frame_count;
    program->scene_updated = sceneload_count;
#endif
}



// Load shader from source and compile it into a shader object. Returns valid shader object
// reference, or 0 on error. sourcefile should not contain more than Z_RESOURCE_NAME_SIZE-1
// character bytes.
static ZShader *zCompileShader(unsigned int flags, const char *sourcefile, GLenum type)
{
    GLchar *shader_source;
    GLuint shader_object;
    GLint shader_compiled;
    ZShader *new;
    int i = 0;
#define SOURCE_NUM_STRINGS 10
    const char *source[SOURCE_NUM_STRINGS];

    shader_source = zGetStringFromFile(zGetPath(sourcefile, NULL, Z_FILE_TRYUSER));

    if (fs_printdiskload) zDebug("Loading shader \"%s\" with flags %#x from disk.", sourcefile, flags);

    if (!shader_source) {
        zError("Failed to read shader source for \"%s\".", sourcefile);
        return NULL;
    }

    shader_object = glCreateShader(type);

    if (!shader_object) {
        zError("An error occured while trying to create shader object while processing \"%s\".",
            sourcefile);
        free(shader_source);
        return NULL;
    }

    // Generate header
    if (flags & Z_SHADER_NORMALMAP)   source[i++] = "#define NORMALMAP 1\n";
    else                              source[i++] = "#define NORMALMAP 0\n";
    if (flags & Z_SHADER_SPECULARMAP) source[i++] = "#define SPECULARMAP 1\n";
    else                              source[i++] = "#define SPECULARMAP 0\n";
    if (flags & Z_SHADER_FRESNEL)     source[i++] = "#define FRESNEL 1\n";
    else                              source[i++] = "#define FRESNEL 0\n";
    source[i++] = shader_source;
    assert(i < SOURCE_NUM_STRINGS);

    glShaderSource(shader_object, i, source, NULL);
    glCompileShader(shader_object);
    free(shader_source);

    glGetShaderiv(shader_object, GL_COMPILE_STATUS, &shader_compiled);

    if (r_shaderlog || !shader_compiled) {

        GLint log_length;

        glGetShaderiv(shader_object, GL_INFO_LOG_LENGTH, &log_length);

        if (log_length > 1) {

            char *log;

            zPrint("Shader compilation log (length %d) for %s: \n", log_length, sourcefile);

            log = malloc(log_length+1);

            if (log) {

                GLsizei log_return_length = 0;

                glGetShaderInfoLog(shader_object, log_length, &log_return_length, log);

                if (log_return_length)
                    zPrint("  %s\n", log);
                else
                    zWarning("Failed to get shader compilation log.");

                free(log);

            } else {
                zError("Failed to allocate memory for compilation log.\n");
            }
        }
    }

    if (!shader_compiled) {
        zError("Failed to compile shader while processing \"%s\".", sourcefile);
        glDeleteShader(shader_object);
        return NULL;
    }

    new = malloc(sizeof(ZShader));
    if (!new) {
        zError("Failed to allocate memory for ZShader.");
        glDeleteShader(shader_object);
        return NULL;
    }

    memset(new, '\0', sizeof(ZShader));
    new->flags = flags;
    strcat(new->name, sourcefile);
    new->handle = shader_object;

    return new;
}



// Create OpenGL shader program object using vshader and fshader. Either of them may be NULL, but
// not both. Returns 0 on failure, or a valid OpenGL shader program handle on success.
static ZShaderProgram *zLinkProgram(unsigned int flags, ZShader *vshader, ZShader *fshader)
{
    int i;
    GLint program_linked = 0;
    GLint validate_status = 0;
    GLuint handle;
    ZShaderProgram *program;

    if ( vshader || fshader ) {

        handle = glCreateProgram();

        if (handle) {

            if (vshader) glAttachShader(handle, vshader->handle);
            if (fshader) glAttachShader(handle, fshader->handle);

            glLinkProgram(handle);
            glGetProgramiv(handle, GL_LINK_STATUS, &program_linked);

            if (!program_linked) {
                glDeleteProgram(handle);
                zWarning("Failed to link program.");
                return NULL;
            }
        } else {
            zError("Failed to create new shader program.");
            return NULL;
        }
    } else {
        assert(0 && "zLinkProgram called with both vshader and fshader NULL...");
        return NULL;
    }

    program = malloc(sizeof(ZShaderProgram));

    if (!program) {
        glDeleteProgram(handle);
        zError("Failed to allocate memory for ZShaderProgram.");
        return NULL;
    }

    memset(program, '\0', sizeof(ZShaderProgram));
    program->flags = flags;

    // Retrieve uniform/attrib locations.
    for (i = 0; i < Z_UNIFORM_NUM; i++) {
        //zDebug("looking for uniform with name \"%s\"", uniform_names[i]);
        program->uniforms[i] = glGetUniformLocation(handle, uniform_names[i]);
        //if (program->uniforms[i] >= 0) zDebug("found uniform location for \"%s\"", uniform_names[i]);
    }

    for (i = 0; i < Z_ATTRIB_NUM; i++) {
        program->attributes[i] = glGetAttribLocation(handle, attrib_names[i]);
        //if (program->attributes[i]) zDebug("got attrib location %d for %s",
        //    program->attributes[i], attrib_names[i] );
    }


    if (vshader) strcat(program->vertex_shader, vshader->name);
    if (fshader) strcat(program->fragment_shader, fshader->name);
    program->handle = handle;

    // Validate program.
    // XXX: Should this maybe be done after uniforms have been assigned? If so, I could probably
    // check for some validated flag and do it in zUpdateShaderProgram..
    glValidateProgram(handle);

    glGetProgramiv(handle, GL_VALIDATE_STATUS, &validate_status);

    if (r_shaderlog || !validate_status) {

        GLint log_length;

        glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &log_length);

        if (log_length > 1) {

            char *log;

            zPrint("Program log (length %d) for shaders %s and %s: \n", log_length,
                program->vertex_shader, program->fragment_shader);

            log = malloc(log_length+1);

            if (log) {

                GLsizei log_return_length = 0;

                glGetProgramInfoLog(handle, log_length, &log_return_length, log);

                if (log_return_length)
                    zPrint("  %s\n", log);
                else
                    zWarning("Failed to get program info log.");

                free(log);
            } else {
                zError("Failed to allocate memory for program info log.\n");
            }
        }
    }

    if (validate_status == GL_FALSE) {
        zWarning("Failed to validate program for shaders %s and %s.", program->vertex_shader,
            program->fragment_shader);
    }


    return program;
}



// Look up a compiled shader, or load and compile from disk if not found. If an error occurs or
// shader is an empty string, NULL is returned, otherwise a valid pointer to the shader.
ZShader *zLookupShader(unsigned int flags, const char *shader, int type)
{
    ZShader *cur, *new;
    unsigned int i;

    i = zHashString(shader, Z_SHADER_HASH_SIZE);
    cur = shaders[i];

    while (cur) {
        // Check wether flags and name match.
        if ( cur->flags == flags && strcmp(cur->name, shader) == 0)
            return cur;

        cur = cur->next;
    }

    // Not found, so load/add it.
    if (type == Z_SHADER_VERTEX) {
        new = zCompileShader(flags, shader, GL_VERTEX_SHADER);
    } else if (type == Z_SHADER_FRAGMENT) {
        new = zCompileShader(flags, shader, GL_FRAGMENT_SHADER);
    } else {
        assert(0 && "Invalid shader type.");
    }

    if (new) {
        new->next = shaders[i];
        shaders[i] = new;
        return new;
    }

    return NULL;
}



// Look up a shader program that matches vshader/fshader, or attempt to load it if not found.
// Returns NULL on failure. The vshader/fshader may not be NULL (but may be empty strings) and
// should each have no more than Z_RESOURCE_NAME_SIZE-1 character bytes. Either vshader or fhader
// needs to be non-empty.
ZShaderProgram *zLookupShaderProgram(unsigned int flags, const char *vshader, const char *fshader)
{
    unsigned int i;
    char programconcat[Z_RESOURCE_NAME_SIZE*2] = {'\0'};
    ZShaderProgram *cur, *new;
    ZShader *vertex_shader = NULL, *fragment_shader = NULL;

    assert( vshader[0] || fshader[0] );

    //zDebug("Looking up shader program for %s and %s.", vshader, fshader);

    // Check if I have a program loaded that matches given vshader/fshader, if so, return pointer to
    // it. To keep things simple I just concatenate the vshader/fshader names and use that to do a
    // lookup in the hashtable.
    strcat(programconcat, vshader);
    strcat(programconcat, fshader);
    i = zHashString(programconcat, Z_SHADER_HASH_SIZE);
    cur = programs[i];

    while (cur != NULL) {
        // Check if flags and shader names match.
        if ( cur->flags == flags && strcmp(cur->vertex_shader, vshader) == 0 &&
             strcmp(cur->fragment_shader, fshader) == 0  ) {
            return cur;
        }
        cur = cur->next;
    }

    //zDebug("No currently loaded program found for shaders %s and %s. Will load them now..",
    //    vshader, fshader);

    // If not, lookup the vertex and fragment shader and link them into a new program.
    if (strlen(vshader)) {
        if ( !(vertex_shader = zLookupShader(flags, vshader, Z_SHADER_VERTEX)) ) {
            zError("Failed to load vertex shader \"%s\".", vshader);
            return NULL;
        }
    }

    if (strlen(fshader)) {
        // If this fails, I may have loaded a vertex shader above that may not be used, but
        // shouldn't be too much of a waste, and it may end up being used anyway..
        if ( !(fragment_shader = zLookupShader(flags, fshader, Z_SHADER_FRAGMENT)) ) {
            zError("Failed to load fragment shader \"%s\".", fshader);
            return NULL;
        }
    }

    if ( (new = zLinkProgram(flags, vertex_shader, fragment_shader)) ) {
        // Reusing hash (i) from the lookup above!
        new->next = programs[i];
        programs[i] = new;
        return new;
    } else {
        return NULL;
    }
}




void zIterShaderPrograms(void (*iter)(ZShaderProgram *, void *), void *data)
{
    int i;
    ZShaderProgram *cur;

    for (i = 0; i < Z_SHADER_HASH_SIZE; i++) {

        cur = programs[i];

        while (cur != NULL) {
            iter(cur, data);
            cur = cur->next;
        }
    }
}



void zIterShaders(void (*iter)(ZShader *, void *), void *data)
{
    int i;
    ZShader *cur;

    for (i = 0; i < Z_SHADER_HASH_SIZE; i++) {

        cur = shaders[i];

        while (cur != NULL) {
            iter(cur, data);
            cur = cur->next;
        }
    }
}


