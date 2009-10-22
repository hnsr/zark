#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
    #include <strings.h>
#endif
#include <assert.h>
#include <GL/glew.h>

#include "common.h"

#define Z_MESH_HASH_SIZE 512

static ZMesh *meshes[Z_MESH_HASH_SIZE];



void zMeshInit(void)
{
}



void zMeshDeinit(void)
{
    // Make meshes non-resident
    zIterMeshes(zMakeMeshNonResident, NULL);
}



// Add mesh to hash table.
static void zAddMeshToHashTable(ZMesh *mesh)
{
    unsigned int i;

    assert(mesh->next == NULL);

    i = zHashString(mesh->name, Z_MESH_HASH_SIZE);
    mesh->next = meshes[i];
    meshes[i] = mesh;
}



// Create and upload VBOs.
static void zMeshMakeResident(ZMesh *mesh)
{
    assert(mesh->vertices);

    // Setup VBOs and upload vertex data
    glGenBuffersARB(1, &(mesh->vertex_vbo_name));
    glBindBufferARB(GL_ARRAY_BUFFER, mesh->vertex_vbo_name);
    glBufferDataARB(GL_ARRAY_BUFFER, mesh->num_vertices * mesh->elem_size * sizeof(float),
        mesh->vertices, GL_STATIC_DRAW);
    glBindBufferARB(GL_ARRAY_BUFFER, 0);

    if (mesh->flags & Z_MESH_VA_INDEXED) {

        assert(mesh->indices);

        glGenBuffersARB(1, &(mesh->index_vbo_name));
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER, mesh->index_vbo_name);
        glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER, mesh->num_indices * sizeof(unsigned int),
            mesh->indices, GL_STATIC_DRAW);
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER, 0);
    }


    if (mesh->flags & (Z_MESH_HAS_TANGENTS|Z_MESH_HAS_BITANGENTS)) {

        assert(mesh->flags & Z_MESH_HAS_TANGENTS);
        assert(mesh->tangents);

        glGenBuffersARB(1, &(mesh->tangent_vbo_name));
        glBindBufferARB(GL_ARRAY_BUFFER, mesh->tangent_vbo_name);
        if (mesh->flags & Z_MESH_HAS_BITANGENTS) {
            glBufferDataARB(GL_ARRAY_BUFFER, mesh->num_vertices * sizeof(ZTangentTB),
                mesh->tangents, GL_STATIC_DRAW);
        } else {
            glBufferDataARB(GL_ARRAY_BUFFER, mesh->num_vertices * sizeof(ZTangentT),
                mesh->tangents, GL_STATIC_DRAW);
        }
        glBindBufferARB(GL_ARRAY_BUFFER, 0);
    }


    mesh->is_resident = 1;
}



