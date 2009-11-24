#include <stdio.h>
#include <GL/glew.h>
#include <math.h>
#include <assert.h>

#include "common.h"


// Returns vector length.
float zLength3(ZVec3 *v)
{
    return sqrtf(v->x*v->x + v->y*v->y + v->z*v->z);
}



float zLength2(ZVec2 *v)
{
    return sqrtf(v->x*v->x + v->y*v->y);
}



// Normalize vector v. Can result in NaNs, so caller must be careful about the vector it passes, if
// it's too close to 0 things might blow up.
void zNormalize3(ZVec3 *v)
{
    float invlen = 1.0f/zLength3(v);

    v->x *= invlen;
    v->y *= invlen;
    v->z *= invlen;
}



// Returns dot-product of vectors a and b.
float zDot3(ZVec3 *a, ZVec3 *b)
{
    return a->x*b->x + a->y*b->y + a->z*b->z;
}



// Returns cross-product of vectors a and b.
ZVec3 zCross3(ZVec3 *a, ZVec3 *b)
{
    ZVec3 c;

    c.x = a->y*b->z - a->z*b->y;
    c.y = a->z*b->x - a->x*b->z;
    c.z = a->x*b->y - a->y*b->x;

    return c;
}



// Scale vector v by s.
void zScaleVec3(ZVec3 *v, float s)
{
    v->x *= s;
    v->y *= s;
    v->z *= s;
}



// Add vector b to a.
void zAddVec3(ZVec3 *a, ZVec3 *b)
{
    a->x += b->x;
    a->y += b->y;
    a->z += b->z;
}



// Subtract vector b from a.
void zSubtractVec3(ZVec3 *a, ZVec3 *b)
{
    a->x -= b->x;
    a->y -= b->y;
    a->z -= b->z;
}


void zSubtractVec2(ZVec2 *a, ZVec2 *b)
{
    a->x -= b->x;
    a->y -= b->y;
}


// Subtract vector b from a with result in r.
void zSubtractVec3r(ZVec3 *r, ZVec3 *a, ZVec3 *b)
{
    r->x = a->x - b->x;
    r->y = a->y - b->y;
    r->z = a->z - b->z;
}



// Transforms v by 3x3 matrix m.
void zTransform3Vec3(float *m, ZVec3 *v)
{
    ZVec3 tmp;

    tmp.x = m[0]*v->x + m[3]*v->y + m[6]*v->z;
    tmp.y = m[1]*v->x + m[4]*v->y + m[7]*v->z;
    tmp.z = m[2]*v->x + m[5]*v->y + m[8]*v->z;

    *v = tmp;
}



// Transforms vector v by affine 4x4 matrix m.
void zTransform4Vec3(float *m, ZVec3 *v)
{
    ZVec3 tmp;

    tmp.x = m[0]*v->x + m[4]*v->y +  m[8]*v->z;
    tmp.y = m[1]*v->x + m[5]*v->y +  m[9]*v->z;
    tmp.z = m[2]*v->x + m[6]*v->y + m[10]*v->z;

    *v = tmp;
}



// Transforms point p (given as Vec3) by affine 4x4 matrix m.
void zTransform4Point3(float *m, ZVec3 *p)
{
    ZVec3 tmp;

    tmp.x = m[0]*p->x + m[4]*p->y +  m[8]*p->z + m[12];
    tmp.y = m[1]*p->x + m[5]*p->y +  m[9]*p->z + m[13];
    tmp.z = m[2]*p->x + m[6]*p->y + m[10]*p->z + m[14];

    *p = tmp;
}



// Add 3x3 matrix b to a.
void zAddMatrix3(float *a, float *b)
{
    a[0]+=b[0]; a[3]+=b[3]; a[6]+=b[6];
    a[1]+=b[1]; a[4]+=b[4]; a[7]+=b[7];
    a[2]+=b[2]; a[5]+=b[5]; a[8]+=b[8];
}



// Add 4x4 matrix b to a.
void zAddMatrix4(float *a, float *b)
{
    a[0]+=b[0]; a[4]+=b[4]; a[8] +=b[8];  a[12]+=b[12];
    a[1]+=b[1]; a[5]+=b[5]; a[9] +=b[9];  a[13]+=b[13];
    a[2]+=b[2]; a[6]+=b[6]; a[10]+=b[10]; a[14]+=b[14];
    a[3]+=b[3]; a[7]+=b[7]; a[11]+=b[11]; a[15]+=b[15];
}



// Scale 3x3 matrix a by s.
void zScaleMatrix3(float *a, float s)
{
    a[0]*=s; a[3]*=s; a[6]*=s;
    a[1]*=s; a[4]*=s; a[7]*=s;
    a[2]*=s; a[5]*=s; a[8]*=s;
}



// Scale 3x3 matrix a by s.
void zScaleMatrix4(float *a, float s)
{
    a[0]*=s; a[4]*=s; a[8] *=s; a[12]*=s;
    a[1]*=s; a[5]*=s; a[9] *=s; a[13]*=s;
    a[2]*=s; a[6]*=s; a[10]*=s; a[14]*=s;
    a[3]*=s; a[7]*=s; a[11]*=s; a[15]*=s;
}



