#include <assert.h>
#ifndef WIN32
#include <strings.h>
#endif
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "common.h"


typedef struct ZLuaConsoleFunc
{
    char *globalname;
    lua_CFunction cfunc;
    char *description;
    char *params;
} ZLuaConsoleFunc;


ZLuaConsoleFunc console_funcs[]; // Initialized at bottom


// Register all the console functions.
void zInitConsoleVM(lua_State *L)
{
    int i;

    lua_pushcfunction(L, luaopen_base);
    lua_call(L, 0, 0);

    for (i = 0; console_funcs[i].globalname; i++) {
        lua_register(L, console_funcs[i].globalname, console_funcs[i].cfunc);
    }
}


// Console command functions -----------------------------------------------------------------------

static int zConsoleHelp(lua_State *L)
{
    int i, matches = 0;
    const ZVariable *var;
    const char *name;

    if ( lua_gettop(L) != 1 || !lua_isstring(L, -1) ) {
        zPrint("Use \"help(name)\" where name is a string naming a command or variable, for more"
               " information.\n\n"
               "To get a complete listing of variables or commands, use \"listcommands()\", or"
               " \"listvars()\".\n\n");
        return 0;
    }

    name = lua_tostring(L, -1);

    for (i = 0; console_funcs[i].globalname; i++) {

        if (strcasecmp(console_funcs[i].globalname, name) == 0) {

            zPrint("Usage information for command \"%s\":\n\n", console_funcs[i].globalname);
            zPrint("  %s\n\n", console_funcs[i].description);
            if (console_funcs[i].params && strlen(console_funcs[i].params))
                zPrint("  parameters: %s\n", console_funcs[i].params);
            zPrint("\n");
            matches++;
        }
    }

    if ( (var = zLookupVariable(name)) ) {

        zPrint("Usage information for variable \"%s\":\n\n", var->name);
        zPrint("  %s\n\n", var->description);
        zPrint("     type: %s\n", zVariableType(var->type));

        if (var->type == Z_VAR_TYPE_INT) {
            zPrint("  current: %d\n", *((int *)var->varptr));
            zPrint("  default: %d\n", var->int_default);
            zPrint("  min/max: %d-%d\n", var->int_min, var->int_max);
        } else if (var->type == Z_VAR_TYPE_FLOAT) {
            zPrint("  current: %.3f\n", *((float *)var->varptr));
            zPrint("  default: %.3f\n", var->float_default);
            zPrint("  min/max: %.3f-%.3f\n", var->float_min, var->float_max);
        } else if (var->type == Z_VAR_TYPE_STRING) {
            zPrint("  current: %s\n", (char *)var->varptr);
            zPrint("  default: %s\n", var->str_default);
        } else if (var->type == Z_VAR_TYPE_FLOAT3) {
            zPrint("  current: %s\n", zGetFloat3String((float *)var->varptr));
            zPrint("  default: %s\n", zGetFloat3String(var->float3_default));
        } else if (var->type == Z_VAR_TYPE_FLOAT4) {
            zPrint("  current: %s\n", zGetFloat4String((float *)var->varptr));
            zPrint("  default: %s\n", zGetFloat4String(var->float4_default));
        }
        matches++;
    }

    if (!matches) zPrint("Unknown console command or variable \"%s\".\n", name);

    return 0;
}


static int zConsoleListKeys(lua_State *L)
{
    int i;

    zPrint("Listing all key symbols:\n");

    // Start at 1 since 0 (KEY_UNKNOWN) isn't a valid key
    for (i = 1; i < Z_NUM_KEYS; i++) {
        zPrint("  %3d: %-16s (%s)\n", i, zKeyName(i), zKeyDesc(i));
    }

    zPrint("\n");

    return 0;
}



static int zConsoleListKeyBindings(lua_State *L)
{
    unsigned int i;
    ZKeyBinding *kb;

    zPrint("Listing all key bindings:\n");

    for (i = 0; i < numkeybindings; i++) {

        kb = &(keybindings[i]);

        if (kb->lua) {
            zPrint("  %16s -> \"%s\"\n", zKeyEventName(&kb->keyevent, 1), kb->lua);
        } else if (kb->impulse && kb->keyevent.keystate == Z_KEY_STATE_PRESS) {
            zPrint("  %16s -> %s\n", zKeyEventName(&kb->keyevent, 0), kb->impulse->name);
        }
    }
    zPrint("\n");

    return 0;
}


