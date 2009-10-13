#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <GL/glew.h>
#include <stdlib.h>

#include "common.h"


void zPrintCurrentMVMatrix(void)
{
    float mat[16];

    glGetFloatv(GL_MODELVIEW_MATRIX, mat);

    zPrint("Current MV matrix:\n");
    zPrint("%6.2f %6.2f %6.2f %6.2f\n",   mat[0], mat[4],  mat[8], mat[12]);
    zPrint("%6.2f %6.2f %6.2f %6.2f\n",   mat[1], mat[5],  mat[9], mat[13]);
    zPrint("%6.2f %6.2f %6.2f %6.2f\n",   mat[2], mat[6], mat[10], mat[14]);
    zPrint("%6.2f %6.2f %6.2f %6.2f\n\n", mat[3], mat[7], mat[11], mat[15]);
}


// Print all keys, commands and keybindings.
void zPrintInputStuff(unsigned int what)
{
    unsigned int i, j, k;

    zDebug("");

    // Dump keys.
    if (what & Z_INPUT_KEYS) {

        zDebug("Listing all defined keys:");
        for (i=0; i<NUM_KEYS; i++) {
            zDebug("  ZKey %d: %s (%s)", i, zKeyName(i), zKeyDesc(i));
        }
        zDebug("");
    }


    // Dump command list.
    if (what & Z_INPUT_COMMANDS) {

        zDebug("Listing all defined commands:");
        for (i=0; commands[i].cmdfunc != NULL; i++) {

            zDebug("  ZCommand %d: %s (%s)", i, commands[i].keyword, commands[i].description);

            // Print argument descriptions.
            for (j=0; j<commands[i].numargs; j++) {
                zDebug("    arg %d: %s", j, commands[i].argdesc[j]);
            }
        }
        zDebug("");
    }

    // Dump variables list.
    if (what & Z_INPUT_VARIABLES) {

        zDebug("Listing all variables:");
        for (i=0; variables[i].varptr != NULL; i++) {

            zDebug("  ZVariable %d: %-16s (%7s) - %s", i, variables[i].name,
                zVariableType(variables[i].type), variables[i].description);

            // Print current/default value.
            if (variables[i].type == Z_VAR_TYPE_INT) {

                zDebug("    default value: %d", variables[i].int_default);
                zDebug("    current value: %d", *((int *)variables[i].varptr));

            } else if (variables[i].type == Z_VAR_TYPE_FLOAT) {
                
                zDebug("    default value: %f", variables[i].float_default);
                zDebug("    current value: %f", *((float *)variables[i].varptr));

            } else if (variables[i].type == Z_VAR_TYPE_STRING) {

                zDebug("    default value: %s", variables[i].str_default);
                zDebug("    current value: %s", (char *)variables[i].varptr);

            }
            zDebug("");
        }
        zDebug("");
    }

    // Display all defined keybindings
    if (what & Z_INPUT_KEYBINDINGS) {

        ZKeyBinding *kb;
        ZParsedCommand *pcmd;

        zDebug("Listing all defined keybindings:");
        for (i = 0; i < numkeybindings; i++) {

            kb = &(keybindings[i]);

            zDebug("  ZKeyBinding %d: %s", i, zKeyEventName(&kb->keyevent));

            for (j = 0; j < kb->numcommands; j++) {

                pcmd = &(kb->parsedcmds[j]);

                zDebug("    bound to \"%s\" with %d args", pcmd->command->keyword, pcmd->numargs);

                for (k = 0; k < pcmd->numargs; k++) {
                    zDebug("      %d: %6.2f, %3d, \"%12s\", variable %s",
                        k,
                        pcmd->args[k].float_arg,
                        pcmd->args[k].int_arg,
                        pcmd->args[k].str_arg,
                        pcmd->args[k].var_arg != NULL ?  pcmd->args[k].var_arg->name : "(none)"
                    );
                }
            }

            zDebug("    cmdline: \"%s\"", kb->cmdstring);
            zDebug("");
        }
        zDebug("");
    }
}