// Pre-multiply 3x3 matrix b by a, store result in r.
void zMultMatrix3(float *r, float *a, float *b)
{
    assert( r != a );
    assert( r != b );

    // Row 1
    r[0] = a[0]*b[0] + a[3]*b[1] + a[6]*b[2];
    r[3] = a[0]*b[3] + a[3]*b[4] + a[6]*b[5];
    r[6] = a[0]*b[6] + a[3]*b[7] + a[6]*b[8];

    // Row 2
    r[1] = a[1]*b[0] + a[4]*b[1] + a[7]*b[2];
    r[4] = a[1]*b[3] + a[4]*b[4] + a[7]*b[5];
    r[7] = a[1]*b[6] + a[4]*b[7] + a[7]*b[8];

    // Row 3
    r[2] = a[2]*b[0] + a[5]*b[1] + a[8]*b[2];
    r[5] = a[2]*b[3] + a[5]*b[4] + a[8]*b[5];
    r[8] = a[2]*b[6] + a[5]*b[7] + a[8]*b[8];
}



// Pre-multiply 4x4 matrix b by a, store the result in r.
void zMultMatrix4(float *r, float *a, float *b)
{
    assert( r != a );
    assert( r != b );

    // Row 1
    r[0]  = a[0]*b[0]  + a[4]*b[1]  + a[8]*b[2]  + a[12]*b[3];
    r[4]  = a[0]*b[4]  + a[4]*b[5]  + a[8]*b[6]  + a[12]*b[7];
    r[8]  = a[0]*b[8]  + a[4]*b[9]  + a[8]*b[10] + a[12]*b[11];
    r[12] = a[0]*b[12] + a[4]*b[13] + a[8]*b[14] + a[12]*b[15];

    // Row 2
    r[1]  = a[1]*b[0]  + a[5]*b[1]  + a[9]*b[2]  + a[13]*b[3];
    r[5]  = a[1]*b[4]  + a[5]*b[5]  + a[9]*b[6]  + a[13]*b[7];
    r[9]  = a[1]*b[8]  + a[5]*b[9]  + a[9]*b[10] + a[13]*b[11];
    r[13] = a[1]*b[12] + a[5]*b[13] + a[9]*b[14] + a[13]*b[15];

    // Row 3
    r[2]  = a[2]*b[0]  + a[6]*b[1]  + a[10]*b[2]  + a[14]*b[3];
    r[6]  = a[2]*b[4]  + a[6]*b[5]  + a[10]*b[6]  + a[14]*b[7];
    r[10] = a[2]*b[8]  + a[6]*b[9]  + a[10]*b[10] + a[14]*b[11];
    r[14] = a[2]*b[12] + a[6]*b[13] + a[10]*b[14] + a[14]*b[15];

    // Row 4
    r[3]  = a[3]*b[0]  + a[7]*b[1]  + a[11]*b[2]  + a[15]*b[3];
    r[7]  = a[3]*b[4]  + a[7]*b[5]  + a[11]*b[6]  + a[15]*b[7];
    r[11] = a[3]*b[8]  + a[7]*b[9]  + a[11]*b[10] + a[15]*b[11];
    r[15] = a[3]*b[12] + a[7]*b[13] + a[11]*b[14] + a[15]*b[15];
}



// m[0] m[3] m[6]
// m[1] m[4] m[7]
// m[2] m[5] m[8]

// Calculate determinant for a 3x3 matrix.
float zDetMatrix3(float *m)
{
    float det = ( m[0] * (m[4]*m[8] - m[7]*m[5]) )
              - ( m[3] * (m[1]*m[8] - m[7]*m[2]) )
              + ( m[6] * (m[1]*m[5] - m[4]*m[2]) ) ;

    return det;
}



// m[0]  m[4]  m[8]  m[12]
// m[1]  m[5]  m[9]  m[13]
// m[2]  m[6]  m[10] m[14]
// m[3]  m[7]  m[11] m[15]

// Calculate determinant for a 4x4 matrix.
float zDetMatrix4(float *m)
{
    float det = (  m[0]  *  (   ( m[5] * (m[10]*m[15] - m[14]*m[11]) )
                              - ( m[9] * ( m[6]*m[15] - m[14]*m[7] ) )
                              + ( m[13]* ( m[6]*m[11] - m[10]*m[7] ) ) ) )

              - (  m[4]  *  (   ( m[1] * ( m[10]*m[15] - m[14]*m[11]) )
                              - ( m[9] * ( m[2] *m[15] - m[14]*m[3] ) )
                              + ( m[13]* ( m[2] *m[11] - m[10]*m[3] ) ) ) )

              + (  m[8]  *  (   ( m[1] * ( m[6]*m[15] - m[14]*m[7] ) )
                              - ( m[5] * ( m[2]*m[15] - m[14]*m[3] ) )
                              + ( m[13]* ( m[2]*m[7]  - m[6] *m[3] ) ) ) )

              - (  m[12] *  (   ( m[1] * ( m[6]*m[11] - m[10]*m[7] ) )
                              - ( m[5] * ( m[2]*m[11] - m[10]*m[3] ) )
                              + ( m[9] * ( m[2]*m[7]  - m[6] *m[3] ) ) ) );

    return det;
}



