#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdio.h>

#define Z_INPUT_COMMANDS    1
#define Z_INPUT_VARIABLES   2
#define Z_INPUT_KEYS        4
#define Z_INPUT_KEYBINDINGS 8

// Flags for opening files / constructing paths.
#define Z_FILE_TRYUSER        1 // Try opening/looking for file from/in user directory first.
#define Z_FILE_WRITE          2 // Open for writing, file will be opened for reading if not set.
#define Z_FILE_REWRITE_DIRSEP 4 // Transform directory seperators in filenames to native ones. Only 
                                // needed if provided filenames are from external sources (material
                                // definitions, console commands).


// Debugging stuff
void zPrintCurrentMVMatrix(void);

void zPrintInputStuff(unsigned int what);

void zPrint(char *format, ...);

void zError(char *format, ...);

void zFatal(char *format, ...);

void zWarning(char *format, ...);

// If not debugging, remove all Debug calls from the code
#ifndef DEBUG
#define zDebug(...)
#else
void zDebug(char *format, ...);
#endif

const char *zGetColorString(float *color);


// File stuff
char *zGetFileExtension(const char *filename);

void zRewriteDirsep(char *path);

const char *zGetSiblingPath(const char *filename, const char *sibling);

const char *zGetPath(const char *filename, const char *prefix, int flags);

FILE *zOpenFile(const char *filename, const char *prefix, int flags);

char *zGetStringFromFile(const char *filename);



// Misc stuff
unsigned int zHashString(const char *str, int tablesize);

int zUTF8IsPrintable(const char *str);

char *zUTF8FindPrevChar(const char *src, const char *pos);

char *zUTF8FindNextChar(const char *pos);


extern float time_elapsed;
float zFrameTime(void);

// Some macros which make setting an array of color/position values slightly more
// convenient/compact.
#define zSetFloat4(array, a ,b ,c, d) ((float *)(array))[0] = (a);\
                                      ((float *)(array))[1] = (b);\
                                      ((float *)(array))[2] = (c);\
                                      ((float *)(array))[3] = (d);

#define zSetFloat3(array, a ,b ,c) ((float *)(array))[0] = (a);\
                                   ((float *)(array))[1] = (b);\
                                   ((float *)(array))[2] = (c);

// Quick and dirty byteswapping macros.
#define zSwap16(x) ( \
        ( ((x) & 0x00FF) << 8) | \
        ( ((x) & 0xFF00) >> 8)   )

#define zSwap32(x) ( \
        ( ((x) & 0x000000FF) << 24) | \
        ( ((x) & 0x0000FF00) << 8 ) | \
        ( ((x) & 0x00FF0000) >> 8 ) | \
        ( ((x) & 0xFF000000) >> 24)   )

#define zSwap64(x) ( \
        ( ((x) & 0x00000000000000FF) << 56) | \
        ( ((x) & 0x000000000000FF00) << 40) | \
        ( ((x) & 0x0000000000FF0000) << 24) | \
        ( ((x) & 0x00000000FF000000) << 8 ) | \
        ( ((x) & 0x000000FF00000000) >> 8 ) | \
        ( ((x) & 0x0000FF0000000000) >> 24) | \
        ( ((x) & 0x00FF000000000000) >> 40) | \
        ( ((x) & 0xFF00000000000000) >> 56)   )

#ifdef WORDS_BIGENDIAN
    #define zSwap16FromBE(x) (x)
    #define zSwap16FromLE(x) zSwap16(x)
    #define zSwap32FromBE(x) (x)
    #define zSwap32FromLE(x) zSwap32(x)
    #define zSwap64FromBE(x) (x)
    #define zSwap64FromLE(x) zSwap64(x)
#else
    #define zSwap16FromBE(x) zSwap16(x)
    #define zSwap16FromLE(x) (x)
    #define zSwap32FromBE(x) zSwap32(x)
    #define zSwap32FromLE(x) (x)
    #define zSwap64FromBE(x) zSwap64(x)
    #define zSwap64FromLE(x) (x)
#endif

#endif
