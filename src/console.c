#define _BSD_SOURCE
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"


static int console_active;
static ZTextBuffer cmdbuf;
static char *prevcmd;


static void zTextConsoleInputHandler(ZKeyEvent *zkev, char *str)
{
    if (zkev && zkev->key == Z_KEY_ESCAPE) {

        // Escape from console.
        zDisableTextConsole();

    } else if (zkev && (zkev->key == Z_KEY_ENTER || zkev->key == Z_KEY_KP_ENTER) ) {

        zPrint("\n");

        if (cmdbuf.bufsize) // Only execute if cmdbuf wasn't empty
        {
            // Store a copy of the given command (unless it was empty)
            free(prevcmd);
            prevcmd = strdup(cmdbuf.buf);

            // Execute cmdstring in text buffer and reset.
            zLuaRunString(Z_VM_CONSOLE, cmdbuf.buf);
            zResetTextBuffer(&cmdbuf);
        }

        // Reprint prompt (if we didn't just exit text input mode).
        if (text_input) {
            fputs("> ", stdout);
            fflush(stdout);
        }
    } else if (zkev && zkev->key == Z_KEY_UP) {

        // Copy previous command into textbuffer.
        if (prevcmd) {
            zResetTextBuffer(&cmdbuf);
            zUpdateTextBuffer(&cmdbuf, NULL, prevcmd);
        }
    } else if (zkev && zkev->key == Z_KEY_DOWN) {

        zResetTextBuffer(&cmdbuf);

    } else {

        // Echo on stdout.
        if (str && zUTF8IsPrintable(str)) {
            fputs(str, stdout);
            fflush(stdout);
        }

        zUpdateTextBuffer(&cmdbuf, zkev, str);
    }
}



void zEnableTextConsole(void)
{
    zReleaseKeys(0);
    zEnableTextInput(zTextConsoleInputHandler);
    console_active = 1;

    fputs("> ", stdout);
    fflush(stdout);
}



void zDisableTextConsole(void)
{
    zPrint("\n");

    console_active = 0;
    zDisableTextInput();
}



// Quick hack for showing console commandline in opengl window.
void zDrawConsole(void)
{
    if (console_active) {

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        // Draw prompt first.
        zTextRenderString(5.0f, 7.0f, "> ", Z_FONT_CONSOLE);

        // Then whatever's been typed in on the commandline.
        zTextRenderBuf(20.0f, 7.0f, &cmdbuf, 1, Z_FONT_CONSOLE);
    }
}



