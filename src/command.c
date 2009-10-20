#include <stdio.h>
#include <string.h>

#ifndef WIN32 // Header not preset on win32, but already defined elsewhere.
#include <strings.h>
#endif

#include <assert.h>
#include <stdlib.h>

#include "common.h"

// ZCmdArgSymbol - special symbols that are looked up when parsing string arguments on the
// commandline. Only used in command-related functions so no need to put this in the header.
typedef enum ZCmdArgSymbol
{
    #define MAKE_ENUM
    #include "cmdsymbols.def"
    #undef MAKE_ENUM
} ZCmdArgSymbol;

struct ZCmdArgName
{
    ZCmdArgSymbol symbol;
    const char *name;
} symbolnames[] = {
    #define MAKE_ARRAY
    #include "cmdsymbols.def"
    #undef MAKE_ARRAY
    {0, NULL}
};



// Look up symbol for given name
int zLookupCmdArgSymbol(const char *symbolname)
{
    int i;

    for (i = 0; symbolnames[i].name != NULL; i++) {
        if (strcasecmp(symbolnames[i].name, symbolname) == 0) return symbolnames[i].symbol;
    }

    return Z_CMDARG_INVALID;
}



// Run commands loaded from file. This should probably be called before a window is opened so that
// various settings (like resolution) can actually have an effect.
void zLoadCommands(const char *filename)
{
    FILE *fp;
    char buf[CMD_BUF_SIZE], *cmdstring;
    const char *fullpath;
    int bufpos = 0, c, linecount = 0;


    // Open file
    fp = zOpenFile(filename, NULL, &fullpath, Z_FILE_TRYUSER);

    if (fp == NULL) {
        zError("Failed to execute commands from \"%s\".", fullpath);
        return;
    }

    zPrint("Loading and executing commands from \"%s\".\n", fullpath);

    // Keep reading lines from fp until EOF
    while ( !feof(fp) ) {

        linecount++;

        // Read everything up to a newline or EOF, make sure I don't overflow buf.
        while ( (c = fgetc(fp)) != '\n' && c != EOF ) {

            // Since buf needs to be null-terminated, I can only store bufsize-1 characters, so
            // bufpos needs to be less than size-1.
            if (bufpos < CMD_BUF_SIZE-1) {
                buf[bufpos++] = c;
            } else {
                // Can't write anymore to buf, skip to newline (or EOF), warn user, and continue
                // with next line.
                zWarning("Line %d in \"%s\" exceeded CMD_BUF_SIZE and was ignored.", linecount, fullpath);
                while ( (c = fgetc(fp)) != '\n' && c != EOF ) ;
                bufpos = 0;
                linecount++;
                continue;
            }
        }
        buf[bufpos] = '\0';
        cmdstring = buf;

        // I have now read a line into buf. Before processing it I will (using the cmdstring
        // pointer) skip any initial whitespace, and check if the first few characters are "//" or
        // "#", in which case I will simply skip it. Oh and I might as well skip if it is an empty
        // string..
        while ( *cmdstring == ' ' || *cmdstring == '\t') cmdstring++;

        if ( *cmdstring == '#' || *cmdstring == '\0' || ( cmdstring[0] == '/' && cmdstring[1] == '/') ) {
            bufpos = 0;
            continue;
        }

        // Execute cmdstring.
        if (!zExecCmdString(buf)) {
            zWarning("Previous error was on line %u in \"%s\".", linecount, fullpath);
        }

        bufpos = 0;
        *buf = '\0';
    }

    fclose(fp);
}



// Free parsedcmds members and parsedcmds itself. Does nothing if numcmds is 0.
void zFreeParsedCmds(ZParsedCommand *parsedcmds, unsigned int numcmds)
{
    unsigned int i, j;

    // Loop over parsed commands and free argstr, and str_arg for all the arguments. No need to
    // null them out since parsedcmds itself will be freed never to be touched again!
    if ( numcmds ) {

        // Loop over all parsed commands
        for (i = 0; i < numcmds; i++) {

            //if (parsedcmds[i].argstr != NULL) zDebug("FREE argstr");

            free(parsedcmds[i].argstr);

            // Loop over all cmdargs.
            for (j = 0; j < parsedcmds[i].numargs; j++) {

                free(parsedcmds[i].args[j].str_arg);
            }
        }

        free(parsedcmds);
    }
}



