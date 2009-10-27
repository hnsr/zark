#ifndef __MESH_H__
#define __MESH_H__

#include <GL/glew.h>

#include "zmath.h"

// Load flags (zLoadMesh)
#define Z_MESH_LOAD_NORMALIZE  1  // Normalize normal vectors
#define Z_MESH_LOAD_THOROUGH   2  // Load thoroughly. This may take much longer, but give better
                                  // results for things like vertex sharing. Meant to be used when
                                  // converting from one format to another.
#define Z_MESH_LOAD_NOINDEX    4  // A hint to the loader to not use indexed vertex arrays, may be
                                  // ignored. To be sure wether or not the loaded mesh uses an indexed
                                  // vertex array, check for Z_MESH_VA_INDEXED in mesh.flags.
#define Z_MESH_LOAD_TANGENTS   8  // Calculate tangent/bitangent vectors for mesh if it has normals
                                  // and didn't supply them itself.


// Data format flags - i.e. vertex array format. (ZMesh.flags)
#define Z_MESH_HAS_NORMALS     1
#define Z_MESH_HAS_TEXCOORDS   2
#define Z_MESH_VA_INDEXED      4
#define Z_MESH_HAS_TANGENTS    8 // For now tangents/bitangents are stored in a seperate (but inter-
#define Z_MESH_HAS_BITANGENTS 16 // leaved if mesh has both) array from the vertices/normals/
                                 // texcoords so that I can use a standard interleaved format for
                                 // those.


// Buffer grow flags (zGrowMeshBuffers)
#define Z_MESH_GROW_VERTICES 1
#define Z_MESH_GROW_INDICES  2


#define Z_MESH_MAXGROUPS 128 // Maximum number of groups a mesh can have, can probably be lowered.

#define Z_MESH_VERTICES_BUFINC 2000 // By how much buffers are resized during loading.
#define Z_MESH_INDICES_BUFINC  2000


// Some structs to make accessing vertex and tangent arrays less painful. These need to be tightly
// packed or else it won't match the OpenGL vertex array format and bad stuff will happen
#pragma pack(push,4)
typedef struct ZVertexV
{
    ZVec3 v;
} ZVertexV;

typedef struct ZVertexNV
{
    ZVec3 vn;
    ZVec3 v;
} ZVertexNV;

typedef struct ZVertexTV
{
    ZVec2 vt;
    ZVec3 v;
} ZVertexTV;

typedef struct ZVertexTNV
{
    ZVec2 vt;
    ZVec3 vn;
    ZVec3 v;
} ZVertexTNV;

typedef struct ZTangentT
{
    ZVec3 t;
} ZTangentT;

typedef struct ZTangentTB
{
    ZVec3 t;
    ZVec3 b;
} ZTangentTB;

#pragma pack(pop)

// ZMeshGroup - A grouping of vertices or indices (depending on wether Z_MESH_VA_INDEXED is set or
// not) associated with a material.
typedef struct ZMeshGroup
{
    unsigned int start; // First index/vertex.

    unsigned int count; // Number of indices/vertices to dereference/draw for this group.

    ZMaterial *material;

} ZMeshGroup;



typedef struct ZMesh
{
    char name[Z_RESOURCE_NAME_SIZE];

    int is_resident; // Wether or not the vertex data has been uploaded to OpenGL.

    unsigned int flags; // Data format flags.

    unsigned int num_groups; // Number of vertex groups.

    // Number of vertiex (and optionally tangents) and indices in buffer.
    unsigned int num_vertices;
    unsigned int num_indices;

    // Element size for vertex/index buffers.
    unsigned int vertices_size;
    unsigned int indices_size;

    unsigned int elem_size; // Size of each vertex in number of floats.

    // Names of OpenGL VBOs, 0 means they haven't been uploaded.
    GLuint vertex_vbo_name;
    GLuint tangent_vbo_name;
    GLuint index_vbo_name;

    // Vertex/tangent/index buffers
    float *vertices;
    float *tangents;
    unsigned int *indices;

    // Linked list of materials local to the mesh (i.e. those loaded from a .mtl library for an .obj
    // model). Groups may or may not refer to these. Should be freed when the mesh is deleted.
    ZMaterial *materials;

    ZMeshGroup groups[Z_MESH_MAXGROUPS];

    struct ZMesh *next;

} ZMesh;


void zMeshInit(void);

void zMeshDeinit(void);

void zBuildTangentArray(ZMesh *mesh, int bitangent);

ZMesh *zLookupMesh(const char *name);

void zIterMeshes(void (*iter)(ZMesh *, void *), void *data);

void zDrawMesh(ZMesh *mdl);

int zGrowMeshBuffers(ZMesh *mesh, int type);

void zMeshInfo(ZMesh *mesh);

void zMakeMeshNonResident(ZMesh *mesh, void *ignored);

void zDeleteMesh(ZMesh *mesh);

#endif
