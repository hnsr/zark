#define _BSD_SOURCE // For strdup
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "common.h"


#define Z_MTL_HASH_SIZE 2048
#define Z_TEX_HASH_SIZE 4096


static ZMaterial *materials[Z_MTL_HASH_SIZE];

static ZTexture *textures[Z_TEX_HASH_SIZE];

static ZMaterial *previous_mat;

ZMaterial default_material = {
    { 'd', 'e', 'f', 'a', 'u', 'l', 't', '\0' },
    /*is_resident*/ 0, /*flags*/ 0, /*blend_type*/ 0,
    /*ambient*/  { 1.0f, 1.0f, 1.0f, 1.0f },
    /*diffuse*/  { 0.5f, 0.5f, 0.5f, 1.0f },
    /*specular*/ { 1.0f, 1.0f, 1.0f, 1.0f },
    /*emission*/ { 0.0f, 0.0f, 0.0f, 1.0f },
    /*shininess*/ 100.0f,
    Z_TEX_WRAP_REPEAT, /*min*/ Z_TEX_FILTER_LINEAR, /*mag*/ Z_TEX_FILTER_LINEAR,
    {'t','e','x','t','u','r','e','s','/','d','e','f','a','u','l','t','.','p','n','g','\0'},
    {'\0'},
    {'\0'},
    NULL, NULL, NULL,
    {'s','h','a','d','e','r','s','/','g','o','u','r','a','u','d','.','z','v','s','\0'},
    {'s','h','a','d','e','r','s','/','g','o','u','r','a','u','d','.','z','f','s','\0'},
    NULL,
    NULL
};



// Add material to hash table. If a material already exists with the same name as the given one,
// nothing is added and FALSE returned (TRUE otherwise). If succesful, the caller should not free()
// or modify mat after a succesful call, if unsuccesful, caller should make sure to clean up (i.e.
// free() mat) after itself.
static int zAddMaterial(ZMaterial *mat)
{
    unsigned int i;

    assert(mat->next == NULL);
    assert(strlen(mat->name)); // Makes no sense to add a material without a name.

    if (zLookupMaterial(mat->name)) {
        zWarning("%s: Material \"%s\" already exists.", __func__,
            mat->name);
        return FALSE;
    }

    i = zHashString(mat->name, Z_MTL_HASH_SIZE);
    mat->next = materials[i];
    materials[i] = mat;

    return TRUE;
}



// This gets called from lua with a table as argument. Sort out the parameters and add a new
// material.
static int zLuaMaterial(lua_State *L)
{
    ZMaterial *newmtl;

    if (!lua_istable(L, -1)) {
        zLuaWarning(L, 1, "Bad syntax, table expected");
        return 0;
    }

    // Clone default material and copy over name.
    if ( !(newmtl = zNewMaterial()) ) {
        zLuaWarning(L, 1, "Failed to allocate new material.");
        return 0;
    }

    // Get name, this is the one required field..
    if (!zLuaGetDataString(L, "name", newmtl->name, Z_RESOURCE_NAME_SIZE)) {
        zLuaWarning(L, 1, "No valid name given for material, ignoring.");
        zDeleteMaterial(newmtl);
        return 0;
    }

    zLuaGetDataUint  (L, "flags",           &(newmtl->flags));
    zLuaGetDataUint  (L, "blend_type",      &(newmtl->blend_type));
    zLuaGetDataFloats(L, "ambient_color",     newmtl->ambient_color,  4);
    zLuaGetDataFloats(L, "diffuse_color",     newmtl->diffuse_color,  4);
    zLuaGetDataFloats(L, "specular_color",    newmtl->specular_color, 4);
    zLuaGetDataFloats(L, "emission_color",    newmtl->emission_color, 4);
    zLuaGetDataFloats(L, "shininess",       &(newmtl->shininess),     1);
    zLuaGetDataUchar (L, "wrap_mode",       &(newmtl->wrap_mode));
    zLuaGetDataUchar (L, "min_filter",      &(newmtl->min_filter));
    zLuaGetDataUchar (L, "mag_filter",      &(newmtl->mag_filter));
    zLuaGetDataString(L, "diffuse_map_name",  newmtl->diffuse_map_name,  Z_RESOURCE_NAME_SIZE);
    zLuaGetDataString(L, "normal_map_name",   newmtl->normal_map_name,   Z_RESOURCE_NAME_SIZE);
    zLuaGetDataString(L, "specular_map_name", newmtl->specular_map_name, Z_RESOURCE_NAME_SIZE);
    zLuaGetDataString(L, "vertex_shader",     newmtl->vertex_shader,     Z_RESOURCE_NAME_SIZE);
    zLuaGetDataString(L, "fragment_shader",   newmtl->fragment_shader,   Z_RESOURCE_NAME_SIZE);

    // Add material to hash table.
    if (!zAddMaterial(newmtl)) {
        zLuaWarning(L, 1, "Failed to add material.");
        zDeleteMaterial(newmtl);
    }
    return 0;
}