static int zConsoleListImpulses(lua_State *L)
{
    int i;

    zPrint("Listing all impulses:\n");

    for (i = 0; impulses[i].name; i++)
        zPrint("  %-14s - %s\n", impulses[i].name, impulses[i].desc);

    zPrint("\n");

    return 0;
}


static int zConsoleListCommands(lua_State *L)
{
    int i;

    zPrint("Listing all console commands:\n");

    for (i = 0; console_funcs[i].globalname; i++)
        zPrint("  %-16s - %s\n", console_funcs[i].globalname, console_funcs[i].description);

    zPrint("\n");

    return 0;
}


static int zConsoleListVars(lua_State *L)
{
    int i;

    zPrint("Listing all configuration variables:\n");

    for (i=0; variables[i].type != Z_VAR_TYPE_INVALID; i++) {
        zPrint("  %-16s (%-7s) - %s\n", variables[i].name, zVariableType(variables[i].type),
            variables[i].description);
    }

    zPrint("\n");

    return 0;
}


static void zConsoleListMeshesCB(ZMesh *mesh, void *data)
{
    ZMaterial *cur = mesh->materials;
    zPrint("  %s\n", mesh->name);

    if (cur) {
        zPrint("    local materials:\n");
        while (cur) {
            zPrint("      %s\n", cur->name);
            cur = cur->next;
        }
    }
}

static int zConsoleListMeshes(lua_State *L)
{
    zPrint("Listing all loaded meshes:\n");
    zIterMeshes(zConsoleListMeshesCB, NULL);
    zPrint("\n");
    return 0;
}


static void zConsoleListMatsCB(ZMaterial *mat, void *data)
{
    if (mat->is_resident)
        zPrint("  %s (resident)\n", mat->name);
    else
        zPrint("  %s\n", mat->name);
}


static int zConsoleListMats(lua_State *L)
{
    zPrint("Listing all materials:\n");

    zIterMaterials(zConsoleListMatsCB, NULL);

    zPrint("\n");

    return 0;
}


static void zConsoleListTexturesCB(ZTexture *tex, void *data)
{
    zPrint("  %s\n", tex->name);
}


static int zConsoleListTextures(lua_State *L)
{
    zPrint("Listing all loaded textures:\n");
    zIterTextures(zConsoleListTexturesCB, NULL);
    zPrint("\n");
    return 0;
}


static void zConsoleListShaderProgramsCB(ZShaderProgram *program, void *data)
{
    zPrint("  %s / %s, flags = %#x\n", program->vertex_shader, program->fragment_shader,
        program->flags);
}

static void zConsoleListShadersCB(ZShader *shader, void *data)
{
    zPrint("  %s, flags = %#x\n", shader->name, shader->flags);
}

static int zConsoleListShaders(lua_State *L)
{
    zPrint("Listing all loaded shader programs:\n");
    zIterShaderPrograms(zConsoleListShaderProgramsCB, NULL);
    zPrint("\n");

    zPrint("Listing all loaded shaders:\n");
    zIterShaders(zConsoleListShadersCB, NULL);
    zPrint("\n");

    return 0;
}


static int zConsoleSceneInfo(lua_State *L)
{
    if (scene)
        zSceneInfo(scene);
    else
        zError("No scene currently loaded.");

    return 0;
}


static int zConsoleRendererInfo(lua_State *L)
{
    if (renderer_active)
        zRendererInfo();
    else
        zError("Can't display renderer information while the renderer isn't active...");

    return 0;
}


static int zConsoleMtlInfo(lua_State *L)
{
    ZMaterial *mtl;
    const char *name = luaL_checkstring(L, 1);

    if ( !(mtl = zLookupMaterial(name)) )
        zError("No material named \"%s\" found.", name);
    else
        zMaterialInfo(mtl);

    return 0;
}



static int zConsoleLoadScene(lua_State *L)
{
    ZCamera cam;
    const char *name = luaL_checkstring(L, 1);

    // For convenience I make a copy of the camera so the view doesn't get reset everytime I load a
    // new scene. I probably want to remove this once I am done with testing scene loading etc.
    zCameraInit(&cam);

    zPrint("Loading scene \"%s\".\n", name);

    // If a scene is currently active, copy its camera orientation, then delete the scene.
    if (scene) {

        // I only copy over some select camera fields to future-proof this code somewhat..
        cam.position = scene->camera.position;
        cam.forward  = scene->camera.forward;
        cam.up       = scene->camera.up;
        cam.fov      = scene->camera.fov;

        zDeleteScene(scene);
    }

    if ((scene = zLoadScene(name)))
        scene->camera = cam;

    return 0;
}