// Execute parsed commands. Returns 0 if one or more commands failed to execute, 1 otherwise.
int zExecParsedCmds(const ZParsedCommand *parsedcmds, unsigned int numcmds)
{
    unsigned int i;
    int errors = 0;
    const ZParsedCommand *pcmd;

    // Loop over all the commands for this keybinding.
    for (i = 0; i < numcmds; i++) {

        pcmd = &(parsedcmds[i]);
        assert( pcmd->command != NULL );

        //zDebug("%s: running \"%s\".", __func__, pcmd->command->keyword);

        // TODO: Consider removing this check since this is already checked during parsing.
        if (pcmd->numargs >= pcmd->command->numrequired) {
            int res = pcmd->command->cmdfunc(pcmd);
            if (!res) errors++;
        } else {
            zWarning("Unable to execute \"%s\", too few arguments.", pcmd->command->keyword);
            errors++;
        }
    }

    if (errors) return 0;

    return 1;
}



// Execute a command string on the fly. Returns 0 if an error/warning occured, 1 otherwise.
int zExecCmdString(const char *cmdstring)
{
    int numcmds;
    ZParsedCommand *parsedcmds;

    // Return silently on empty/NULL string
    if (!cmdstring || strlen(cmdstring) == 0) return 1;

    // Parse the command string
    numcmds = zParseCmdString(cmdstring, &parsedcmds);

    // Execute parsed commands
    if (numcmds) {

        int res;

        res = zExecParsedCmds(parsedcmds, numcmds);
        zFreeParsedCmds(parsedcmds, numcmds);
        return res;

    } else {

        zWarning("Failed to parse command string \"%s\" (try \"help\").", cmdstring);
        return 0;
    }
}



// Look up a ZCommand by its keyname and return a pointer to it.
const ZCommand * zLookupCommand(const char *cmdname)
{
    int i;

    // Loop through every command and compare strings until there's a match.
    for (i = 0; commands[i].cmdfunc != NULL; i++) {
        if (strcasecmp(commands[i].keyword, cmdname) == 0)
            return &(commands[i]);
    }

    return NULL;
}




// Command handlers ================================================================================


int zCmdHelp(const ZParsedCommand *pcmd)
{
    const ZCommand *cmd;
    const ZVariable *var;

    // Display usage info on ourself if no argument was passed.
    if ( pcmd->numargs == 0 ) {
        zPrint("Use \"help <commandname|variablename>\" for information on a command or variable.\n\n"
               "To get a complete listing, use \"listcommands\", or \"listvars\".\n\n");
        return 1;
    }

    // Lookup command
    if ( (cmd = zLookupCommand(pcmd->args[0].str_arg)) ) {

        zPrint("Usage information for command \"%s\":\n\n", cmd->keyword);
        zPrint("  %s\n\n", cmd->description);

        if (cmd->numargs > 0) {
            unsigned int i;

            if (cmd->numrequired == 1)
                zPrint("  Takes the following arguments (1 is required):\n\n");
            else
                zPrint("  Takes the following arguments (%d are required):\n\n", cmd->numrequired);

            for (i = 0; i < cmd->numargs; i++) {
                zPrint("    %d: %s\n", i+1, cmd->argdesc[i]);
            }
            zPrint("\n");
        }
        return 1;

    } else if ( (var = zLookupVariable(pcmd->args[0].str_arg)) ) {

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
        }
        // TODO: Add support for float3/4.

        return 1;
    }

    zPrint("Unknown command/variable \"%s\".\n", pcmd->args[0].str_arg);
    return 0;
}


int zCmdQuit(const ZParsedCommand *pcmd)
{
    text_input = running = 0;
    return 1;
}


int zCmdEcho(const ZParsedCommand *pcmd)
{
    if (pcmd->numargs == 1) {
        zPrint("%s\n", pcmd->args[0].str_arg);
    } else if (pcmd->numargs > 1) {
        // Just print the entire argument string.
        zPrint("%s\n", pcmd->argstr);
    }
    return 1;
}


int zCmdDebug(const ZParsedCommand *pcmd)
{
    zDebug("%s", pcmd->args[0].str_arg);
    return 1;
}


int zCmdWarning(const ZParsedCommand *pcmd)
{
    zWarning("%s", pcmd->args[0].str_arg);
    return 1;
}


