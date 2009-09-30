#ifndef __RENDERER_H__
#define __RENDERER_H__


extern int viewport_width;
extern int viewport_height;

extern int renderer_active;



void zRendererInfo(void);

void zRendererInit(void);

void zRendererDeinit(void);

int zCheckOpenGLSupport(void);

void zCheckRendererError(void);

void zReshapeViewport(void);

void zApplyGUITransforms(void);

#endif