// Build tangent array for mesh. If bitangent is 1, store both tangent and bitangent, otherwise only
// tangent is stored in the tangent array. Mesh must have both texcoords and normals.
void zBuildTangentArray(ZMesh *mesh, int bitangent)
{
    unsigned int i, count;
    ZVec3 t, b;
    ZTangentT  *tangents_t;
    ZTangentTB *tangents_tb;
    ZVertexTNV *vertices = (ZVertexTNV *) mesh->vertices;
    size_t tangent_size = bitangent ? sizeof(ZTangentTB) : sizeof(ZTangentT);

    // Assert of the mesh already has a tangent array.. that would probably not be right.
    assert(!(mesh->flags & (Z_MESH_HAS_TANGENTS|Z_MESH_HAS_BITANGENTS)));
    assert(!mesh->tangents);

    // We'll need texcoords to build the tangents, also require normals. Actually normals aren't
    // needed, maybe I can also make building normals an option?
    assert(mesh->flags & Z_MESH_HAS_NORMALS);
    assert(mesh->flags & Z_MESH_HAS_TEXCOORDS);

    // Allocate array for tangents, assign to mesh->tangents. Initialize to 0.
    mesh->tangents = malloc(tangent_size * mesh->num_vertices);
    if (!mesh->tangents) {
        zError("Failed to allocate memory while building tangent array for mesh \"%s\".",
            mesh->name);
        return;
    }
    memset(mesh->tangents, '\0', tangent_size * mesh->num_vertices);
    tangents_t  = (ZTangentT *) mesh->tangents;
    tangents_tb = (ZTangentTB *) mesh->tangents;


    // Figure out how many vertices to process, and ensure it is a multiple of 3 (since we're
    // processing whole triangles in one go).
    count = mesh->flags & Z_MESH_VA_INDEXED ? mesh->num_indices : mesh->num_vertices;
    assert(count % 3 == 0);


    // For every (possibly indexed) 3 vertices, get pointers to the vertices, calculate T/B vectors,
    // add them for each vertex in the tangent array.
    // XXX: This code makes me sad, how can I simplify this? :(
    if (mesh->flags & Z_MESH_VA_INDEXED) {

        for (i = 0; i < count; i += 3) {

            // Figure out where to read vertices from. Depends on wether or not I am using indexed
            // vertex arrays. I assume the vertex array contains both texcoords and normals (since
            // there's no point in calculating tangent vectors without those).
            zCalcTriangleTB(&t, &b,
                &(vertices[mesh->indices[i  ]].v),
                &(vertices[mesh->indices[i+1]].v),
                &(vertices[mesh->indices[i+2]].v),
                &(vertices[mesh->indices[i  ]].vt),
                &(vertices[mesh->indices[i+1]].vt),
                &(vertices[mesh->indices[i+2]].vt)
            );

            // Add up the calculated tangent(s) in the tangent array.
            if (bitangent) {
                zAddVec3(&(tangents_tb[mesh->indices[i  ]].t), &t);
                zAddVec3(&(tangents_tb[mesh->indices[i  ]].b), &b);
                zAddVec3(&(tangents_tb[mesh->indices[i+1]].t), &t);
                zAddVec3(&(tangents_tb[mesh->indices[i+1]].b), &b);
                zAddVec3(&(tangents_tb[mesh->indices[i+2]].t), &t);
                zAddVec3(&(tangents_tb[mesh->indices[i+2]].b), &b);
            } else {
                zAddVec3(&(tangents_t[mesh->indices[i  ]].t), &t);
                zAddVec3(&(tangents_t[mesh->indices[i+1]].t), &t);
                zAddVec3(&(tangents_t[mesh->indices[i+2]].t), &t);
            }
        }
    } else {
        // Same thing but for non-indexed arrays..
        for (i = 0; i < count; i += 3) {

            zCalcTriangleTB(&t, &b, &(vertices[i  ].v),  &(vertices[i+1].v),  &(vertices[i+2].v),
                                    &(vertices[i  ].vt), &(vertices[i+1].vt), &(vertices[i+2].vt));

            // XXX: Is there a point in adding them up with non-indexed arrays?
            if (bitangent) {
                zAddVec3(&(tangents_tb[i  ].t), &t);
                zAddVec3(&(tangents_tb[i  ].b), &b);
                zAddVec3(&(tangents_tb[i+1].t), &t);
                zAddVec3(&(tangents_tb[i+1].b), &b);
                zAddVec3(&(tangents_tb[i+2].t), &t);
                zAddVec3(&(tangents_tb[i+2].b), &b);
            } else {
                zAddVec3(&(tangents_t[i  ].t), &t);
                zAddVec3(&(tangents_t[i+1].t), &t);
                zAddVec3(&(tangents_t[i+2].t), &t);
            }
        }
    }

    // Normalize tangent vectors.
    if (bitangent) {
        for (i = 0; i < mesh->num_vertices; i++) {
            zNormalize3(&(tangents_tb[i].t));
            zNormalize3(&(tangents_tb[i].b));
        }
    } else {
        for (i = 0; i < mesh->num_vertices; i++) {
            zNormalize3(&(tangents_t[i].t));
        }
    }

    mesh->flags |= Z_MESH_HAS_TANGENTS;
    if (bitangent) mesh->flags |= Z_MESH_HAS_BITANGENTS;
}



ZMesh *zLoadMeshObj(const char *filename, unsigned int load_flags);
ZMesh *zLoadMeshPly(const char *filename, unsigned int load_flags);

