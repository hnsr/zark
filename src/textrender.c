#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "common.h"

#ifdef HAVE_CAPITALIZED_FTGL_H
#include <FTGL/FTGL.h>
#else
#include <FTGL/ftgl.h>
#endif


// XXX: Quick and ugly hack for some basic text-rendering

#define Z_CURSOR_BYTES 512

#define Z_CONSOLE_TEXTSIZE  13
#define Z_CONSOLE_FONT      "dejavu-sans-mono-bold.ttf"


static FTGLfont *font_console;
static GLubyte textcursor_console[Z_CURSOR_BYTES];



static void zInitCursor(GLubyte *pixels, int height)
{
    int i;

    assert( (height * 4) <= Z_CURSOR_BYTES );

    // Generate cursor image.
    for (i = 0; i < height; i++) {

        pixels[(i*4)]   = 255;
        pixels[(i*4)+1] = 255;
        pixels[(i*4)+2] = 255;
        pixels[(i*4)+3] = 255;
    }
}



// Destroy fonts and uninitialize.
void zTextRenderDeinit(void)
{
    if (font_console) {
        ftglDestroyFont(font_console);
        font_console = NULL;
    }
}



void zTextRenderInit(void)
{
    // Load font/cursor for console.
    if( !(font_console = ftglCreateBufferFont(zGetPath(Z_CONSOLE_FONT, "fonts", Z_FILE_TRYUSER))) ) {
        zDebug("Failed to open font.");
        exit(EXIT_FAILURE);
    }

    ftglSetFontFaceSize(font_console, Z_CONSOLE_TEXTSIZE, 0);
    zInitCursor(textcursor_console, Z_CONSOLE_TEXTSIZE);

    //zDebug("Font line height is %f", ftglGetFontLineHeight(font_console));
    //zDebug("Font global ascender height is %f", ftglGetFontAscender(font_console));
    //zDebug("Font global descender height is %f", ftglGetFontDescender(font_console));
    //ftglSetFontOutset(font, 4.0, 4.0);
}


// Returns cursor offset.
static float zTextCursorAdvance(ZTextBuffer *textbuf, unsigned int font_type)
{
    FTGLfont *font;

    //if (!initialized) zTextRenderInit();

    switch (font_type) {
        case Z_FONT_CONSOLE:
            font = font_console;
            break;
        default:
            assert(0 && "No valid font type given");
    }

    // Make sure buffer was initialized first
    if (textbuf->buf) {

        // Copy buf up until cursor_bytes into shadow buffer and use it to lookup advance.
        strncpy(textbuf->buf_shadow, textbuf->buf, textbuf->cursor_bytes);
        textbuf->buf_shadow[textbuf->cursor_bytes] = '\0';

        return ftglGetFontAdvance(font, textbuf->buf_shadow);
    }

    return 0;
}


void zTextRenderString(float x, float y, const char *str, unsigned int font_type)
{
    FTGLfont *font;

    switch (font_type) {
        case Z_FONT_CONSOLE:
            font = font_console;
            break;
        default:
            assert(0 && "No valid font type given");
    }

    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    ftglRenderFont(font, str, FTGL_RENDER_ALL);
    glPopMatrix();
}



void zTextRenderBuf(float x, float y, ZTextBuffer *textbuf, int draw_cursor, unsigned int font_type)
{
    FTGLfont *font;
    GLubyte *cursor;
    int cursor_height;

    float cursor_offset;

    switch (font_type) {
        case Z_FONT_CONSOLE:
            font = font_console;
            cursor = textcursor_console;
            cursor_height = Z_CONSOLE_TEXTSIZE;
            break;
        default:
            assert(0 && "No valid font type given");
    }

    glPushMatrix();

    // Don't bother if buffer contains no text.
    if (textbuf->bytes) {
        glTranslatef(x, y, 0.0f);
        ftglRenderFont(font, textbuf->buf, FTGL_RENDER_ALL);
    }

    if (draw_cursor) {

        cursor_offset = zTextCursorAdvance(textbuf, font_type);
        glWindowPos2f(x+cursor_offset, y-3.0f);
        glDrawPixels(1, cursor_height, GL_RGBA, GL_UNSIGNED_BYTE, cursor);

        //glTranslatef(cursor_offset, 0.0f, 0.0f);
        //ftglRenderFont(font, "_", FTGL_RENDER_ALL);
    }

    glPopMatrix();
}