// Simple wrapper for if I ever want to redirect this or do something else than just write to
// stdout. Maybe this belongs in os*.c, so that I can for example redirect it to a manually opened
// console using Win32 API calls.
void zPrint(char *format, ...)
{
    va_list args;

    va_start(args, format);
    vprintf(format, args);
    va_end(args);

}


void zError(char *format, ...)
{
    va_list args;

    fprintf(stderr, "ERROR: ");

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
}

// TODO: Make this exit?
void zFatal(char *format, ...)
{
    va_list args;

    fprintf(stderr, "FATAL: ");

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
}


void zWarning(char *format, ...)
{
    va_list args;

    fprintf(stderr, "WARNING: ");

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
}


#ifdef DEBUG
void zDebug(char *format, ...)
{
    va_list args;

    fprintf(stderr, "DEBUG: ");

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
}
#endif


const char *zGetColorString(float *color)
{
    static char color_string[64];

    // This could, in theory, overflow the buffer, could use snprintf but not sure if I get that on
    // win32 (c99)..
    sprintf(color_string, "{ %.2f, %.2f, %.2f, %.2f }", color[0], color[1], color[2], color[3]);

    return color_string;
}


// Returns a pointer that starts at the first character of the file extension in filename, or if no
// extension is found, to the trailing null byte (i.e. an empty string).
char *zGetFileExtension(const char *filename)
{
    char *pos = (char *) filename, *s = (char *) filename;

    assert(filename);

    // Find last directory separator. Since this can be either '\' or '/', I check for both.
    while (*s != '\0') {
        if (*s == '\\' || *s == '/')
            pos = s;
        s++;
    }

    // Scan through the filename, if a period is found, break and return pointer to the character
    // that follows it, or keep on scanning till trailing null-byte.
    s = pos; // Start looking at last directory separator.
    while ( *s != '\0' ) {
        if ( *s++ == '.') break;
    }

    return s;
}



// Rewrite directory separators to native ones in path.
void zRewriteDirsep(char *path)
{
    //char *start = path;
    
    assert(path);

    //zDebug("Replacing dirseps in \"%s\".", path);

    while (*path != '\0') {

        if (*path == '/' || *path == '\\')
            *path = *Z_DIR_SEPARATOR;

        path++;
    }

    //zDebug("  result: \"%s\".", start);
}



// Construct path using base path component of filename, with sibling appended to it. See zGetPath
// on how to treat returned string.
const char *zGetSiblingPath(const char *filename, const char *sibling)
{
    static char path[Z_MAX_PATH];
    int basename_start = 0;
    const char *s = filename;

    if (strlen(filename) >= Z_MAX_PATH) {
        zError("%s: File path too long.", __func__);
        return NULL;
    }

    path[0] = '\0';
    strcat(path, filename);

    // Figure out where the base part of filename starts and truncate path at that point.
    while ( *s != '\0' ) {

        if ( *s == '\\' || *s == '/') {
            basename_start = (s-filename)+1;
        }
        s++;
    }
    path[basename_start] = '\0';

    // Check that there's enough room to append sibling to path
    if ( (strlen(sibling) + strlen(path)) >= Z_MAX_PATH) {
        zError("%s: File path too long.", __func__);
        return NULL;
    }

    strcat(path, sibling);

    return (const char *) path;
}



