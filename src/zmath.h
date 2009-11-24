#ifndef __MATH_H__
#define __MATH_H__

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327
#endif

#ifndef DEG_TO_RAD
#define DEG_TO_RAD(degrees) ((degrees) * (float) 0.017453293 )
#endif


#pragma pack(push,4)
typedef struct ZVec2
{
    float x, y;

} ZVec2;


typedef struct ZVec3
{
    float x, y, z;

} ZVec3;


typedef struct ZVec4
{
    float x, y, z, w;

} ZVec4;
#pragma pack(pop)

// XXX: Maybe I should turn some of these in macros or inline functions?

float zLength3(ZVec3 *v);
float zLength2(ZVec2 *v);

void  zNormalize3(ZVec3 *v);

float zDot3(ZVec3 *a, ZVec3 *b);

ZVec3 zCross3(ZVec3 *a, ZVec3 *b);

void  zScaleVec3(ZVec3 *v, float s);

void  zAddVec3(ZVec3 *a, ZVec3 *b);

void  zSubtractVec3(ZVec3 *a, ZVec3 *b);
void  zSubtractVec2(ZVec2 *a, ZVec2 *b);
void  zSubtractVec3r(ZVec3 *r, ZVec3 *a, ZVec3 *b);

void  zTransform3Vec3(float *m, ZVec3 *v);

void  zTransform4Vec3(float *m, ZVec3 *v);

void  zTransform4Point3(float *m, ZVec3 *p);

void  zAddMatrix3(float *a, float *b);

void  zAddMatrix4(float *a, float *b);

void  zScaleMatrix3(float *a, float s);

void  zScaleMatrix4(float *a, float s);

void  zMultMatrix3(float *r, float *a, float *b);

void  zMultMatrix4(float *r, float *a, float *b);

float zDetMatrix3(float *m);

float zDetMatrix4(float *m);

void zCalcTriangleTB(ZVec3 *T, ZVec3 *B, ZVec3 *v0,  ZVec3 *v1,  ZVec3 *v2,
                                         ZVec2 *vt0, ZVec2 *vt1, ZVec2 *vt2);


// Math related debugging stuff
void  zPrintVec3(ZVec3 *v);

void  zPrintMatrix3(float *m);

void  zPrintMatrix4(float *m);

void  zDrawVec3(ZVec3 *v);

void  zDrawVec3f(float x, float y, float z);

#endif