// Load mesh.
static ZMesh *zLoadMesh(char *name, unsigned int load_flags)
{
    ZMesh *mesh;
    char *ext = zGetFileExtension(name);
    const char *realpath = zGetPath(name, NULL, Z_FILE_REWRITE_DIRSEP | Z_FILE_TRYUSER);

    if (printdiskload) zDebug("Loading mesh \"%s\" from disk.", name);

    if (strlen(name) >= Z_RESOURCE_NAME_SIZE) {
        zError("Mesh name \"%s\" exceeds RESOURCE_NAME_SIZE, not loading.", name);
        return NULL;
    }

    if (strcasecmp(ext, "obj") == 0) {
        mesh = zLoadMeshObj(realpath, load_flags);
    } else if (strcasecmp(ext, "ply") == 0) {
        mesh = zLoadMeshPly(realpath, load_flags);
    } else {
        zError("Unable to determine model format for \"%s\"", name);
        return NULL;
    }

    if (!mesh) return NULL;

    mesh->name[0] = '\0';
    strcat(mesh->name, name);

    mesh->next = NULL;
    zAddMeshToHashTable(mesh);

    // Calculate tangents if I need to, but check that they weren't already provided by the loader,
    if ( (load_flags & Z_MESH_LOAD_TANGENTS) ) {
        if ((mesh->flags & (Z_MESH_HAS_TANGENTS | Z_MESH_HAS_BITANGENTS)) != 0 ) {
            zDebug("Tangent generation requested but already provided by loader.");
        } else {
            // Make sure mesh has both normals and texcoords before trying to build tangent array.
            if (mesh->flags & Z_MESH_HAS_TEXCOORDS && mesh->flags & Z_MESH_HAS_NORMALS)
                zBuildTangentArray(mesh, 1);
            else
                zWarning("Not generating tangents for mesh \"%s\", mesh has no texcoords and/or"
                    " normals.", mesh->name);
        }
    }

    // From now on I keep the local copy, needed anyway for when it needs to be reuploaded after the
    // OpenGL context is recreated.
    // XXX: If I ever uncomment this, add tangent stuff.
#if 0
    // Get rid of the local copy, if I really need to access the data in the buffer objects I can
    // just map them with glMapBuffer.
    // FIXME: I can't really free the data since the buffer object can get corrupted on
    // glUnmapBuffer, so I need to keep a copy around so I can reinitialize the buffer.
    free(mesh->vertices);
    mesh->vertices = NULL;

    if (mesh->flags & Z_MESH_VA_INDEXED) {
        free(mesh->indices);
        mesh->indices = NULL;
    }
#endif
    return mesh;
}




// Lookup mesh with name in hash table.
ZMesh *zLookupMesh(char *name)
{
    unsigned int i;
    ZMesh *cur;

    assert(name);
    assert(strlen(name) > 0);

    // See if it is already loaded.
    i = zHashString(name, Z_MESH_HASH_SIZE);
    cur = meshes[i];

    while (cur != NULL) {

        if ( strcmp(cur->name, name) == 0)
            return cur;

        cur = cur->next;
    }

    // Not found, so load it.
    // FIXME: Make a toggle for loading with index/noindex?
    return zLoadMesh(name, Z_MESH_LOAD_TANGENTS);
}



// Iterate over all the meshes in hash table and call iter for each.
void zIterMeshes(void (*iter)(ZMesh *, void *), void *data)
{
    int i;
    ZMesh *cur;

    for (i = 0; i < Z_MESH_HASH_SIZE; i++) {

        cur = meshes[i];

        while (cur != NULL) {
            iter(cur, data);
            cur = cur->next;
        }
    }
}




// Returns appropriate OpengL interleaved vertex array format for mesh.
static GLenum zGetGLVAFormat(ZMesh *mesh)
{
    unsigned int flags = mesh->flags;

    if ( (flags & Z_MESH_HAS_NORMALS) && (flags & Z_MESH_HAS_TEXCOORDS) )
        return GL_T2F_N3F_V3F;

    else if (flags & Z_MESH_HAS_NORMALS)
        return GL_N3F_V3F;

    else if (flags & Z_MESH_HAS_TEXCOORDS)
        return GL_T2F_V3F;

    else
        return GL_V3F;
}