static int zConsoleAddMesh(lua_State *L)
{
    const char *name;

    name = luaL_checkstring(L, 1);

    if (!scene) {
        zError("Unable to load mesh without an active scene.");
        return 0;
    }

    if (!strlen(name)) {
        zError("Failed to load mesh, no mesh name given.");
        return 0;
    }

    zPrint("Adding mesh \"%s\" to current scene.\n", name);

    if (lua_tointeger(L, 2))
        zAddMeshToScene(scene, name, 1);
    else
        zAddMeshToScene(scene, name, 0);

    return 0;
}


static int zConsoleRunScript(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);

    zPrint("Running script \"%s\".\n", name);

    zLuaRunFile(Z_VM_CONSOLE, name);

    return 0;
}


static int zConsoleEcho(lua_State *L)
{
    zPrint("%s\n", luaL_checkstring(L, 1));
    return 0;
}


static int zConsoleQuit(lua_State *L)
{
    text_input = running = 0;
    return 0;
}



static int zConsoleResetCamera(lua_State *L)
{
    if (!scene) {
        zError("Unable to reset camera, no active scene.");
        return 0;
    }

    zCameraInit(&scene->camera);
    zCameraApplyProjection(&scene->camera);
    return 0;
}



static int zConsoleSetCameraPos(lua_State *L)
{
    float pos[3];

    if (!scene) {
        zError("Unable to set camera position, no active scene.");
    } else {
        pos[0] = (float) luaL_checknumber(L, 1);
        pos[1] = (float) luaL_checknumber(L, 2);
        pos[2] = (float) luaL_checknumber(L, 3);
        zCameraSetPosition(&scene->camera, pos[0], pos[1], pos[2]);
    }

    return 0;
}


static int zConsoleSetCameraDir(lua_State *L)
{
    float yaw = 0.0f, pitch = 0.0f;

    if (!scene) {
        zError("Unable to set camera direction, no active scene.");
    } else {
        yaw   = (float) luaL_checknumber(L, 1);

        if (lua_gettop(L) > 1)
            pitch = (float) luaL_checknumber(L, 2);

        zCameraSetForward(&scene->camera, 0.0f, 0.0f, -1.0f);
        zCameraSetUp(&scene->camera, 0.0f, 1.0f, 0.0f);
        zCameraYaw(&scene->camera, yaw);
        if (lua_gettop(L) > 1)
            zCameraPitch(&scene->camera, pitch);
    }

    return 0;
}


static int zConsoleToggleFullscreen(lua_State *L)
{
    zSetFullscreen(Z_FULLSCREEN_TOGGLE);
    return 0;
}


static int zConsoleRestartVideo(lua_State *L)
{
    zCloseWindow();
    zOpenWindow();
    return 0;
}