// Construct a full path for given filename and prefix, prepending either the user or system data
// directory to it; if TRY_USER is specified in flags, an extra check is done to see if the
// requested filename/prefix exists in the user data directory, and if so, that path is returned,
// else it is returned for the system data directory (without checking wether or not it exists). The
// returned string is statically allocated, should not be freed, and is modified on the next call.
// Returns NULL if the path was too long (>Z_MAX_PATH).
const char *zGetPath(const char *filename, const char *prefix, int flags)
{
    static char path[Z_MAX_PATH];
    char *userdir = zGetUserDir();
    size_t reqsize = 0;

    assert(filename && strlen(filename));

    // Truncate or else we append to the path constructed on the previous invocation.
    path[0] = '\0';

    // Try looking for file in user direcotry first if desired.
    if (flags & Z_FILE_TRYUSER) {

        // Check that the full path doesn't exceed Z_MAX_PATH.
        reqsize += strlen(userdir) + 1; // +1 for directory separator.
        if (prefix && strlen(prefix) > 0) reqsize += strlen(prefix) + 1;
        reqsize += strlen(filename);

        if (reqsize >= Z_MAX_PATH) {
            zError("%s: Resulting file path too long for file \"%s\" with prefix \"%s\".", __func__,
                filename, prefix);
            return NULL;
        }

        strcat(path, userdir);
        strcat(path, Z_DIR_SEPARATOR);
        if (prefix) {
            strcat(path, prefix);
            strcat(path, Z_DIR_SEPARATOR);
        }

        strcat(path, filename);

        if (flags & Z_FILE_REWRITE_DIRSEP)
            zRewriteDirsep(path);

        if (zFileExists(path))
            return path;

        //zDebug("%s: \"%s\" doesn't exist, trying system data directory.", __func__, path);
    }

    // Same thing but this time for the system root dir.
    reqsize = 0;
    path[0] = '\0';

    reqsize += strlen(Z_DIR_SYSDATA) + 1;
    if (prefix && strlen(prefix) > 0) reqsize += strlen(prefix) + 1;
    reqsize += strlen(filename);

    if (reqsize >= Z_MAX_PATH) {
        zError("%s: Resulting file path too long for file \"%s\" with prefix \"%s\".", __func__,
            filename, prefix);
        return NULL;
    }

    strcat(path, Z_DIR_SYSDATA);
    strcat(path, Z_DIR_SEPARATOR);
    if (prefix) {
        strcat(path, prefix);
        strcat(path, Z_DIR_SEPARATOR);
    }
    strcat(path, filename);

    if (flags & Z_FILE_REWRITE_DIRSEP)
        zRewriteDirsep(path);


    return path;
}



// Tries to open the file at root+prefix+filename. If Z_FILE_TRYUSER is set in flags, it will try
// opening using the user data directory as root first. If that fails or of the Z_FILE_TRYUSER bit
// is not set it will use the system data directory as root.
FILE *zOpenFile(const char *filename, const char *prefix, int flags)
{
    char *mode;
    const char *path;

    assert(filename && strlen(filename) > 0);

    // Set file access mode.
    if (flags & Z_FILE_WRITE) mode = "w+b";
    else                      mode = "rb";

    path = zGetPath(filename, prefix, flags);
    
    //zDebug("Opening \"%s\" with path \"%s\".", filename, path);

    return fopen(path, mode);
}



// Read entire contents of file into NULL-terminated string and return a pointer to it. Caller
// should free the string when it is done using it. Returns NULL on error.
char *zGetStringFromFile(const char *filename)
{
#define READ_INC 8
    FILE *file = fopen(filename, "rb");
    char *result = NULL, *tmp;
    size_t result_size = 0, bytes_read = 0;

    if (!file) return NULL;

    while (!feof(file)) {

        // Make room for READ_INC more bytes and reading another block.
        if ( (tmp = realloc(result, result_size+READ_INC)) ) {
            result_size += READ_INC;
            result = tmp;
        } else
            goto zGetStringFromFile_error0;

        bytes_read += fread(result+bytes_read, 1, READ_INC, file);
    }

    result[bytes_read] = '\0';
    return result;

zGetStringFromFile_error0:
    free(result);
    return NULL;
}




// Return hash for given string and table size. Current hash function may not be ideal but seems
// decent enough for my purposes.
// Some links if I ever want to improve this:
// http://burtleburtle.net/bob/hash/doobs.html
// http://www.azillionmonkeys.com/qed/hash.html
unsigned int zHashString(const char *str, int tablesize)
{
    unsigned int tmp = 0, hash = 0;
    int shift = 0;

    assert(str);

    // Turn every 4 characters into an uint and add to hash.
    while ( *str != '\0' ) {

        tmp += *str++ << shift++;

        if (shift == 4) {
            hash += tmp;
            tmp = shift = 0;
        }
    }

    // Add any left overs.
    hash += tmp;

    return hash % tablesize;
}



