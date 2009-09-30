#ifndef __TEXTRENDER_H__
#define __TEXTRENDER_H__


#define Z_FONT_CONSOLE 1


void zTextRenderInit(void);

void zTextRenderDeinit(void);

void zTextRenderString(float x, float y, const char *str, unsigned int font_type);

void zTextRenderBuf(float x, float y, ZTextBuffer *textbuf, int draw_cursor, unsigned int font_type);

#endif