int zCmdListCommands(const ZParsedCommand *pcmd)
{
    int i;

    zPrint("Listing all defined commands:\n");

    for (i=0; commands[i].cmdfunc != NULL; i++) {
        zPrint("  %-16s - %s\n", commands[i].keyword, commands[i].description);
    }

    zPrint("\n");

    return 1;
}


int zCmdListKeyBindings(const ZParsedCommand *pcmd)
{
    unsigned int i;
    ZKeyBinding *kb;

    zPrint("Listing all key bindings:\n");

    for (i = 0; i < numkeybindings; i++) {

        kb = &(keybindings[i]);

        zPrint("  %20s -> %s\n", zKeyEventName(&(kb->keyevent)), kb->cmdstring);
    }
    zPrint("\n");

    return 1;
}


int zCmdListKeys(const ZParsedCommand *pcmd)
{
    int i;

    zPrint("Listing all key symbols:\n");

    // Start at 1 since 0 (KEY_UNKNOWN) isn't a valid key
    for (i = 1; i < NUM_KEYS; i++) {
        zPrint("  %3d: %-12s (%s)\n", i, zKeyName(i)+4, zKeyDesc(i));
    }

    zPrint("\n");

    return 1;
}


int zCmdListVars(const ZParsedCommand *pcmd)
{
    int i;

    zPrint("Listing all variables:\n");

    for (i=0; variables[i].type != Z_VAR_TYPE_INVALID; i++) {
        zPrint("  %-16s (%-7s) - %s\n", variables[i].name, zVariableType(variables[i].type),
            variables[i].description);
    }

    zPrint("\n");

    return 1;
}


static void zCmdListMatsCB(ZMaterial *mat, void *data)
{
    if (mat->is_resident)
        zPrint("  %s (resident)\n", mat->name);
    else
        zPrint("  %s\n", mat->name);
}

int zCmdListMats(const ZParsedCommand *pcmd)
{
    zPrint("Listing all materials:\n");

    zIterMaterials(zCmdListMatsCB, NULL);

    zPrint("\n");

    return 1;
}


static void zCmdListTexturesCB(ZTexture *tex, void *data)
{
    zPrint("  %s\n", tex->name);
}

int zCmdListTextures(const ZParsedCommand *pcmd)
{
    zPrint("Listing all loaded textures:\n");
    zIterTextures(zCmdListTexturesCB, NULL);
    zPrint("\n");
    return 1;
}


static void zCmdListMeshesCB(ZMesh *mesh, void *data)
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

int zCmdListMeshes(const ZParsedCommand *pcmd)
{
    zPrint("Listing all loaded meshes:\n");
    zIterMeshes(zCmdListMeshesCB, NULL);
    zPrint("\n");
    return 1;
}


static void zCmdListShaderProgramsCB(ZShaderProgram *program, void *data)
{
    zPrint("  %s / %s, flags = %#x\n", program->vertex_shader, program->fragment_shader,
        program->flags);
}

static void zCmdListShadersCB(ZShader *shader, void *data)
{
    zPrint("  %s, flags = %#x\n", shader->name, shader->flags);
}

int zCmdListShaders(const ZParsedCommand *pcmd)
{
    zPrint("Listing all loaded shader programs:\n");
    zIterShaderPrograms(zCmdListShaderProgramsCB, NULL);
    zPrint("\n");

    zPrint("Listing all loaded shaders:\n");
    zIterShaders(zCmdListShadersCB, NULL);
    zPrint("\n");

    return 1;
}


int zCmdLoadScene(const ZParsedCommand *pcmd)
{
    // For convenience I make a copy of the camera so the view doesn't get reset everytime I load a
    // new scene. I probably want to remove this once I am done with testing scene loading etc.
    ZCamera cam;

    zCameraInit(&cam);

    //zPrint("Loading scene \"%s\".\n", pcmd->args[0].str_arg);

    // Make sure to delete current scene and make a copy of the camera first..
    if (scene) {

        // I only copy over some select camera fields to future-proof this code somewhat..
        cam.position = scene->camera.position;
        cam.forward  = scene->camera.forward;
        cam.up       = scene->camera.up;
        cam.fov      = scene->camera.fov;

        zDeleteScene(scene);
    }

    if ((scene = zLoadScene(pcmd->args[0].str_arg)))
        scene->camera = cam;

    return 1;
}


