#include <string.h>
#ifndef WIN32
#include <strings.h>
#endif
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>


#include "common.h"

// Generate variable definitions.
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



// Return statically allocated string containing name of given variable type.
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

    assert(NULL && "Invalid variable type given");

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



// Return pointer to variable with given name, or NULL if none found.
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

                if (!lua_istable(L, 2) || lua_rawlen(L, 2) != elems) {
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



// Load variables from file. Returns 0 if there were no errors, Z_ERROR_SYNTAX if there was a syntax
// error, Z_ERROR_FILE if the file didn't exist, or Z_ERROR for all other errors. Prefix and flags
// are passed on to zGetPath.
static int zParseVariables(const char *filename, const char *prefix, int flags)
{
    lua_State *L;
    const char *path = zGetPath(filename, prefix, flags);

    if (!path) {
        zWarning("%s: Failed to construct path for \"%s\".", __func__, filename);
        return Z_ERROR;
    }

    if (zPathExists(path) != Z_EXISTS_REGULAR)
        return Z_ERROR_FILE;

    // TODO: Not sure if it's right to put this zPrint here, but the caller won't know the full
    // path (and full paths is what I want to print). I should probably just have the caller call
    // zGetPath and make zParseVariables work with a full path..
    zPrint("Loading variables from \"%s\".\n", path);

    // Create state, expose base library and zLuaSet.
    L = luaL_newstate();
    assert(L);
    lua_pushcfunction(L, luaopen_base);
    lua_call(L, 0, 0);
    lua_register(L, "set", zLuaSet);

    if (luaL_dofile(L, path)) {
        zWarning("Failed to parse variables: %s", lua_tostring(L, -1));
        lua_close(L);
        return Z_ERROR_SYNTAX;
    }

    lua_close(L);
    return 0;
}



// A flag that helps me prevent data loss when a user makes a typo in his config, and not everything
// is loaded. In that case I just print a warning and skip saving the incomplete configuration.
static int uservars_badsyntax = 0;

// Load previously saved variables from user config, or the default config (from the system data
// directory) if there was an error.
void zLoadConfig(void)
{
    int err;

    // Attempt to load for user first, if that fails, try loading from system data dir.
    if ( (err = zParseVariables(Z_FILE_CONFIG, NULL, Z_FILE_FORCEUSER)) ) {

        // See if there was a syntax error.
        uservars_badsyntax = err == Z_ERROR_SYNTAX;

        zPrint("Falling back to default config.\n");

        // TODO: For the sake of not ending up with a partial user config, with bits overriden by
        // the default config, I could revert all variables to their defaults at this point (before
        // loading the default config). Even better: If I prevent zSaveConfig from doing anything if
        // all veriables are at their defaults, I can probably do away with the whole
        // uservars_badsyntax check.

        // Now try loading it from system data dir to get some sane defaults.
        if ( (err = zParseVariables(Z_FILE_CONFIG, NULL, 0)) )
            zWarning("Failed to fall back to default configuration.");
    }
}



// Save variables with non-default values to user config.
void zSaveConfig(void)
{
    const char *path;
    FILE *fp;
    ZVariable *var;
    float *vec;


    if (uservars_badsyntax) {
        zWarning("Not writing config, check \"%s\" for syntax errors.", Z_FILE_CONFIG);
        return;
    }

    if (fs_nosave) {
        zPrint("Not writing config (fs_nosave).\n");
        return;
    }

    if ( !(fp = zOpenFile(Z_FILE_CONFIG, NULL, &path, Z_FILE_FORCEUSER | Z_FILE_WRITE)) ) {
        zWarning("Failed to open \"%s\" for writing config.", path);
        return;
    }

    zPrint("Writing config to \"%s\".\n", path);

    fprintf(fp, "-- This file is automatically written on exit.\n"
                "-- Only change values here (anything else will be overwritten).\n\n");

    // Iterate over each variable, write it if currentval != defaultval.
    for (var = variables; var->type != Z_VAR_TYPE_INVALID; var++) {

        if (!zVarIsDefault(var)) {

            switch (var->type) {

                case Z_VAR_TYPE_INT:
                    fprintf(fp, "set(\"%s\", %d) -- %s\n", var->name, *(int *)var->varptr,
                        var->description);
                    break;

                case Z_VAR_TYPE_FLOAT:
                    fprintf(fp, "set(\"%s\", %f) -- %s\n", var->name, *(float *)var->varptr,
                        var->description);
                    break;

                case Z_VAR_TYPE_FLOAT3:
                    vec = (float *)var->varptr;
                    fprintf(fp, "set(\"%s\", { %f, %f, %f }) -- %s\n", var->name, vec[0], vec[1], vec[2],
                        var->description);
                    break;

                case Z_VAR_TYPE_FLOAT4:
                    vec = (float *)var->varptr;
                    fprintf(fp, "set(\"%s\", { %f, %f, %f, %f }) -- %s\n", var->name,
                        vec[0], vec[1], vec[2], vec[3], var->description);
                    break;

                case Z_VAR_TYPE_STRING:
                    fprintf(fp, "set(\"%s\", \"%s\") -- %s\n", var->name, (char *)var->varptr,
                        var->description);
                    break;

                default:
                    assert(0 && "Invalid variable type.");
            }
        }
    }

    fclose(fp);
}