// Load materials from material description files (= files with zmtl extension under "materials"
// directory).
void zLoadMaterials(void)
{
    char *file;
    lua_State *L;

    // Crerate temporary lua state.
    L = luaL_newstate();
    lua_pushcfunction(L, luaopen_base);
    lua_call(L, 0, 0);

    // Expose functions, enums/bitflags etc
    lua_register(L, "material", zLuaMaterial);
    lua_pushinteger(L, Z_MTL_FRESNEL);        lua_setglobal(L, "FRESNEL");
    lua_pushinteger(L, Z_MTL_BLEND_NONE);     lua_setglobal(L, "BLEND_NONE");
    lua_pushinteger(L, Z_MTL_BLEND_ALPHA);    lua_setglobal(L, "BLEND_ALPHA");
    lua_pushinteger(L, Z_MTL_BLEND_ADD);      lua_setglobal(L, "BLEND_ADD");
    lua_pushinteger(L, Z_TEX_WRAP_REPEAT);    lua_setglobal(L, "WRAP_REPEAT");
    lua_pushinteger(L, Z_TEX_WRAP_CLAMP);     lua_setglobal(L, "WRAP_CLAMP");
    lua_pushinteger(L, Z_TEX_WRAP_CLAMPEDGE); lua_setglobal(L, "WRAP_CLAMPEDGE");
    lua_pushinteger(L, Z_TEX_FILTER_NEAREST); lua_setglobal(L, "FILTER_NEAREST");
    lua_pushinteger(L, Z_TEX_FILTER_LINEAR);  lua_setglobal(L, "FILTER_LINEAR");

    // Run every material library through lua.
    while ( (file = zGetFileFromDir("materials")) ) {

        if (strcasecmp(zGetFileExtension(file), "zmtl") != 0)
            continue;

        zPrint("Loading materials from \"%s\".\n", file);

        if (luaL_dofile(L, file)) {
            zWarning("Failed to parse materials: %s", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }

    lua_close(L);
}



// Misc initializations that need to be done right after creating the renderer.
void zMaterialInit(void)
{
}



// Any clean-up that needs to be done when the renderer is destroyed.
void zMaterialDeinit(void)
{
    // Delete all textures and make materials non-resident.
    zDeleteTextures();
    zIterMaterials(zMakeMaterialNonResident, NULL);

    // Don't forget the hard-coded default material.
    zMakeMaterialNonResident(&default_material, NULL);
}



// Print some info on material.
void zMaterialInfo(ZMaterial *mtl)
{
    assert(mtl);

    zPrint("Dumping info on material \"%s\":\n", mtl->name);

    if (mtl->is_resident) zPrint("  resident:        yes\n");
    else                  zPrint("  resident:        no\n");

    zPrint("  flags:           ");
    if (mtl->flags & Z_MTL_FRESNEL) zPrint("fresnel ");
    zPrint("(%#x)\n", mtl->flags);

    zPrint("  blending type:   ");
    if      (mtl->blend_type == Z_MTL_BLEND_NONE)  zPrint("none\n");
    else if (mtl->blend_type == Z_MTL_BLEND_ALPHA) zPrint("alpha\n");
    else if (mtl->blend_type == Z_MTL_BLEND_ADD)   zPrint("additive\n");
    else zPrint("unknown?\n");

    zPrint("  ambient_color:   %s\n", zGetFloat4String(mtl->ambient_color));
    zPrint("  diffuse_color:   %s\n", zGetFloat4String(mtl->diffuse_color));
    zPrint("  specular_color:  %s\n", zGetFloat4String(mtl->specular_color));
    zPrint("  emission_color:  %s\n", zGetFloat4String(mtl->emission_color));
    zPrint("  shininess:       %.2f\n", mtl->shininess);

    zPrint("  texture wrap:    ");
    if      (mtl->wrap_mode == Z_TEX_WRAP_REPEAT)    zPrint("repeat\n");
    else if (mtl->wrap_mode == Z_TEX_WRAP_CLAMP)     zPrint("clamp\n");
    else if (mtl->wrap_mode == Z_TEX_WRAP_CLAMPEDGE) zPrint("clampedge\n");
    else zPrint("unknown?\n");

    if (mtl->min_filter == Z_TEX_FILTER_NEAREST) zPrint("  min filter:      nearest\n");
    if (mtl->min_filter == Z_TEX_FILTER_LINEAR)  zPrint("  min filter:      linear\n");

    if (mtl->mag_filter == Z_TEX_FILTER_NEAREST) zPrint("  mag filter:      nearest\n");
    if (mtl->mag_filter == Z_TEX_FILTER_LINEAR)  zPrint("  mag filter:      linear\n");

    zPrint("  diffuse map:     %s\n", mtl->diffuse_map_name);
    zPrint("  normal map:      %s\n", mtl->normal_map_name);
    zPrint("  specular map:    %s\n", mtl->specular_map_name);
    zPrint("  vertex shader:   %s\n", mtl->vertex_shader);
    zPrint("  fragment shader: %s\n", mtl->fragment_shader);

    zPrint("\n");
}



// Iterate over each material and call iter with it.
void zIterMaterials(void (*iter)(ZMaterial *, void *), void *data)
{
    int i;
    ZMaterial *cur;

    for (i = 0; i < Z_MTL_HASH_SIZE; i++) {

        cur = materials[i];

        while (cur != NULL) {
            iter(cur, data);
            cur = cur->next;
        }
    }
}



// Look up material by name. Returns pointer if found or NULL if not.
ZMaterial *zLookupMaterial(const char *name)
{
    unsigned int i;
    ZMaterial *cur;

    assert(name);
    assert(strlen(name));

    i = zHashString(name, Z_MTL_HASH_SIZE);
    cur = materials[i];

    while (cur != NULL) {

        if ( strcmp(cur->name, name) == 0)
            return cur;

        cur = cur->next;
    }

    return NULL;
}



// Set texture parameters according to those specified in the material, if neccesary. Unlike OpenGL
// itself, I store parameters (like repeat, clamping, filtering) per-material rather than
// per-texture. This allows me to share textures between materials, but it means I may need to
// update these parameters when switching to a different material that uses a shared texture but
// possibly with different parameters.
static void zSetTextureParams(ZMaterial *mat, ZTexture *tex)
{
    int bound = 0;

    if (!tex) return;

    assert(tex->gltexname > 0);

    if (mat->wrap_mode != tex->wrap_mode) {

        //zDebug("Setting wrap_mode for texture \"%s\".", tex->name);

        if (!bound) {
            glBindTexture(GL_TEXTURE_2D, tex->gltexname);
            bound = 1;
        }

        if (mat->wrap_mode == Z_TEX_WRAP_REPEAT) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        } else if (mat->wrap_mode == Z_TEX_WRAP_CLAMP) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        } else if (mat->wrap_mode == Z_TEX_WRAP_CLAMPEDGE) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        tex->wrap_mode = mat->wrap_mode;
    }

    if (mat->min_filter != tex->min_filter) {

        if (!bound) {
            glBindTexture(GL_TEXTURE_2D, tex->gltexname);
            bound = 1;
        }

        //zDebug("Setting min_filter for texture \"%s\".", tex->name);

        if (mat->min_filter == Z_TEX_FILTER_NEAREST) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        } else if (mat->min_filter == Z_TEX_FILTER_LINEAR) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_mipmap ?
            GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        }
        tex->min_filter = mat->min_filter;
    }

    if (mat->mag_filter != tex->mag_filter) {

        if (!bound) {
            glBindTexture(GL_TEXTURE_2D, tex->gltexname);
            bound = 1;
        }

        //zDebug("Setting mag_filter for texture \"%s\".", tex->name);

        if (mat->mag_filter == Z_TEX_FILTER_NEAREST)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        else if (mat->mag_filter == Z_TEX_FILTER_LINEAR)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        tex->mag_filter = mat->mag_filter;
    }

    if (bound) glBindTexture(GL_TEXTURE_2D, 0);

}