int zCmdAddMesh(const ZParsedCommand *pcmd)
{
    //zPrint("Adding mesh \"%s\" to current scene.\n", pcmd->args[0].str_arg);

    if (!scene) {
        zError("Unable to load mesh without an active scene.");
        return 0;
    }

    if (pcmd->numargs == 2 && pcmd->args[1].int_arg) {
        zAddMeshToScene(scene, pcmd->args[0].str_arg, 1);
    } else {
        zAddMeshToScene(scene, pcmd->args[0].str_arg, 0);
    }

    return 1;
}



int zCmdRendererInfo(const ZParsedCommand *pcmd)
{
    if (renderer_active) {
        zRendererInfo();
    } else {
        zError("Can't display renderer information while the renderer isn't active...");
    }
    return 1;
}



int zCmdSceneInfo(const ZParsedCommand *pcmd)
{
    if (!scene) {
        zError("No scene currently loaded.");
        return 0;
    }

    zSceneInfo(scene);
    return 1;
}



int zCmdMaterialInfo(const ZParsedCommand *pcmd)
{
    ZMaterial *mtl;

    // zLookupMesh has a side-effect of actually loading the mesh if not currently loaded. Not sure
    // if that is sensible, but it doesn't really hurt either with meshinfo being mostly a debugging
    // command..
    if (!(mtl = zLookupMaterial(pcmd->args[0].str_arg))) {
        zError("No material named \"%s\" found.", pcmd->args[0].str_arg);
        return 0;
    }

    zMaterialInfo(mtl);

    return 1;
}


int zCmdMeshInfo(const ZParsedCommand *pcmd)
{
    ZMesh *mesh;

    // zLookupMesh has a side-effect of actually loading the mesh if not currently loaded. Not sure
    // if that is sensible, but it doesn't really hurt either with meshinfo being mostly a debugging
    // command..
    if (!(mesh = zLookupMesh(pcmd->args[0].str_arg))) {
        zError("No mesh named \"%s\" found.", pcmd->args[0].str_arg);
        return 0;
    }

    zMeshInfo(mesh);
    return 1;
}


int zCmdBindKey(const ZParsedCommand *pcmd)
{
    // TODO:
    // Lookup first arg as key.
    // Parse second arg as cmdstring.
    // How to specify modifier keys?
    zError("Not implemented yet :( ");
    return 0;
}


int zCmdResetCamera(const ZParsedCommand *pcmd)
{
    if (!scene) {
        zError("Unable to reset camera, no active scene.");
        return 0;
    }

    zCameraInit(&scene->camera);
    zCameraApplyProjection(&scene->camera);
    return 1;
}


int zCmdSetCameraPos(const ZParsedCommand *pcmd)
{
    if (!scene) {
        zError("Unable to set camera position, no active scene.");
        return 0;
    }

    zCameraSetPosition(&scene->camera,
        (float) pcmd->args[0].float_arg,
        (float) pcmd->args[1].float_arg,
        (float) pcmd->args[2].float_arg
    );
    return 1;
}


int zCmdSetCameraDir(const ZParsedCommand *pcmd)
{
    if (!scene) {
        zError("Unable to set camera direction, no active scene.");
        return 0;
    }

    zCameraSetForward(&scene->camera, 0.0f, 0.0f, -1.0f);
    zCameraSetUp(&scene->camera, 0.0f, 1.0f, 0.0f);
    zCameraYaw(&scene->camera, (float) pcmd->args[0].float_arg);
    zCameraPitch(&scene->camera, (float) pcmd->args[1].float_arg);
    return 1;
}


int zCmdStartMove(const ZParsedCommand *pcmd)
{
    int direction = pcmd->args[0].int_arg;
    unsigned int flags = controller.update_flags;

    switch (direction) {
        case Z_CMDARG_UP:
            flags |= Z_CONTROL_UP;
            break;
        case Z_CMDARG_DOWN:
            flags |= Z_CONTROL_DOWN;
            break;
        case Z_CMDARG_LEFT:
            flags |= Z_CONTROL_LEFT;
            break;
        case Z_CMDARG_RIGHT:
            flags |= Z_CONTROL_RIGHT;
            break;
        case Z_CMDARG_FORWARD:
            flags |= Z_CONTROL_FORWARD;
            break;
        case Z_CMDARG_BACK:
            flags |= Z_CONTROL_BACK;
            break;
        default:
            zWarning("startmove: Invalid direction");
            return 0;
    }

    controller.update_flags = flags;
    return 1;
}