static int zConsoleSet(lua_State *L)
{
    const char *varname = luaL_checkstring(L, 1);
    ZVariable *var = zLookupVariable(varname);
    const char *str = NULL;
    size_t len;
    float vec[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    if (!var) {
        zError("Failed to set variable \"%s\", not a valid variable name.", varname);
        return 0;
    }

    switch (var->type) {

        case Z_VAR_TYPE_INT:
            zVarSetInt(var, luaL_checkinteger(L, 2));
            break;

        case Z_VAR_TYPE_FLOAT:
            zVarSetFloat(var, (float) luaL_checknumber(L, 2));
            break;

        case Z_VAR_TYPE_FLOAT3:
            vec[0] = (float) luaL_checknumber(L, 2);
            vec[1] = (float) luaL_checknumber(L, 3);
            vec[2] = (float) luaL_checknumber(L, 4);
            memcpy(var->varptr, vec, sizeof(float)*3);
            break;

        case Z_VAR_TYPE_FLOAT4:
            vec[0] = (float) luaL_checknumber(L, 2);
            vec[1] = (float) luaL_checknumber(L, 3);
            vec[2] = (float) luaL_checknumber(L, 4);
            vec[3] = (float) luaL_checknumber(L, 5);
            memcpy(var->varptr, vec, sizeof(float)*4);
            break;

        case Z_VAR_TYPE_STRING:
            str = luaL_checkstring(L, 2);
            assert(str);
            len = strlen(str);
            if (len > Z_VAR_STRING_SIZE-1)
                zError("Failed to set variable \"%s\", string exceeded %d bytes.", varname,
                    Z_VAR_STRING_SIZE);
            else
                strcpy((char *)var->varptr, str);

            break;

        default:
            assert(0 && "Unknown variable type.");
    }

    var->changed++;

    return 0;
}


static int zConsoleGet(lua_State *L)
{
    const char *varname = luaL_checkstring(L, 1);
    ZVariable *var = zLookupVariable(varname);

    if (!var) {
        zError("Failed to get variable \"%s\", not a valid variable name.", varname);
        return 0;
    }

    switch (var->type) {
        case Z_VAR_TYPE_INT:
            zPrint("%s = %d\n", var->name, *((int *)var->varptr));
            break;

        case Z_VAR_TYPE_FLOAT:
            zPrint("%s = %f\n", var->name, *((float *)var->varptr));
            break;

        case Z_VAR_TYPE_STRING:
            zPrint("%s = \"%s\"\n", var->name, (char *)var->varptr);
            break;

        case Z_VAR_TYPE_FLOAT3:
            zPrint("%s = %s\n", var->name, zGetFloat3String((float *)var->varptr));
            break;

        case Z_VAR_TYPE_FLOAT4:
            zPrint("%s = %s\n", var->name, zGetFloat4String((float *)var->varptr));
            break;

        default:
            assert(0 && "Unknown variable type.");
    }

    return 0;
}


static int zConsoleIncrease(lua_State *L)
{
    const char *varname = luaL_checkstring(L, 1);
    ZVariable *var = zLookupVariable(varname);

    if (!var) {
        zError("Failed to get variable \"%s\", not a valid variable name.", varname);
        return 0;
    }

    switch (var->type) {

        case Z_VAR_TYPE_INT:
            if (lua_gettop(L) > 1)
                zVarSetInt(var, *(int *)var->varptr + luaL_checkinteger(L, 2));
            else
                zVarSetInt(var, *(int *)var->varptr + 1);
            break;

        case Z_VAR_TYPE_FLOAT:
            if (lua_gettop(L) > 1)
                zVarSetFloat(var, *(float *)var->varptr + (float) luaL_checknumber(L, 2));
            else
                zVarSetFloat(var, *(float *)var->varptr + 1.0f);
            break;

        default:
            zError("Can only increase variable of type integer and float.");
            break;
    }

    var->changed++;

    return 0;
}


static int zConsoleDecrease(lua_State *L)
{
    const char *varname = luaL_checkstring(L, 1);
    ZVariable *var = zLookupVariable(varname);

    if (!var) {
        zError("Failed to get variable \"%s\", not a valid variable name.", varname);
        return 0;
    }

    switch (var->type) {

        case Z_VAR_TYPE_INT:
            if (lua_gettop(L) > 1)
                zVarSetInt(var, *(int *)var->varptr - luaL_checkinteger(L, 2));
            else
                zVarSetInt(var, *(int *)var->varptr - 1);
            break;

        case Z_VAR_TYPE_FLOAT:
            if (lua_gettop(L) > 1)
                zVarSetFloat(var, *(float *)var->varptr - (float) luaL_checknumber(L, 2));
            else
                zVarSetFloat(var, *(float *)var->varptr - 1.0f);
            break;

        default:
            zError("Can only decrease variable of type integer and float.");
            break;
    }

    var->changed++;

    return 0;
}


static int zConsoleToggle(lua_State *L)
{
    const char *varname = luaL_checkstring(L, 1);
    ZVariable *var = zLookupVariable(varname);

    if (!var) {
        zError("Failed to get variable \"%s\", not a valid variable name.", varname);
        return 0;
    }

    if (var->type == Z_VAR_TYPE_INT) {
        int newval = !(*((int *)var->varptr));
        zVarSetInt(var, newval);
    } else {
        zError("Given variable was not an integer.");
    }

    var->changed++;

    return 0;
}


static int zConsoleRevert(lua_State *L)
{
    const char *varname = luaL_checkstring(L, 1);
    ZVariable *var = zLookupVariable(varname);

    if (!var) {
        zError("Failed to revert variable \"%s\" to default, not a valid variable name.", varname);
        return 0;
    }

    switch (var->type) {

        case Z_VAR_TYPE_INT:
            zVarSetInt(var, var->int_default);
            break;
        case Z_VAR_TYPE_FLOAT:
            zVarSetFloat(var, var->float_default);
            break;
        case Z_VAR_TYPE_FLOAT3:
            memcpy(var->varptr, var->float3_default, sizeof(float)*3);
            break;
        case Z_VAR_TYPE_FLOAT4:
            memcpy(var->varptr, var->float4_default, sizeof(float)*4);
            break;
        case Z_VAR_TYPE_STRING:
            // Unlikely, but just to be safe.
            if (strlen(var->str_default) < Z_VAR_STRING_SIZE)
                strcpy((char *)var->varptr, var->str_default);
            else
                zError("Failed to revert variable \"%s\" to default, default string too large...",
                    varname);
            break;
        default:
            assert(0 && "Unknown variable type.");
    }

    var->changed++;

    return 0;
}



ZLuaConsoleFunc console_funcs[] = {
    { "help",            zConsoleHelp,            "Show usage info for a command or variable.", "name (string)" },
    { "listkeys",        zConsoleListKeys,        "Lists all defined key symbols.",             NULL },
    { "listkeybindings", zConsoleListKeyBindings, "Lists currently defined key bindings.",      NULL },
    { "listimpulses",    zConsoleListImpulses,    "Lists impulses.",                            NULL },
    { "listcommands",    zConsoleListCommands,    "Lists console commands.",                    NULL },
    { "listvars",        zConsoleListVars,        "Lists configuration variables.",             NULL },
    { "listmeshes",      zConsoleListMeshes,      "Lists loaded meshes.",                       NULL },
    { "listmats",        zConsoleListMats,        "Lists loaded materials.",                    NULL },
    { "listtextures",    zConsoleListTextures,    "Lists loaded textures.",                     NULL },
    { "listshaders",     zConsoleListShaders,     "Lists loaded shaders.",                      NULL },
    { "sceneinfo",       zConsoleSceneInfo,       "Prints details on currently loaded scene.",  NULL },
    { "rendererinfo",    zConsoleRendererInfo,    "Prints details on renderer.",                NULL },
    { "mtlinfo",         zConsoleMtlInfo,         "Prints details on a material.",              "name (string)" },
    { "loadscene",       zConsoleLoadScene,       "Loads a new scene.",                         "name (string)" },
    { "addmesh",         zConsoleAddMesh,         "Adds a mesh to the scene.",                  "filename (string), is_sky (number, optional)" },
    { "runscript",       zConsoleRunScript,       "Run a console script.",                      "filename (string)" },
    { "echo",            zConsoleEcho,            "Echoes back a message.",                     "message (string)" },
    { "quit",            zConsoleQuit,            "Quit " PACKAGE_NAME ".",                     NULL },
    { "resetcamera",     zConsoleResetCamera,     "Resets camera orientation.",                 NULL },
    { "setcamerapos",    zConsoleSetCameraPos,    "Sets camera position.",                      "pos_x (number), pos_y (number), pos_z (number)" },
    { "setcameradir",    zConsoleSetCameraDir,    "Sets camera direction.",                     "yaw (number), pitch (number, optional)" },
    { "restartvideo",    zConsoleRestartVideo,    "Reinitializes renderer.",                    NULL },
    { "togglefullscreen",zConsoleToggleFullscreen, "Toggle fullscreen window.",                 NULL },
    { "set",             zConsoleSet,             "Sets variable to given value.",              "varname (string), value (mixed) ..." },
    { "get",             zConsoleGet,             "Prints current value of a variable.",        "varname (string)" },
    { "increase",        zConsoleIncrease,        "Increase value of scalar variable.",         "varname (string), increment (number, optional)" },
    { "decrease",        zConsoleDecrease,        "Decrease value of scalar variable.",         "varname (string), decrement (number, optional)" },
    { "toggle",          zConsoleToggle,          "Negate value of integer variable.",          "varname (string)" },
    { "revert",          zConsoleRevert,          "Revert value of variable to default.",       "varname (string)" },
    { NULL, NULL, NULL, NULL }
};

