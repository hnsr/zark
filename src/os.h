#ifndef __OS_H__
#define __OS_H__

// Anything OS specific goes in here (some file access bits, windowing, input, timing etc).

#define Z_EXISTS_REGULAR 1
#define Z_EXISTS_DIR     2

#define Z_FULLSCREEN_ON     1
#define Z_FULLSCREEN_OFF    2
#define Z_FULLSCREEN_TOGGLE 3

// Attempt to sleep for nsecs nanoseconds. Nothing is guaranteed.
void zSleep(float ms);

// Return timestamp in ms. Starts counting from the first call (first call always returns 0).
float zGetTimeMS(void);

// Open a window.
void zOpenWindow(void);

// Close our window.
void zCloseWindow(void);

// Set window fullscreen depending on state (see Z_FULLSCREEN_*).
void zSetFullscreen(int state);

// Enable/disable text input, only call after a window has been opened.
void zEnableTextInput(ZTextInputCallback cb);
void zDisableTextInput(void);

// Enable/disable tracking of mouse motion, this will hide the cursor. Only call after a window has
// been opened.
void zEnableMouse(void);
void zDisableMouse(void);

// Set swap interval.
void zSwapInterval(int interval);

// Swap front/back buffers.
void zSwapBuffers(void);

// Process input and windowing events.
void zProcessEvents(void);

// OS-specific clean up.
void zShutdown(void);

// Return pointer to statically allocated string with the path to the user data directory (i.e.
// C:\Windows\Users\<user>\Documents\zark on Windows or /home/<user>/.zark on *nix), or NULL if the
// user directory could not be determined, or created (if it didn't exist).
char *zGetUserDir(void);

// Returns one of Z_EXISTS_* (depending on type of file), or 0 if path doesn't exist.
int zPathExists(const char *path);

// Returns strings for each regular file found in directory 'path' (path is relative to data
// directory). Returns NULL when no more files are found. This function should only be called in a
// while loop that terminates when NULL is returned so it can clean up after itself. For ease of
// use, the string returned is a full path (to be used directly with fopen etc.).
char *zGetFileFromDir(const char *path);



#endif
