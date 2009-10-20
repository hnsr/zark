#include <string.h>
#ifndef WIN32
#include <strings.h>
#endif
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>


#include "common.h"

// Generate definitions.
#define MAKE_DEFINITIONS
#include "variables.def"
#undef MAKE_DEFINITIONS


ZVariable variables[] = {
    // Generate array initializers.
    #define MAKE_ARRAYINITS
    #include "variables.def"
    #undef MAKE_ARRAYINITS
    { Z_VAR_TYPE_INVALID }
};



// Return (statically allocated) string containing name of given type.
const char *zVariableType(ZVariableType type)
{
    switch (type) {
        case Z_VAR_TYPE_INT:    return "integer";
        case Z_VAR_TYPE_FLOAT:  return "float";
        case Z_VAR_TYPE_FLOAT3: return "float3";
        case Z_VAR_TYPE_FLOAT4: return "float4";
        case Z_VAR_TYPE_STRING: return "string";
        default: break;
    }

    assert( NULL && "Invalid variable type given");

    return "invalid";
}



// Returns 1 if the current value of var equals the default value, 0 if not.
int zVarIsDefault(ZVariable *var)
{
    const float *cur, *def;

    switch (var->type) {

        case Z_VAR_TYPE_INT:
            return var->int_default == *(int *)var->varptr;

        case Z_VAR_TYPE_FLOAT:
            return var->float_default == *(float *)var->varptr;

        case Z_VAR_TYPE_FLOAT3:
            cur = (float *)var->varptr;
            def = var->float3_default;
            return cur[0] == def[0] &&
                   cur[1] == def[1] &&
                   cur[2] == def[2];

        case Z_VAR_TYPE_FLOAT4:
            cur = (float *)var->varptr;
            def = var->float4_default;
            return cur[0] == def[0] &&
                   cur[1] == def[1] &&
                   cur[2] == def[2] &&
                   cur[3] == def[3];

        case Z_VAR_TYPE_STRING:
            return strcmp((char *)var->varptr, var->str_default) == 0;

        default:
            assert(0 && "Invalid variable type.");
    }

    return 0;
}



// Return pointer to variable with name given, or NULL if none found.
ZVariable *zLookupVariable(const char *name)
{
    int i;

    // Just doing a linear search, this is only really called when parsing zvar files so it's not
    // going to matter performance-wise.
    for (i = 0; variables[i].type != Z_VAR_TYPE_INVALID; i++) {

        if (strcasecmp(variables[i].name, name) == 0) {
            return variables+i;
        }
    }

    return NULL;
}



// Called from lua when a global is set. It is passed two parameteres k, v where k is a string value
// that names the variable to be set, and v the value, of mixed type.
// XXX: Since I am going to need this exact function exposed in the main scripting state, I'll
// probably need to move this to script.c or wherever at some point
static int zLuaSet(lua_State *L)
{
    ZVariable *var;

    // Should be safe to assume I always get two parameters, the first one a string, unless the glue
    // code fails somehow?
    assert(lua_gettop(L) == 2 && lua_isstring(L, 1));

    // Just warn if the variable doesn't exist, no need to error.
    if ( !(var = zLookupVariable(lua_tostring(L, 1))) ) {
        zWarning("Unknown variable \"%s\".", lua_tostring(L ,1));
        lua_pop(L, 2);
        return 0;
    }

    switch (var->type) {

        case Z_VAR_TYPE_FLOAT:
            zVarSetFloat(var, (float) lua_tonumber(L, 2));
            break;

        case Z_VAR_TYPE_INT:
            zVarSetInt(var, (int) lua_tonumber(L, 2));
            break;

        case Z_VAR_TYPE_STRING:
            {
                // Note: lua strings may contain zeroes (\0), but I truncate them to the first one.
                const char *str = lua_tostring(L, 2);
                size_t len = 0;

                if (!str) {
                    zWarning("Failed to set variable \"%s\", value must be a string or number.",
                        var->name);
                    break;
                } else if ( (len = strlen(str)) > Z_VAR_STRING_SIZE-1 ) {
                    zWarning("Failed to set variable \"%s\", value exceeded %d characters.",
                        var->name, Z_VAR_STRING_SIZE-1);
                    break;
                }
                memcpy(var->varptr, str, len);
            }
            break;

        case Z_VAR_TYPE_FLOAT3:
        case Z_VAR_TYPE_FLOAT4:
            {
                unsigned int count = 0, elems = (var->type == Z_VAR_TYPE_FLOAT3 ? 3 : 4);

                if (!lua_istable(L, 2) || lua_objlen(L, 2) != elems) {
                    zWarning("Failed to set variable \"%s\", value should be a table of %d"
                        " numbers.", var->name, elems);
                    break;
                }

                lua_pushnil(L);

                while (lua_next(L, 2)) {
                    if (count < elems) {
                        ((float *)var->varptr)[count++] = (float) lua_tonumber(L, -1);
                    }
                    lua_pop(L, 1);
                }
            }
            break;

        default:
            assert(0 && "Invalid variable type?");
            break;
    }

    assert(lua_gettop(L) == 2);
    lua_pop(L, 2);
    return 0;
}



