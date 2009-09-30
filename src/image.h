#ifndef __IMAGE_H__
#define __IMAGE_H__

typedef struct ZImage
{
    int width;
    int height;

    unsigned char *data;

} ZImage;

ZImage *zLoadImage(const char *filename);
void zDrawImage(ZImage *img, int x, int y);
void zDeleteImage(ZImage *img);

#endif