// Draw mesh.
void zDrawMesh(ZMesh *mesh)
{
    unsigned int i;

    assert(mesh);

    if (!mesh->is_resident) zMeshMakeResident(mesh);

    // Save initial state.
    glPushAttrib(GL_CURRENT_BIT | GL_ENABLE_BIT | GL_POLYGON_BIT);

    // Setup vertex arrays.
    glBindBufferARB(GL_ARRAY_BUFFER, mesh->vertex_vbo_name);

    // XXX: If I ever start using multiple sets of texture coordinates, I will need to ensure that
    // I set the texcoord array pointers correctly here..
    glInterleavedArrays(zGetGLVAFormat(mesh), 0, 0);

    if (mesh->flags & Z_MESH_VA_INDEXED)
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER, mesh->index_vbo_name);

    // Make sure meshes without texcoords don't get affected by left over state.
    if (!(mesh->flags & Z_MESH_HAS_TEXCOORDS))
        glTexCoord3f(0.0f, 0.0f, 0.0f);

    // Disable lighting if mesh has no normals.
    if (!(mesh->flags & Z_MESH_HAS_NORMALS))
        glDisable(GL_LIGHTING);

    // Draw normal filled triangles.
    if (!r_nofill) {
        for (i = 0; i < mesh->num_groups; i++) {
            ZMaterial *mat = mesh->groups[i].material;

            zMakeMaterialActive(mat);

            // If mesh has tangent/bitangent vectors, this group's material has a normalmap, and if
            // material's shader program has attrib locations for these, set attrib pointer here.
            if ( (mesh->flags & Z_MESH_HAS_TANGENTS) && mat->program ) {

                GLint tangent_loc   = mat->program->attributes[Z_ATTRIB_TANGENT];
                GLint bitangent_loc = mat->program->attributes[Z_ATTRIB_BITANGENT];

                assert(mesh->tangent_vbo_name);

                glBindBufferARB(GL_ARRAY_BUFFER, mesh->tangent_vbo_name);

                if (mesh->flags & Z_MESH_HAS_BITANGENTS && bitangent_loc >= 0) {

                    glEnableVertexAttribArray(bitangent_loc);
                    glVertexAttribPointer(bitangent_loc, 3, GL_FLOAT, GL_FALSE, sizeof(float)*6,
                        (void *) (sizeof(float)*3));
                }

                if (tangent_loc >= 0) {

                    glEnableVertexAttribArray(tangent_loc);
                    glVertexAttribPointer(tangent_loc, 3, GL_FLOAT, GL_FALSE,
                        sizeof(float) * ((mesh->flags & Z_MESH_HAS_BITANGENTS)?6:0), 0);

                }

            }

            // Finally, draw \o/
            if (mesh->flags & Z_MESH_VA_INDEXED) {
                glDrawElements(GL_TRIANGLES, mesh->groups[i].count, GL_UNSIGNED_INT,
                    (void *) (mesh->groups[i].start*sizeof(unsigned int)) );
            } else {
                glDrawArrays(GL_TRIANGLES, mesh->groups[i].start, mesh->groups[i].count);
            }
        }
    }


    // For wires/points/normals I don't want lighting, texturing, shading etc.
    if (r_drawwires | r_drawvertices | r_drawnormals | r_drawtangents) {
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        if (glUseProgram) glUseProgram(0);
    }


    // Draw points.
    if (r_drawvertices) {

        glColor3f(1.0f, 1.0f, 1.0f); // TODO: Turn this into a setting once setting tuples is
                                     // possible.

        // Pull points slightly to the front.
        glEnable(GL_POLYGON_OFFSET_POINT);
        glPolygonOffset(-0.5f, -1.0f);

        glPolygonMode(GL_FRONT, GL_POINT);

        if (mesh->flags & Z_MESH_VA_INDEXED)
            glDrawElements(GL_TRIANGLES, mesh->num_indices, GL_UNSIGNED_INT, 0);
        else
            glDrawArrays(GL_TRIANGLES, 0, mesh->num_vertices);
    }


    // Draw wireframe.
    if (r_drawwires) {

        glColor3f(1.0f, 1.0f, 1.0f); // TODO: Turn this into a setting once setting tuples is
                                     // possible.
        // Offset wires to the front.
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-0.5f, -1.0f);

        glPolygonMode(GL_FRONT, GL_LINE);

        if (mesh->flags & Z_MESH_VA_INDEXED)
            glDrawElements(GL_TRIANGLES, mesh->num_indices, GL_UNSIGNED_INT, 0);
        else
            glDrawArrays(GL_TRIANGLES, 0, mesh->num_vertices);
    }


    // Draw tangent vectors.
    if ( (r_drawtangents) && mesh->tangents ) {

        ZVec3 *v, *t, *b;

        // Draw lines between vertex and vertex+tangent.
        glBegin(GL_LINES);
        for (i = 0; i < mesh->num_vertices; i++) {

            // I probably won't need to check for all formats since a mesh with tangents likely has
            // texcoords and normals, but just in case..
            if ( (mesh->flags & Z_MESH_HAS_NORMALS) && (mesh->flags & Z_MESH_HAS_TEXCOORDS))
                v = &(((ZVertexTNV *)mesh->vertices)[i].v);
            else if (mesh->flags & Z_MESH_HAS_NORMALS)
                v = &(((ZVertexNV *)mesh->vertices)[i].v);
            else if (mesh->flags & Z_MESH_HAS_TEXCOORDS)
                v = &(((ZVertexTV *)mesh->vertices)[i].v);
            else
                v = &(((ZVertexV *)mesh->vertices)[i].v);

            // Since mesh->tangents exists, I'll assume mesh has tangents and I only check if maybe it
            // has bitangents as well.
            // XXX: Make it more obvious somehow that it's not possible to only have bitangets, it's
            // either only tangents, or both.
            if ( mesh->flags & Z_MESH_HAS_BITANGENTS ) {
                t = &(((ZTangentTB *)mesh->tangents)[i].t);
                b = &(((ZTangentTB *)mesh->tangents)[i].b);
            } else {
                t = &(((ZTangentT *)mesh->tangents)[i].t);
                b = NULL; // Silence compiler warning about uninitialized use.
            }

            glColor3f(1.0f, 0.0f, 0.0f);
            glVertex3fv((float *)v);
            glVertex3f( v->x + r_normalscale * t->x, v->y + r_normalscale * t->y,
                v->z + r_normalscale * t->z );

            if ( mesh->flags & Z_MESH_HAS_BITANGENTS) {
                glColor3f(0.0f, 1.0f, 0.0f);
                glVertex3fv((float *)v);
                glVertex3f(v->x + r_normalscale * b->x, v->y + r_normalscale * b->y,
                    v->z + r_normalscale * b->z );
            }
        }
        glEnd();
    }


    // Draw normals as line segments.
    // TODO: Draw from array in system memory instead.
    // TODO: Rewrite using typecasted structs for better readability
    if ( (r_drawnormals) && (mesh->flags & Z_MESH_HAS_NORMALS) ) {

        unsigned int n_offset, v_offset, stride, texcoordsize;
        float *vertices;

        glColor3f(0.0f, 0.0f, 1.0f);

        texcoordsize = (mesh->flags & Z_MESH_HAS_TEXCOORDS) ? 2 : 0;
        stride = 3 + 3 + texcoordsize;
        glBindBufferARB(GL_ARRAY_BUFFER, mesh->vertex_vbo_name);
        if ( !(vertices = glMapBufferARB(GL_ARRAY_BUFFER, GL_READ_ONLY)) ) {
            zWarning("Failed to map vertex buffer for drawing normals.");
            glPopAttrib();
            return;
        }

        glBegin(GL_LINES);

        for (i = 0; i < mesh->num_vertices; i++) {

            n_offset = (stride * i) + texcoordsize;
            v_offset = n_offset + 3;

            // Draw line from vertex to vertex + (r_normalscale * normal)
            glVertex3fv(vertices+v_offset);
            glVertex3f(
                vertices[v_offset]   + r_normalscale * vertices[n_offset],
                vertices[v_offset+1] + r_normalscale * vertices[n_offset+1],
                vertices[v_offset+2] + r_normalscale * vertices[n_offset+2]
            );
        }

        glEnd();

        if (!glUnmapBufferARB(GL_ARRAY_BUFFER)) {
            zWarning("Vertex buffer object became invalid, should re-upload it.");
        }
    }


    // Restore previous state
    glPopAttrib();
}