// Calculate tangent and bitangent vectors for a triangle defined in object space and texcoord space
// by v1,v2,v3 and vt1,vt2,vt3 respectively. Resulting tangent and bitangent vectors are stored at
// T and B unless they are NULL.
void zCalcTriangleTB(ZVec3 *T, ZVec3 *B, ZVec3 *v0,  ZVec3 *v1,  ZVec3 *v2,
                                         ZVec2 *vt0, ZVec2 *vt1, ZVec2 *vt2)
{
    // Based on the derivation at http://www.terathon.com/code/tangent.html
    //
    // The known vectors v1-v0 and vt1-vt0 are the same vector but represented in different
    // coordinates spaces (object space and tangent space). I need to solve for the basis vectors
    // (T and B) which can be linearly combined with the components of vt1-vt0 and vt2-vt0, to yield
    // v1-v0 and v2-v0. So if I let
    //
    //      Q1 = v1-v0,
    //      Q2 = v2-v0,
    // (s1,t1) = vt1-vt0, and
    // (s2,t2) = vt2-vt0,
    //
    // then the above relationship can be written as:
    //
    // Q1 = s1*T + t1*B
    // Q2 = s2*T + t2*B
    //
    // and the system I need to solve is (in matrix notation):
    //
    // [ Q1x Q1y Q1z ] = [ s1 t1 ] [ Tx Ty Tz ]
    // [ Q2x Q2y Q2z ]   [ s2 t2 ] [ Bx By Bz ]
    //
    // which is done by pre-multiplying both sides with the inverse of [ s1 t1 ]
    //                                                                 [ s2 t2 ]:
    //
    // 1/(s1*t2-t1*s2) [ t2 -t1 ] [ Q1x Q1y Q1z ] = [ Tx Ty Tz ]
    //                 [-s2  s1 ] [ Q2x Q2y Q2z ]   [ Bx By Bz ]

    ZVec3 Q1, Q2;
    float s1, t1, s2, t2;
    float r;

    s1 = vt1->x - vt0->x;
    t1 = vt1->y - vt0->y;
    s2 = vt2->x - vt0->x;
    t2 = vt2->y - vt0->y;

    zSubtractVec3r(&Q1, v1, v0);
    zSubtractVec3r(&Q2, v2, v0);

    r = (s1*t2 - t1*s2);

    //assert(r > 0.0078125); // XXX: remove?

    r = 1.0f / r;

    if (T) {
        T->x = ( t2*Q1.x - t1*Q2.x) * r;
        T->y = ( t2*Q1.y - t1*Q2.y) * r;
        T->z = ( t2*Q1.z - t1*Q2.z) * r;
    }
    if (B) {
        B->x = (-s2*Q1.x + s1*Q2.x) * r;
        B->y = (-s2*Q1.y + s1*Q2.y) * r;
        B->z = (-s2*Q1.z + s1*Q2.z) * r;
    }
}



// Print vector to stdout.
void zPrintVec3(ZVec3 *v)
{
    zPrint("(%f, %f, %f)\n", v->x, v->y, v->z);
}



// Print 3x3 matrix m to stdout.
void zPrintMatrix3(float *m) {

    zPrint("%.2f %.2f %.2f\n",   m[0], m[3], m[6]);
    zPrint("%.2f %.2f %.2f\n",   m[1], m[4], m[7]);
    zPrint("%.2f %.2f %.2f\n\n", m[2], m[5], m[8]);
}



// Print 4x4 matrix m to stdout.
void zPrintMatrix4(float *m) {

    zPrint("%.2f %.2f %.2f %.2f\n",   m[0], m[4], m[8],  m[12]);
    zPrint("%.2f %.2f %.2f %.2f\n",   m[1], m[5], m[9],  m[13]);
    zPrint("%.2f %.2f %.2f %.2f\n",   m[2], m[6], m[10], m[14]);
    zPrint("%.2f %.2f %.2f %.2f\n\n", m[3], m[7], m[11], m[15]);
}



// Draw vector.
void zDrawVec3(ZVec3 *v)
{
    glPushAttrib(GL_LIGHTING_BIT | GL_LINE_BIT | GL_POINT_BIT);
    glDisable(GL_LIGHTING);
    glLineWidth(1.0f);
    glPointSize(4.0f);

    glBegin(GL_POINTS);
        glVertex3f(v->x, v->y, v->z);
    glEnd();

    glBegin(GL_LINES);
        glVertex3f(0.0f, 0.0f, 0.0f);
        glVertex3f(v->x, v->y, v->z);
    glEnd();

    glPopAttrib();
}



// Draw vector given by x,y,z.
void zDrawVec3f(float x, float y, float z)
{
    ZVec3 tmp;

    tmp.x = x;
    tmp.y = y;
    tmp.z = z;

    zDrawVec3(&tmp);
}