int zCmdStopMove(const ZParsedCommand *pcmd)
{
    int direction = pcmd->args[0].int_arg;
    unsigned int flags = controller.update_flags;

    switch (direction) {
        case Z_CMDARG_UP:
            flags &= ~Z_CONTROL_UP;
            break;
        case Z_CMDARG_DOWN:
            flags &= ~Z_CONTROL_DOWN;
            break;
        case Z_CMDARG_LEFT:
            flags &= ~Z_CONTROL_LEFT;
            break;
        case Z_CMDARG_RIGHT:
            flags &= ~Z_CONTROL_RIGHT;
            break;
        case Z_CMDARG_FORWARD:
            flags &= ~Z_CONTROL_FORWARD;
            break;
        case Z_CMDARG_BACK:
            flags &= ~Z_CONTROL_BACK;
            break;
        default:
            zWarning("stopmove: Invalid direction");
            return 0;
    }

    controller.update_flags = flags;
    return 1;
}


int zCmdStartTurn(const ZParsedCommand *pcmd)
{
    int direction = pcmd->args[0].int_arg;
    unsigned int flags = controller.update_flags;

    switch (direction) {
        case Z_CMDARG_UP:
            flags |= Z_CONTROL_AIM_UP;
            break;
        case Z_CMDARG_DOWN:
            flags |= Z_CONTROL_AIM_DOWN;
            break;
        case Z_CMDARG_LEFT:
            flags |= Z_CONTROL_AIM_LEFT;
            break;
        case Z_CMDARG_RIGHT:
            flags |= Z_CONTROL_AIM_RIGHT;
            break;
        default:
            zWarning("startturn: Invalid direction");
            return 0;
    }

    controller.update_flags = flags;
    return 1;
}


int zCmdStopTurn(const ZParsedCommand *pcmd)
{
    int direction = pcmd->args[0].int_arg;
    unsigned int flags = controller.update_flags;

    switch (direction) {
        case Z_CMDARG_UP:
            flags &= ~Z_CONTROL_AIM_UP;
            break;
        case Z_CMDARG_DOWN:
            flags &= ~Z_CONTROL_AIM_DOWN;
            break;
        case Z_CMDARG_LEFT:
            flags &= ~Z_CONTROL_AIM_LEFT;
            break;
        case Z_CMDARG_RIGHT:
            flags &= ~Z_CONTROL_AIM_RIGHT;
            break;
        default:
            zWarning("stopturn: Invalid direction");
            return 0;
    }
    controller.update_flags = flags;
    return 1;
}


int zCmdStartAim(const ZParsedCommand *pcmd)
{
    zEnableMouse();
    controller.update_flags |= Z_CONTROL_AIM;
    return 1;
}


int zCmdStopAim(const ZParsedCommand *pcmd)
{
    zDisableMouse();
    controller.update_flags &= ~Z_CONTROL_AIM;
    return 1;
}


int zCmdStartZoom(const ZParsedCommand *pcmd)
{
    zEnableMouse();
    controller.update_flags |= Z_CONTROL_ZOOM;
    return 1;
}


int zCmdStopZoom(const ZParsedCommand *pcmd)
{
    zDisableMouse();
    controller.update_flags &= ~Z_CONTROL_ZOOM;
    return 1;
}


int zCmdTextConsole(const ZParsedCommand *pcmd)
{
    zEnableTextConsole();
    return 1;
}


int zCmdSet(const ZParsedCommand *pcmd)
{
    ZVariable *var = pcmd->args[0].var_arg;
    int len;
    int   i = pcmd->args[1].int_arg;
    float f = (float) pcmd->args[1].float_arg;
    char *s = pcmd->args[1].str_arg;

    if (var == NULL) {
        zError("Unrecognized variable name \"%s\".", pcmd->args[0].str_arg);
        return 0;
    }

    // Set to second parameter
    switch (var->type) {

    case Z_VAR_TYPE_INT:
        zVarSetInt(var, i);
        return 1;

    case Z_VAR_TYPE_FLOAT:
        zVarSetFloat(var, f);
        return 1;

    case Z_VAR_TYPE_STRING:
        if (s == NULL) return 1;

        len = MIN(Z_VAR_STRING_SIZE-1, strlen(s));
        memcpy((void *)var->varptr, s, len+1);

        // This is needed in case the string was truncated, last char won't be a \0
        ((char *)var->varptr)[Z_VAR_STRING_SIZE-1] = '\0';
        return 1;

    case Z_VAR_TYPE_FLOAT3:
    case Z_VAR_TYPE_FLOAT4:
        zDebug("float3/float3 var types not supported yet.");
        return 1;

    default:
        assert( 0 && "Invalid variable type. This is a bug :(");
        return 0;
    }

    zError("Value for variable \"%s\" is invalid or out of range.", var->name);
    return 0;
}


