#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef WIN32

	// Must include these for the exit/assert macros below.
    #include <stdio.h>
    #include <stdlib.h>

    // inline keyword causes syntax errors in msvc
    #define inline

    // http://msdn.microsoft.com/en-us/library/ms235392(VS.80).aspx
    #define strcasecmp _stricmp
    // .. and some others
    #define strdup _strdup
    #define fileno _fileno
    #define snprintf _snprintf

    #ifdef _DEBUG
        #define DEBUG
    #endif

    #include "../win32/config.h"

    #include <conio.h>

    // MS compiler doesn't have __func__, but does have __FUNCTION__, which does the same.
    #define __func__ __FUNCTION__

    // Make sure user has a chance to see console output before program exits.
    #define exit(x) printf("Press any key to exit...\n");\
                    _getch();\
                    exit(x);

    // I don't like CRT assert much so I roll my own here..
    #undef assert

    #ifdef DEBUG
        #define assert(x) \
            if ( !(x) ) { \
                zError("Assertion \"" #x "\" failed in \"%s\" on line %d.", __FILE__, __LINE__);\
                exit(EXIT_FAILURE); \
            }
    #else
        #define assert(x)
    #endif

    // TODO: Define this in config.h, or maybe just get rid of config.h for win32 and put it all in
    // here...
    #define Z_DIR_SEPARATOR "\\"         // The directory separator character used on this platform
                                         // (must be a single character).
    #define Z_DIR_USERDATA  PACKAGE_NAME // Data directory relative to user home directory (or
                                         // My Documents) on Win32).
    #define Z_DIR_SYSDATA   "data"       // System-wide data directory.

#else

    #ifdef HAVE_CONFIG_H
    #include "config.h"
    #endif

    #define Z_DIR_SEPARATOR "/"
    #define Z_DIR_USERDATA  "." PACKAGE_NAME
    #define Z_DIR_SYSDATA   ASSET_DIR

#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef MIN
#define MIN(x, y) ((x)<(y) ? (x):(y))
#endif

// Size of array for storing names of recources (meshes, textures, materials etc.)
#define Z_RESOURCE_NAME_SIZE 128

// Size of buffers for filenames/paths. Using 260 since that happens to be the limit for win32
// file-related API, see
// http://msdn.microsoft.com/en-us/library/aa365247(VS.85).aspx#maximum_path_length
#define Z_PATH_SIZE 260


// On startup, zark will look for FILE_CONFIG and FILE_KEYBINDINGS under DIR_USERDATA first, if not
// found, it will look under DIR_SYSDATA instead. These should always be available under DIR_SYSDATA
// and provide some sane defaults. On exit, keybindings are dumped to FILE_KEYBINDINGS under the
// DIR_USERDATA, and non-default variables are dumped to FILE_CONFIG under DIR_USERDATA.
#define Z_FILE_CONFIG      "config.zvar"
#define Z_FILE_KEYBINDINGS "keybindings.zkbd"
#define Z_FILE_STARTUP     "startup.zcmd"


#include "renderer.h"
#include "shader.h"
#include "image.h"
#include "material.h"
#include "mesh.h"
#include "zmath.h"
#include "camera.h"
#include "main.h"
#include "util.h"
#include "input.h"
#include "textrender.h"
#include "os.h"
#include "variables.h"
#include "command.h"
#include "console.h"
#include "script.h"


#endif