// Return true if the first character in str is not a control character.
int zUTF8IsPrintable(const char *str)
{
    if (str && *str != '\0') {
        if ( !( (unsigned char) *str < 0x20 || (unsigned char) *str == 0x7f ) ) {
            return 1;
        }
        //zDebug("%s: str was nonprintable, first char is %d", __func__, (char) *str);
    }
    return 0;
}



// Returns pointer to previous character in src relative to pos.
char *zUTF8FindPrevChar(const char *src, const char *pos)
{
    while (pos > src) {

        pos--;

        // If the current character is not a continuation byte, we've reached the previous
        // character.
        if ( (*pos & 0xc0) != 0x80)
            return (char *) pos;
    }

    return (char *) src;
}



char *zUTF8FindNextChar(const char *pos)
{
    while (*pos != '\0') {
        
        pos++;

        // If the next character is not a continuation byte, we've reached the next character.
        if ( (*pos & 0xc0) != 0x80)
            return (char *) pos;
    }

    return (char *) pos;
}

// Time elapsed in seconds.
float time_elapsed;

// Returns the amount of time in ms since last call. Will block (by looping/sleeping) if r_maxfps is
// set to maintain framerate.
float zFrameTime(void)
{

    static float this_time, frame_time, last_time, ideal_frame_time, error, sleep_time;
    static float next_fps_print, last_fps_time, dtime;
    static unsigned int last_fps_frame_count, dframes;

    // Don't risk sleeping too long.
    #define FPS_SLEEP_MARGIN 1.0f
    
    // To limit the FPS, we loop until the time that has passed since we last started drawing a
    // frame (frame_time) is equal to or larger than the 'ideal' time of each frame (1000/r_maxfps).
    // If we haven't reached ideal frame time yet, see if there's enough time left to throw in a
    // sleep call to limit CPU load.
    if (r_maxfps > 0.0f) {

        // Recalc ideal frame time. A bit of a waste to do it like this but since it can be changed
        // at any time I will have to (unless I ever decide to implement setter functions for vars).
        ideal_frame_time = 1000.0f/r_maxfps;

        // Loop infinitely until we reach the ideal frame time.
        while (1) {

            this_time = zGetTimeMS();
            frame_time = this_time - last_time;
            
            //zDebug("this_time %.2f, frame_time, %.2f", this_time, frame_time);

            // See if we are at or over the ideal frame time. The amount of time we over- or under-
            // shot last frame is taken into account (error) to balance out the framerate.
            if ((frame_time + error) >= ideal_frame_time) {
                error += frame_time - ideal_frame_time;
                if (error > 5.0f) error = 5.0f; // Clamp error so we don't get crazy high values if
                                                // ideal framereate can't be attained.
                //zDebug("error %.3f  ideal_frame_time %.3f  frame_time %.3f",error,ideal_frame_time,
                //    frame_time);
                break;
            } else if ( (sleep_time = ideal_frame_time - frame_time) > FPS_SLEEP_MARGIN ) {
                zSleep(sleep_time-FPS_SLEEP_MARGIN);
            }
        }
    } else {
        this_time = zGetTimeMS();
        frame_time = this_time - last_time;
    }

    last_time = this_time;
    time_elapsed = this_time*0.001f;

    // Keep track of FPS, even if printfps is 0...
    if (this_time >= next_fps_print) {
        
        dframes = frame_count - last_fps_frame_count;
        dtime = this_time - last_fps_time;

        // Make sure I only print if enabled, and not if we're doing text input or just started
        // running..
        if (printfps &&  next_fps_print > 0  && !text_input) {
            zPrint("%d frames rendered in %.2f ms (%.2f fps).\n", dframes, dtime,
                (((float) dframes*1000) / dtime));
            //zDebug("time_elapsed = %.3f", time_elapsed);
        }

        next_fps_print += printfpstime;
        last_fps_time = this_time;
        last_fps_frame_count = frame_count;
    }

    return frame_time;
}

