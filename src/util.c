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


const char *zGetFloat3String(const float *f)
{
    static char str[32];

    int len = snprintf(str, 32, "{ %.2f, %.2f, %.2f }", f[0], f[1], f[2]);

    assert(len < 32);

    return str;
}

const char *zGetFloat4String(const float *f)
{
    static char str[32];

    int len = snprintf(str, 32, "{ %.2f, %.2f, %.2f, %.2f }", f[0], f[1], f[2], f[3]);

    assert(len < 32);

    return str;
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
    assert(path);

    while (*path != '\0') {

        if (*path == '/' || *path == '\\')
            *path = *Z_DIR_SEPARATOR;

        path++;
    }
}



// Construct path using base path component of filename, with sibling appended to it. See zGetPath
// on how to treat returned string.
const char *zGetSiblingPath(const char *filename, const char *sibling)
{
    static char path[Z_PATH_SIZE];
    int basename_start = 0;
    const char *s = filename;

    if (strlen(filename) >= Z_PATH_SIZE) {
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
    if ( (strlen(sibling) + strlen(path)) >= Z_PATH_SIZE) {
        zError("%s: File path too long.", __func__);
        return NULL;
    }

    strcat(path, sibling);

    return (const char *) path;
}



// Construct a path from datadir+prefix+filename. The path returned is statically allocated and
// valid until the next call of this function. The 'datadir' component depends on flags. If
// FORCEUSER is given, this will the user data directory. If TRYUSER is given, 'datadir' will be the
// user data directory if the file exists there, or the system data directory otherwise. If neither
// flag is given it will always be the system data dir. Returns NULL if the path's length exceeded
// Z_PATH_SIZE-1 or if FORCEUSER was specified, and the user data directory could not be determined.
// If flags has Z_FILE_REWRITE_DIRSEP set, directory seperators are rewritten to those native of
// the platform.
// XXX: Maybe I should just make Z_FILE_REWRITE_DIRSEP default, overhead of rewriting is going to be
// nothing compared to processing the file and doing it by default should make things simpler and
// more foolproof.. I think
const char *zGetPath(const char *filename, const char *prefix, int flags)
{
    static char path[Z_PATH_SIZE];
    char *userdir = zGetUserDir();
    size_t reqsize = 0;
    size_t prefix_len = prefix ? strlen(prefix) : 0;
    size_t filename_len = filename ? strlen(filename) : 0;

    assert(filename_len && strlen(Z_DIR_SYSDATA));

    // Truncate or else we append to the path constructed on the previous invocation.
    path[0] = '\0';

    // As a special case, just return NULL if FORCEUSER was specified and userdir is invalid.
    if (flags & Z_FILE_FORCEUSER && !userdir)
        return NULL;

    // Try looking for file in user directory first if desired (and if userdir is valid).
    if ( userdir && (flags & (Z_FILE_TRYUSER | Z_FILE_FORCEUSER)) ) {


        // Check that the full path doesn't exceed Z_PATH_SIZE.
        reqsize += strlen(userdir) + 1; // +1 for directory separator.
        reqsize += prefix_len ? prefix_len + 1 : 0;
        reqsize += filename_len;

        if (reqsize >= Z_PATH_SIZE) {
            zError("%s: Resulting file path too long for file \"%s\" with prefix \"%s\".", __func__,
                filename, prefix);
            return NULL;
        }

        strcat(path, userdir);
        strcat(path, Z_DIR_SEPARATOR);
        if (prefix_len) {
            strcat(path, prefix);
            strcat(path, Z_DIR_SEPARATOR);
        }
        strcat(path, filename);

        // Just in case prefix (and/or filename?) contained non-native dirseps.
        if (flags & Z_FILE_REWRITE_DIRSEP)
            zRewriteDirsep(path);

        // Return path unconditionally if FORCEUSER was given, else check for existance.
        if ( (flags & Z_FILE_FORCEUSER) || (zPathExists(path) == Z_EXISTS_REGULAR) )
            return path;
    }

    // Same thing but this time for the system root dir.
    reqsize = 0;
    path[0] = '\0';

    reqsize += strlen(Z_DIR_SYSDATA) + 1;
    reqsize += prefix_len ? prefix_len + 1 : 0;
    reqsize += filename_len;

    if (reqsize >= Z_PATH_SIZE) {
        zError("%s: Resulting file path too long for file \"%s\" with prefix \"%s\".", __func__,
            filename, prefix);
        return NULL;
    }

    strcat(path, Z_DIR_SYSDATA);
    strcat(path, Z_DIR_SEPARATOR);
    if (prefix_len) {
        strcat(path, prefix);
        strcat(path, Z_DIR_SEPARATOR);
    }
    strcat(path, filename);

    if (flags & Z_FILE_REWRITE_DIRSEP)
        zRewriteDirsep(path);

    return path;
}



// Tries to open the file at datadir+prefix+filename. For the meaning of datadir path component and
// 'flags', see zGetPath. If fullpath is not NULL, it will be pointed to a statically allocated
// string that contains the full path of the file being opened, or if constructing the path failed,
// simply to 'filename'. 'fullpath' remains valid until the next call of either this function or
// zGetPath. Returns valid file handle on success, or NULL on failure.
FILE *zOpenFile(const char *filename, const char *prefix, const char **fullpath, int flags)
{
    char *mode;
    const char *path;

    assert(filename && strlen(filename) > 0);

    // Set file access mode.
    if (flags & Z_FILE_WRITE) mode = "w+b";
    else                      mode = "rb";

    if ( (path = zGetPath(filename, prefix, flags)) ) {
        if (fullpath) *fullpath = path;
        return fopen(path, mode);
    } else {
        if (fullpath) *fullpath = filename;
        return NULL;
    }
}



// Read entire contents of file into NULL-terminated string and return a pointer to it. Caller
// should free the string when it is done using it. Returns NULL on error.
char *zGetStringFromFile(const char *filename)
{
#define READ_INC 512
    FILE *file = fopen(filename, "rb");
    char *result = NULL, *tmp;
    size_t result_size = 0, bytes_read = 0;

    if (!file) return NULL;

    while (!feof(file)) {

        // Make room for READ_INC more bytes and reading another block.
        if ( (tmp = realloc(result, result_size+READ_INC)) ) {
            result_size += READ_INC;
            result = tmp;
        } else {
            free(result);
            zError("%s: Failed to allocated memory.", __func__);
            return NULL;
        }

        bytes_read += fread(result+bytes_read, 1, READ_INC, file);
    }

    result[bytes_read] = '\0';
    return result;
}



// Return hash for given string and table size. Current hash function may not be ideal but seems
// decent enough for my purposes. Some links if I ever want to improve this:
// http://burtleburtle.net/bob/hash/doobs.html / http://www.azillionmonkeys.com/qed/hash.html
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


// Return pointer to the next character after pos.
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

    #define FPS_SLEEP_MARGIN 1.0f // Don't sleep for too short periods, not sure if it matters..

    // Don't maintain framerate if r_maxfps is 0
    if (r_maxfps > 0.0f) {

        ideal_frame_time = 1000.0f/r_maxfps; // FIXME: Use reciprocal? Would need to check if
                                             // r_maxfps changes and recalc it..

        // Wait until ideal frame time is reached.
        while (1) {

            this_time = zGetTimeMS();
            frame_time = this_time - last_time;

            // See if we are at or over the ideal frame time. The amount of time we over- or under-
            // shot last frame is taken into account (error) to balance out the framerate.
            if ((frame_time + error) >= ideal_frame_time) {
                error += frame_time - ideal_frame_time;

                // Clamp error so it can't reach crazy values if framerate can't be maintained.
                if (error > 5.0f) error = 5.0f;
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

    // Keep track of FPS, even if printfps is 0... FIXME: this can probably be improved
    if (this_time >= next_fps_print) {

        dframes = frame_count - last_fps_frame_count;
        dtime = this_time - last_fps_time;

        if (printfps &&  next_fps_print > 0  && !text_input) {
            zPrint("%d frames rendered in %.2f ms (%.2f fps).\n", dframes, dtime,
                (((float) dframes*1000) / dtime));
        }

        next_fps_print += printfpstime;
        last_fps_time = this_time;
        last_fps_frame_count = frame_count;
    }

    return frame_time;
}