// Load variables from file. Returns 0 if there was parse error, 1 otherwise. If there was a parse
// error, I should probably not subsequently call zWriteVariables, as that could cause data loss;
// Since the parse error could have prevented lots of settings form being loaded, overwriting the
// variables file would be a bad idea.
int zLoadVariables(const char *file)
{
    lua_State *L;
    const char *filename = zGetPath(file, NULL, Z_FILE_TRYUSER);

    if (!filename) return 0;

    if (zPathExists(filename) != Z_EXISTS_REGULAR) {
        zWarning("Failed to read variables from \"%s\", file doesn't exist (or is not a regular"
            " file).", filename);
        return 1; // Returning 1, because there was no parse error.
    }

    zPrint("Loading variables from \"%s\".\n", filename);

    // Create state, expose base library and zLuaSet.
    assert(L = luaL_newstate());
    lua_pushcfunction(L, luaopen_base);
    lua_call(L, 0, 0);
    lua_register(L, "set", zLuaSet);

    // Some glue so user can just set globals instead of having to call set() itself, this also
    // creates an empty environment.
    zLua(L, "new_env = {}");
    zLua(L, "setmetatable(new_env, { __newindex = function (t,k,v) set(k,v) end })");
    zLua(L, "setfenv(0, new_env)");

    if (luaL_dofile(L, filename)) {
        // XXX: This doesn't (always?) print source filename/linenumber, why?
        zWarning("lua: %s", lua_tostring(L, -1));
        lua_close(L);
        return 0;
    }

    lua_close(L);
    return 1;
}



// Dump variables with non-default values to file. File will be saved relative to DIR_USERDATA.
// Return 1 on success, 0 on failure.
int zWriteVariables(const char *file)
{
    const char *fullpath;
    ZVariable *var;
    float *vec;

    FILE *fp = zOpenFile(file, "", &fullpath, Z_FILE_FORCEUSER | Z_FILE_WRITE);

    if (!fp) {
        zWarning("Failed to open \"%s\" for writing variables.", fullpath);
        return 0;
    }

    zPrint("Writing variables to \"%s\".\n", fullpath);

    fprintf(fp, "-- This file is automatically written on shutdown.\n-- Only change values here"
        " (anything else will be lost).\n\n");

    // Iterate over each variable, if currentval != defaultval, write it.
    for (var = variables; var->type != Z_VAR_TYPE_INVALID; var++) {

        if (!zVarIsDefault(var)) {

            switch (var->type) {

                case Z_VAR_TYPE_INT:
                    fprintf(fp, "%-16s = %-12d -- %s\n", var->name, *(int *)var->varptr,
                        var->description);
                    break;

                case Z_VAR_TYPE_FLOAT:
                    fprintf(fp, "%-16s = %-12f -- %s\n", var->name, *(float *)var->varptr,
                        var->description);
                    break;

                case Z_VAR_TYPE_FLOAT3:
                    vec = (float *)var->varptr;
                    fprintf(fp, "%-16s = { %f, %f, %f } -- %s\n", var->name, vec[0], vec[1], vec[2],
                        var->description);
                    break;

                case Z_VAR_TYPE_FLOAT4:
                    vec = (float *)var->varptr;
                    fprintf(fp, "%-16s = { %f, %f, %f, %f } -- %s\n", var->name,
                        vec[0], vec[1], vec[2], vec[3], var->description);
                    break;

                case Z_VAR_TYPE_STRING:
                    fprintf(fp, "%-16s = \"%s\" -- %s\n", var->name, (char *)var->varptr,
                        var->description);
                    break;

                default:
                    assert(0 && "Invalid variable type.");
            }
        }
    }

    fclose(fp);

    return 1;
}