int zCmdGet(const ZParsedCommand *pcmd)
{
    ZVariable *var = pcmd->args[0].var_arg;

    if (var == NULL) {
        zError("No valid variable name was given.");
        return 0;
    }

    switch (var->type) {

    case Z_VAR_TYPE_INT:
        zPrint("%s = %d\n", var->name, *((int *)var->varptr));
        return 1;

    case Z_VAR_TYPE_FLOAT:
        zPrint("%s = %f\n",var->name, *((float *)var->varptr));
        return 1;

    case Z_VAR_TYPE_STRING:
        zPrint("%s = %s\n", var->name, (char *)var->varptr);
        return 1;

    case Z_VAR_TYPE_FLOAT3:
    case Z_VAR_TYPE_FLOAT4:
        zDebug("float3/float3 var types not supported yet.");
        return 1;

    default:
        assert( 0 && "Invalid variable type. This is a bug :(");
        return 0;
    }

    return 1;
}


int zCmdIncrease(const ZParsedCommand *pcmd)
{
    ZVariable *var = pcmd->args[0].var_arg;

    if (!var) {
        zError("No valid variable name was given.");
        return 0;
    }

    if (var->type == Z_VAR_TYPE_INT) {

        int newval = *((int *)var->varptr);

        if (pcmd->numargs > 1)
            newval += pcmd->args[1].int_arg;
        else
            newval += 1;

        zVarSetInt(var, newval);

    } else if (var->type == Z_VAR_TYPE_FLOAT) {

        float newval = *((float *)var->varptr);

        if (pcmd->numargs > 1)
            newval += (float) pcmd->args[1].float_arg;
        else
            newval += 1.0f;

        zVarSetFloat(var, newval);

    } else {
        zError("Given variable was not of float or integer type.");
        return 0;
    }

    return 1;
}


int zCmdDecrease(const ZParsedCommand *pcmd)
{
    ZVariable *var = pcmd->args[0].var_arg;

    if (!var) {
        zError("No valid variable name was given.");
        return 0;
    }

    if (var->type == Z_VAR_TYPE_INT) {

        int newval = *((int *)var->varptr);

        if (pcmd->numargs > 1)
            newval -= pcmd->args[1].int_arg;
        else
            newval -= 1;

        zVarSetInt(var, newval);

    } else if (var->type == Z_VAR_TYPE_FLOAT) {

        float newval = *((float *)var->varptr);

        if (pcmd->numargs > 1)
            newval -= (float) pcmd->args[1].float_arg;
        else
            newval -= 1.0f;

        zVarSetFloat(var, newval);

    } else {
        zError("Given variable was not of float or integer type.");
        return 0;
    }

    return 1;
}


int zCmdToggle(const ZParsedCommand *pcmd)
{
    ZVariable *var = pcmd->args[0].var_arg;

    if (!var) {
        zError("No valid variable name was given.");
        return 0;
    }

    if (var->type == Z_VAR_TYPE_INT) {
        int newval = !(*((int *)var->varptr));
        zVarSetInt(var, newval);
    } else {
        zError("Given variable was not an integer.");
        return 0;
    }

    return 1;
}


int zCmdRestartVideo(const ZParsedCommand *pcmd)
{
    zCloseWindow();
    zOpenWindow(r_winwidth, r_winheight);
    return 1;
}


// All the commands that are exposed are defined in here. MSVC complains about "{ }", so I'll have
// to put a dummy NULL entry in there.