// Set OpenGL state for rendering material
static void zApplyMaterialState(ZMaterial *mat)
{
    // Make sure the texture parameters (texture filtering, wrap modes, etc) are set right for the
    // material. I keep track of what the texture paremeters are set to in ZTexture, so that I don't
    // set them if I don't need to.
    zSetTextureParams(mat, mat->diffuse_map);
    zSetTextureParams(mat, mat->normal_map);
    zSetTextureParams(mat, mat->specular_map);

    glMaterialfv(GL_FRONT, GL_AMBIENT,   mat->ambient_color);
    glMaterialfv(GL_FRONT, GL_DIFFUSE,   mat->diffuse_color);
    glMaterialfv(GL_FRONT, GL_SPECULAR,  mat->specular_color);
    glMaterialfv(GL_FRONT, GL_EMISSION,  mat->emission_color);
    glMaterialfv(GL_FRONT, GL_SHININESS, &(mat->shininess));

    if (mat->blend_type == Z_MTL_BLEND_ALPHA) {
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    } else if (mat->blend_type == Z_MTL_BLEND_ADD) {
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);
    } else {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }

    // Bind texture maps to the right texture units. For now, diffuse textures get bound to unit 0,
    // normalmaps to unit 1, and specular maps to unit 2. I may need to make this more flexible at
    // some point..
    glActiveTexture(GL_TEXTURE0);
    if (mat->diffuse_map) {
        glBindTexture(GL_TEXTURE_2D, mat->diffuse_map->gltexname);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    } else {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glActiveTexture(GL_TEXTURE1);
    if (mat->normal_map) {
        glBindTexture(GL_TEXTURE_2D, mat->normal_map->gltexname);
    } else {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glActiveTexture(GL_TEXTURE2);
    if (mat->specular_map) {
        glBindTexture(GL_TEXTURE_2D, mat->specular_map->gltexname);
    } else {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glActiveTexture(GL_TEXTURE0);

    if (!r_noshaders) {
        if (mat->program)
            glUseProgram(mat->program->handle);
        else
            glUseProgram(0);
    }
}



// Load textures, shader etc for material
void zMakeMaterialResident(ZMaterial *mat)
{
    assert(mat);

    // Make sure mat was initialized / made non-resident properly.
    assert(mat->is_resident == 0 && !mat->diffuse_map && !mat->specular_map && !mat->normal_map &&
           !mat->program);

    // Always make resident even if some resources fail to load, this is so I don't get stuck in an
    // infinite loop. Should probably give a warning if something fails to load however..
    mat->is_resident = 1;

    // Load texture maps.
    if (mat->diffuse_map_name[0]) {
        if ( !(mat->diffuse_map = zLookupTexture(mat->diffuse_map_name)) )
            zWarning("Failed to load texture \"%s\" for material \"%s\".", mat->diffuse_map_name,
                mat->name);
    }
    if (mat->specular_map_name[0]) {
        if ( !(mat->specular_map = zLookupTexture(mat->specular_map_name)) )
            zWarning("Failed to load texture \"%s\" for material \"%s\".", mat->specular_map_name,
                mat->name);
    }
    if (mat->normal_map_name[0]) {
        if ( !(mat->normal_map = zLookupTexture(mat->normal_map_name)) )
            zWarning("Failed to load texture \"%s\" for material \"%s\".", mat->normal_map_name,
                mat->name);
    }

    // Load shader.
    if ( !r_noshaders && (mat->vertex_shader[0] || mat->fragment_shader[0]) ) {
        unsigned int flags = 0;

        // Figure out which flags need to be passed on to the shader.
        if (mat->flags & Z_MTL_FRESNEL) flags |= Z_SHADER_FRESNEL;
        if (mat->normal_map)            flags |= Z_SHADER_NORMALMAP;
        if (mat->specular_map)          flags |= Z_SHADER_SPECULARMAP;

        mat->program = zLookupShaderProgram(flags, mat->vertex_shader, mat->fragment_shader);
        if (!mat->program) {
            zWarning("Failed to load shader program for material \"%s\".", mat->name);
        }
    }
}



// Mark material non-resident
void zMakeMaterialNonResident(ZMaterial *mat, void *ignored)
{
    if (!mat->is_resident) return;

    mat->diffuse_map  = NULL;
    mat->specular_map = NULL;
    mat->normal_map   = NULL;
    mat->program      = NULL;
    mat->is_resident  = 0;
}






// Update OpenGL state for drawing with given material.
void zMakeMaterialActive(ZMaterial *mat)
{
    if (mat && previous_mat == mat)
        return;

    if (!mat->is_resident)
        zMakeMaterialResident(mat);

    zApplyMaterialState(mat);

    // Update shader uniforms etc..
    if (mat->program)
        zUpdateShaderProgram(mat->program);

    previous_mat = mat;
}



// Create a new empty/basic material with some neutral defaults so it can be used as a base for
// user-defined materials.
ZMaterial *zNewMaterial(void)
{
    ZMaterial *new;

    new = malloc(sizeof(ZMaterial));

    if (!new) return NULL;

    memset(new, '\0', sizeof(ZMaterial));

    zSetFloat4(new->ambient_color,  1.0f, 1.0f, 1.0f, 1.0f);
    zSetFloat4(new->diffuse_color,  1.0f, 1.0f, 1.0f, 1.0f);
    zSetFloat4(new->specular_color, 1.0f, 1.0f, 1.0f, 1.0f);
    zSetFloat4(new->emission_color, 0.0f, 0.0f, 0.0f, 1.0f);

    new->blend_type = Z_MTL_BLEND_NONE;
    new->shininess  = 80.0f;
    new->wrap_mode  = Z_TEX_WRAP_REPEAT;
    new->min_filter = Z_TEX_FILTER_LINEAR;
    new->mag_filter = Z_TEX_FILTER_LINEAR;

    // XXX: Maybe use gouraud shader by default? since ff shading is borked at the moment..

    return new;
}



// Makes a proper copy (= makes sure to copy dynamically allocated members) of material. Returns
// pointer to new copy or NULL on failure.
ZMaterial *zCopyMaterial(ZMaterial *mat)
{
    ZMaterial *new;

    assert(mat);
    assert(!mat->next); // It probably doesn't make sense to copy something part of a linked list,
                        // so I want to know about it if this ever happens.

    new = malloc(sizeof(ZMaterial));

    if (!new) return NULL;

    *new = *mat;

    return new;
}



// Free material
void zDeleteMaterial(ZMaterial *mat)
{
    assert(mat);
    free(mat);
}



// This should be called when material OpenGL state has been changed in between zMaterialMakeActive
// calls. This is because I keep track of the previously active material and skip needlessly setting
// its state again if the same material is activated. This assumption wouldn't hold if something
// else touched the material state. For now I am just calling this right before I draw a scene,
// since I won't be touching any material OpenGL state myself when drawing a scene, but something
// after that (GUI?) might..
void zResetMaterialState(void)
{
    previous_mat = NULL;
}



// Add texture to hash table.
static void zAddTexture(ZTexture *tex)
{
    unsigned int i;

    assert(tex->next == NULL);

    i = zHashString(tex->name, Z_TEX_HASH_SIZE);
    tex->next = textures[i];
    textures[i] = tex;

    return;
}



// Load texture and add to texture list. Returns pointer to loaded texture or NULL on error.
static ZTexture *zLoadTexture(const char *name)
{
    ZTexture *tex;
    ZImage *img;
    int namelen;

    if ( (namelen = strlen(name)) >= Z_RESOURCE_NAME_SIZE) {
        zError("Texture name \"%s\" exceeds RESOURCE_NAME_SIZE, ignoring.", name);
        return NULL;
    }

    if ( !(tex = malloc(sizeof(ZTexture))) ) {
        zError("%s: Failed to allocate memory for while loading texture \"%s\".", __func__, name);
        return NULL;
    }

    memset(tex, '\0', sizeof(ZTexture));
    memcpy(tex->name, name, namelen);
    tex->name[namelen] = '\0';

    tex->gltexname = 0;
    tex->next = NULL;

    if (fs_printdiskload) zDebug("Loading texture \"%s\" from disk.", tex->name);

    // Load image and load OpenGL texture.
    if ( (img = zLoadImage(zGetPath(tex->name, NULL, Z_FILE_REWRITE_DIRSEP | Z_FILE_TRYUSER))) ) {

        glGenTextures(1, &(tex->gltexname));

        assert(tex->gltexname);

        glBindTexture(GL_TEXTURE_2D, tex->gltexname);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, r_mipmapbias);

        // No other parameters are set here. Because wrap_mode/(min|mag)_filter are initialized to
        // 0 they will be set the first time a material that uses the texture is made active.

        if (r_mipmap)
            gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, img->width, img->height, GL_RGBA,
                GL_UNSIGNED_BYTE, img->data);
        else
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img->width, img->height, 0, GL_RGBA,
                GL_UNSIGNED_BYTE, img->data);

        glBindTexture(GL_TEXTURE_2D, 0);
        zDeleteImage(img);

        zAddTexture(tex);
        return tex;
    }

    // Failed to load or add texture image, so delete it return NULL.
    zDeleteTexture(tex);

    return NULL;
}



