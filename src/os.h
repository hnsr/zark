#ifndef __OS_H__
#define __OS_H__

// Anything OS specific goes in here (some file access bits, windowing, input, timing etc).


// Attempt to sleep for nsecs nanoseconds. Nothing is guaranteed.
void zSleep(float ms);

// Return timestamp in ms. Starts counting from the first call (first call always returns 0).
float zGetTimeMS(void);

// Open a window.
void zOpenWindow(int width, int height);

// Close our window.
void zCloseWindow(void);

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

// Return pointer to user data directory (i.e. C:\Windows\Users\<user>\Documents\Zark on Win32 or
// /home/<user>/.zark on *nix). The string returned is is statically allocated and should not be
// freed or modified. Always returns a valid pointer though the string may be empty.
char *zGetUserDir(void);

// Returns 1 if path exists and is a regular file.
int zFileExists(char *path);

// Returns strings for each regular file found in directory 'path' (path is relative to data
// directory). Returns NULL when no more files are found. This function should only be called in a
// while loop that terminates when NULL is returned so it can clean up after itself. For ease of
// use, the string returned is a full path (to be used directly with fopen etc.).
char *zGetFileFromDir(const char *path);



#endif