//    cmdfuncptr           cmdkeyword   numarg/req  cmddescription
//       argdescriptions[]
ZCommand commands[] = {
    { zCmdHelp,            "help",            1, 0, "Show help information for a command.",
        { "Command name" } },
    { zCmdQuit,            "quit",            0, 0, "Exit " PACKAGE_NAME ".",
        { NULL } },
    { zCmdEcho,            "echo",            1, 1, "Write message.",
        { "Message" } },
    { zCmdDebug,           "debug",           1, 1, "Write debug message.",
        { "Message" } },
    { zCmdWarning,         "warning",         1, 1, "Write warning message.",
        { "Message" } },
    { zCmdListCommands,    "listcommands",    0, 0, "List all commands.",
        { NULL } },
    { zCmdListKeyBindings, "listkeybindings", 0, 0, "List all keybindings.",
        { NULL } },
    { zCmdListKeys,        "listkeys",        0, 0, "List all keys.",
        { NULL } },
    { zCmdListVars,        "listvars",        0, 0, "List all variables.",
        { NULL } },
    { zCmdListMats,        "listmats",        0, 0, "List all materials.",
        { NULL } },
    { zCmdListTextures,    "listtextures",    0, 0, "List all loaded textures.",
        { NULL } },
    { zCmdListMeshes,      "listmeshes",      0, 0, "List all loaded meshes.",
        { NULL } },
    { zCmdListShaders,     "listshaders",     0, 0, "List all loaded shaders and shader programs.",
        { NULL } },
    { zCmdLoadScene,       "loadscene",       1, 1, "Load a scene.",
        { "Scene filename" } },
    { zCmdAddMesh,         "addmesh",         2, 1, "Add a mesh to the active scene.",
        { "Mesh name", "Skybox flag" } },
    { zCmdRendererInfo,    "rendererinfo",    0, 0, "Display OpenGL renderer information.",
        { NULL } },
    { zCmdSceneInfo,       "sceneinfo",       0, 0, "Display info for the current scene.",
        { NULL } },
    { zCmdMaterialInfo,    "mtlinfo",         1, 1, "Display info for a material.",
        { "Material name" } },
    { zCmdMeshInfo,        "meshinfo",        1, 1, "Display info for a loaded mesh.",
        { "Mesh name" } },
    { zCmdBindKey,         "bindkey",         2, 2, "Bind key to a command string.",
        { "Key name (see listkeys)", "Quoted command string" } },
    { zCmdResetCamera,     "resetcamera",     0, 0, "Reset the view.",
        { NULL } },
    { zCmdSetCameraPos,    "setcamerapos",    3, 3, "Set view position.",
        { "X coordinate", "Y coordinate", "Z coordinate" } },
    { zCmdSetCameraDir,    "setcameradir",    2, 2, "Set view direction.",
        { "Yaw (degrees)", "Pitch (degrees)" } },
    { zCmdStartMove,       "startmove",       1, 1, "Start moving in the given direction.",
        { "Direction, should be one of: up, down, left, right, forward, back" } },
    { zCmdStopMove,        "stopmove",        1, 1, "Stop moving in the given direction.",
        { "Direction, should be one of: up, down, left, right, forward, back" } },
    { zCmdStartTurn,       "startturn",       1, 1, "Start turning in the given direction.",
        { "Direction, should be one of: up, down, left, right" } },
    { zCmdStopTurn,        "stopturn",        1, 1, "Stop turning in the given direction.",
        { "Direction, should be one of: up, down, left, right" } },
    { zCmdStartAim,        "startaim",        0, 0, "Start aiming using mouse motion.",
        { NULL } },
    { zCmdStopAim,         "stopaim",         0, 0, "Stop aiming using mouse motion.",
        { NULL } },
    { zCmdStartZoom,       "startzoom",       0, 0, "Start zooming using mouse motion.",
        { NULL } },
    { zCmdStopZoom,        "stopzoom",        0, 0, "Stop zooming using mouse motion.",
        { NULL } },
    { zCmdTextConsole,     "textconsole",     0, 0, "Open text console.",
        { NULL } },
    { zCmdSet,             "set",             2, 2, "Change the value of a variable.",
        { "Variable name", "Value to be set (default assumed if omitted)" } },
    { zCmdGet,             "get",             1, 1, "Display the value of a variable.",
        { "Variable name" } },
    { zCmdIncrease,        "increase",        2, 1,"Increase float/integer variable by given value.",
        { "Variable name", "Float/integer value (assumed 1 if omitted)" } },
    { zCmdDecrease,        "decrease",        2, 1,"Decrease float/integer variable by given value.",
        { "Variable name", "Float/integer value (assumed 1 if omitted)" } },
    { zCmdToggle,          "toggle",          1, 1,"Toggle integer variable.",
        { "Variable name" } },
    { zCmdRestartVideo,    "restartvideo",    0, 0,"Reinitialize OpenGL context.",
        { NULL } },
    { NULL, NULL, 0, 0, NULL, { NULL } },
};



