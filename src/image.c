#include <GL/glew.h>
#include <IL/il.h>
#include <IL/ilu.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "common.h"


ZImage *zLoadImage(const char *filename)
{
    ZImage *img = NULL;

    ILuint image_name;
    ILenum error;
    ILconst_string error_str;
    ILint cur_format;

    ilGenImages(1, &image_name);
    ilBindImage(image_name);

    // Force OpenGL-compatible row order.
    ilEnable(IL_ORIGIN_SET);
    ilOriginFunc(IL_ORIGIN_LOWER_LEFT);

    if (ilLoadImage(filename)) {

        cur_format = ilGetInteger(IL_IMAGE_FORMAT);

        if ( cur_format == IL_COLOR_INDEX ) {

            zDebug("%s: Image format for \"%s\" is COLOR_INDEX, converting to RGBA", __func__,
                filename);

            if ( !ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE) ) {

                zWarning("%s: Failed to convert color indexed image \"%s\" to RGBA.", __func__,
                    filename);
                ilDeleteImages(1, &image_name);
                return NULL;
            }
        }

        img = (ZImage *) malloc(sizeof(ZImage));
        img->width  = ilGetInteger(IL_IMAGE_WIDTH);
        img->height = ilGetInteger(IL_IMAGE_HEIGHT);
        img->data   = (unsigned char *) malloc(img->width * img->height * 4);

        if( !ilCopyPixels(0, 0, 0, img->width, img->height, 1, IL_RGBA, IL_UNSIGNED_BYTE, img->data)
            ) {

            error = ilGetError();
            error_str = iluErrorString(error);
            zWarning("%s: Failed to read image data for \"%s\". (Last error given by DevIL was"
                " \"%s (code %d)\".)", __func__, filename, error_str, error);
            free( (void *) img->data );
            free(img);
            ilDeleteImages(1, &image_name);
            return NULL;
        }

    } else {

        //zWarning("%s: Failed to load image \"%s\".", __func__, filename);
        ilDeleteImages(1, &image_name);
        return NULL;
    }

    ilDeleteImages(1, &image_name);

    return img;
}


void zDrawImage(ZImage *img, int x, int y)
{
    assert(NULL != img);

    glWindowPos2i(x, y);
    glDrawPixels(img->width, img->height, GL_RGBA, GL_UNSIGNED_BYTE, img->data);
}


void zDeleteImage(ZImage *img)
{
    assert(NULL != img);
    assert(NULL != img->data);

    free(img->data);
    free(img);
}