// Lookup texture in list of textures, if not found, attempt to load it. Returns NULL on failure.
ZTexture *zLookupTexture(const char *name)
{
    unsigned int i;
    ZTexture *cur;

    assert(name);
    assert(strlen(name) > 0);

    // See if the texture is already loaded, return a pointer to it if so.
    i = zHashString(name, Z_TEX_HASH_SIZE);
    cur = textures[i];

    while (cur != NULL) {

        if ( strcmp(cur->name, name) == 0)
            return cur;

        cur = cur->next;
    }

    // Load the texture.
    return zLoadTexture(name);
}



// Delete OpenGL texture object and free tex.
void zDeleteTexture(ZTexture *tex)
{
    assert(tex);

    // This is safe even for textures that for some reason aren't loaded (i.e. gltextname == 0), as
    // glDeleteTextures silently ignores invalid texture names/0s.
    glDeleteTextures(1, &(tex->gltexname));

    free(tex);
}



// Delete all currently loaded textures.
void zDeleteTextures(void)
{
    int i;
    ZTexture *cur, *tmp;

    for (i = 0; i < Z_TEX_HASH_SIZE; i++) {

        cur = textures[i];

        while (cur != NULL) {
            tmp = cur->next;
            zDeleteTexture(cur);
            cur = tmp;
        }

        textures[i] = NULL;
    }
}



// Iterate over textures in the hash table and call iter with it.
void zIterTextures(void (*iter)(ZTexture *, void *), void *data)
{
    int i;
    ZTexture *cur;

    for (i = 0; i < Z_TEX_HASH_SIZE; i++) {

        cur = textures[i];

        while (cur != NULL) {
            iter(cur, data);
            cur = cur->next;
        }
    }
}