// Checks wether either vertex or index buffer needs to be resized. After successfully calling this
// function (i.e. it returns 1) it is guaranteed that one more vertex or index can be added to the
// buffer.
int zGrowMeshBuffers(ZMesh *mesh, int type)
{
    assert(mesh);
    assert(mesh->elem_size); // Can't resize vertex array if I don't know the element size.. this
                             // means I can't call zGrowMeshBuffers until the vertex format has been
                             // established!

    if (type == Z_MESH_GROW_VERTICES) {

        assert(mesh->num_vertices <= mesh->vertices_size);

        if (mesh->num_vertices == mesh->vertices_size) {

            float *tmp = (float *) realloc(mesh->vertices, (mesh->vertices_size +
                Z_MESH_VERTICES_BUFINC) * mesh->elem_size * sizeof(float) );

            if (!tmp) {
                zWarning("Failed to allocate memory for mesh vertex buffer.");
                return 0;
            }

            mesh->vertices = tmp;
            mesh->vertices_size += Z_MESH_VERTICES_BUFINC;
        }

    } else if (type == Z_MESH_GROW_INDICES) {

        assert(mesh->num_indices <= mesh->indices_size);

        if (mesh->num_indices == mesh->indices_size) {

            unsigned int *tmp = (unsigned int *) realloc(mesh->indices,
                    (mesh->indices_size + Z_MESH_INDICES_BUFINC) * sizeof(unsigned int) );

            if (!tmp) {
                zWarning("Failed to allocate memory for mesh index buffer.");
                return 0;
            }

            mesh->indices = tmp;
            mesh->indices_size += Z_MESH_INDICES_BUFINC;
        }

    } else {
        assert(0 && "Invalid buffer type given.");
    }

    return 1;
}



// Print some info on mesh.
void zMeshInfo(ZMesh *mesh)
{
    if (!mesh) return;

    zPrint("Dumping info on mesh \"%s\":\n", mesh->name);

    zPrint("  %u vertices (%u bytes)\n", mesh->num_vertices, mesh->num_vertices * mesh->elem_size *
         sizeof(float));

    zPrint("  %u indices (%u bytes)\n", mesh->num_indices,
        mesh->num_indices * sizeof(unsigned int));

    if (mesh->flags & Z_MESH_HAS_NORMALS)   zPrint("  mesh has normals\n");
    if (mesh->flags & Z_MESH_HAS_TEXCOORDS) zPrint("  mesh has texcoords\n");
    if (mesh->flags & Z_MESH_VA_INDEXED)    zPrint("  mesh uses indexed vertex array\n");

    if (mesh->groups) {
        unsigned int i;
        zPrint("  %u groups:\n", mesh->num_groups);
        for (i = 0; i < mesh->num_groups; i++) {
            ZMaterial *mat = mesh->groups[i].material;
            zPrint("    %u: material %s, start %u, count %u\n", i, mat->name,
                mesh->groups[i].start, mesh->groups[i].count);
        }
    } else {
        zPrint("  no groups\n");
    }
}



// Destroy VBO for mesh, and make materials local to mesh non-resident as well. Last parameter is
// ignored and makes it possible to pass this function directly to zIterMeshes.
void zMakeMeshNonResident(ZMesh *mesh, void *ignored)
{
    ZMaterial *curmat = mesh->materials;

    if (!mesh->is_resident) return;

    assert(mesh->vertex_vbo_name);
    glDeleteBuffersARB(1, &mesh->vertex_vbo_name);
    mesh->vertex_vbo_name = 0;

    // It's possible that I built a tangent array but for whatever reason never uploaded it to
    // OpenGL, so I don't check data format flags.
    if (mesh->tangent_vbo_name) {
        glDeleteBuffersARB(1, &mesh->tangent_vbo_name);
        mesh->tangent_vbo_name = 0;
    }

    if (mesh->flags & Z_MESH_VA_INDEXED) {
        assert(mesh->index_vbo_name);
        glDeleteBuffersARB(1, &mesh->index_vbo_name);
        mesh->index_vbo_name = 0;
    }


    while (curmat) {
        zMakeMaterialNonResident(curmat, NULL);
        curmat = curmat->next;
    }

    mesh->is_resident = 0;
}



// Free all resources associated with mesh.
void zDeleteMesh(ZMesh *mesh)
{
    ZMaterial *tmp, *cur;

    if (!mesh) return;

    if (mesh->index_vbo_name) {
        assert(glIsBufferARB(mesh->index_vbo_name));
        glDeleteBuffersARB(1, &(mesh->index_vbo_name));
    }

    if (mesh->vertex_vbo_name) {
        assert(glIsBufferARB(mesh->vertex_vbo_name));
        glDeleteBuffersARB(1, &(mesh->vertex_vbo_name));
    }

    if (mesh->tangent_vbo_name) {
        assert(glIsBufferARB(mesh->tangent_vbo_name));
        glDeleteBuffersARB(1, &(mesh->tangent_vbo_name));
    }

    // Free list of materials if there are any.
    cur = mesh->materials;

    while (cur) {
        tmp = cur->next;
        zDeleteMaterial(cur);
        cur = tmp;
    }

    free(mesh->vertices);
    free(mesh->indices);
    free(mesh->tangents);
    free(mesh);
}

