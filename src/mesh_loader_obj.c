#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <GL/glew.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

#include "common.h"


/* This code can read Wavefront .OBJ model files (and referenced material libraries) with a bunch of
 * limitations and modifications:
 *
 *  - Only supports the basic v/vn/vt/f geometry keywords.
 *
 *  - Only supports 2 dimensional texture coordinates, if given, the w component will be ignored.
 *
 *  - All face vertex index references in the entire file have to be consistent in wether or not
 *    they reference texcoord and/or normals. The specification doesn't make it clear wether this
 *    restriction only goes for the vertex index references within a single face, or for the entire
 *    file, this code assumes the latter.
 *
 *  - Material library support is limited to the Kd, Ka, Ks, Ns, d (without -halo), map_Kd, map_Ks,
 *    and bump (used as normal map) keywords. For the K* keywords only RGB parameters are supported,
 *    for the map_* and bump directives only the filename parameter is supported.
 *
 *  - Non-standard keywords supported are:
 *    
 *    - normalize: normalize normal vectors.
 *    - scale <scalar>: to scale vertex coordinates.
 *    
 *    For material libraries:
 *    - blend <type>: use fragment blending. Type should be one of: none, blend, add (more?).
 *    - tex_prefix <path>: prefix to for references to textues in material libraries.
 *    - tex_wrap <type>: set texture wrap mode. Type should be one of: clamp, clampedge, repeat.
 *    - vertex_shader <path>: path (relative to data dir) to vertex program.
 *    - fragment_shader <path>: path (relative to data dir) to fragment program.
 */


#define OBJ_LINE_BUFFER_SIZE 2000
#define OBJ_TOKEN_SIZE 100

// Increase filled-up buffers by these values * sizeof(vec3). On windows, anything above 1500
// doesn't give much improvement anymore. On Linux, any value works. Maybe realloc-ing isn't a good
// idea on Windows?
#define OBJ_VEC3_BUFFER_INC   5000

#define OBJ_VERTEX_BUFFER_INC  1000
#define OBJ_INDEX_BUFFER_INC   1000

// How far to search back when looking for shared vertices.
#define OBJ_VERTEX_SEARCH_WINDOW 512

#define OBJ_GROW_VERTICES 1
#define OBJ_GROW_INDICES  2


static ZMesh *mesh;

static const char *filename; // For printing diagnostic messages.

// Special processing options
static unsigned int load_flags; // Load flags
static float scale;

static char tex_prefix[OBJ_TOKEN_SIZE];

static int format_picked; // Wether or not a vertex format has been picked
static int warned_inconsistency; // Used to only warn about inconsistent vertex format once.
static unsigned int ignored_faces;

// Indicates the number of vertices each face is defined with. 0 means it hasn't been established
// yet how many vertices to process, 3 for triangles 4 for quads, anything else is invalid.
static unsigned int face_vertex_count;

// Pointers to memory where vertex data is temporarily stored as the file is parsed.
static ZVec3 *vertices;
static ZVec3 *normals;
static ZVec3 *texcoords;

// Using seperate groups for each material enountered. Once done parsing I transform them into a
// single vertex/index array and fill mesh->groups. This way I can reuse existing groups more easily
// when lots of groups using the same materials are listed in a random order in the model file.
static struct {
    float *vertices;
    unsigned int *indices;
    unsigned int num_vertices;
    unsigned int num_indices;
    size_t indices_size;
    size_t vertices_size;
    ZMaterial *material;
} groups[Z_MESH_MAXGROUPS];

static unsigned int num_groups;
static int cur_group;

// These hold the number of vec3's the currently allocated memory can hold.
static unsigned int vertices_size;
static unsigned int normals_size;
static unsigned int texcoords_size;

static unsigned int vertex_count;
static unsigned int normal_count;
static unsigned int texcoord_count;


// These keep track of where we are in the .obj file.
static unsigned int line_count;
static unsigned int triangle_count;


// Statically allocated buffers to hold lines/tokens being parsed.
static char line[OBJ_LINE_BUFFER_SIZE];
static char token[OBJ_TOKEN_SIZE];

// Current line position for token parsing.
static char *line_pos;



// Read a token from line into token. Returns number of chars read. Increases line_pos to point to
// the first character past the token.
static int parse_token(void)
{
    char *p = line_pos, *o = token;
    int count = 0;

    // Skip whitespace.
    while (*p == ' ' || *p == '\t') p++;

    // Read chars to token and stop if we hit whitespace, newline, \0, or if we don't have enough
    // room in token to fit a char + \0.
    while ( *p != ' '  && *p != '\t' && *p != '\0' && *p != '\n' && *p != '\r') {

        // See if we have room left in token.
        if ( (OBJ_TOKEN_SIZE-count) > 1 ) {
            *o++ = *p++;
            count++;
        } else {
            zWarning("Unable to read entire token on line %u while parsing \"%s\", token buffer"
                " too small.", line_count, filename);
        }
    }

    *o = '\0';
    line_pos = p; // Save line position.
    return count;
}



// Parses a vec3 from a line after token has been read (and line_pos set to point to whatever
// follows the token) and stores it in the appropriate buffer as indicated by type.
#define OBJ_DATATYPE_VERTEX 0
#define OBJ_DATATYPE_NORMAL 1
#define OBJ_DATATYPE_TEXCOORD 2
static int parse_vec3(int type)
{
    ZVec3 v = { 0.0f, 0.0f, 0.0f };
    ZVec3 **buffer;
    unsigned int *buffer_size;  // buffer_size*sizeof(vec3) = size in bytes.
    unsigned int *buffer_count; // Number of vec3s in the buffer.

    // Wire up the buffer to hold the vec3 we're parsing.
    if (type == OBJ_DATATYPE_VERTEX) {
        buffer = &vertices;
        buffer_size = &vertices_size;
        buffer_count = &vertex_count;
    } else if (type == OBJ_DATATYPE_NORMAL) {
        buffer = &normals;
        buffer_size = &normals_size;
        buffer_count = &normal_count;
    } else if (type == OBJ_DATATYPE_TEXCOORD) {
        buffer = &texcoords;
        buffer_size = &texcoords_size;
        buffer_count = &texcoord_count;
    } else {
        assert(0 && "No valid vec3 type was given");
    }

    // Increase size of buffer if neccesary.
    if (*buffer_count >= *buffer_size) {

        if ( ( *buffer = (ZVec3 *) 
                    realloc(*buffer, (*buffer_size+OBJ_VEC3_BUFFER_INC) * sizeof(ZVec3)) ) == NULL) {

            zFatal("%s: Failed to (re)allocate more memory for datatype data while parsing \"%s\".",
                __func__, filename);
            exit(EXIT_FAILURE);
        }

        *buffer_size += OBJ_VEC3_BUFFER_INC;
    }

    // Parse vertex coordinates, if one or more component is not parsed due to a malformed string,
    // they will be left at their initial 0.0 values and added to the buffer anyway. This way there
    // will still be enough vertices/normals/texcoords once the faces' indices are dereferenced.
    sscanf(line_pos, "%f %f %f", &(v.x), &(v.y), &(v.z));

    // Scale vertices if a scale factor was set,
    if (type == OBJ_DATATYPE_VERTEX && scale != 0.0f) {
        v.x *= scale;
        v.y *= scale;
        v.z *= scale;
    } else if (type == OBJ_DATATYPE_NORMAL && (load_flags & Z_MESH_LOAD_NORMALIZE)) {
        zNormalize3(&v);
    }

    (*buffer)[(*buffer_count)++] = v;

    return 1;
}



// Check if the given vertex format matches the aleady-established one. If it matches, returns 1,
// else 0. If no format has been established yet, it establishes the new format using the given
// values and returns 1.
//static int check_format(int has_t, int has_n) {
static inline int check_format(unsigned int flags) {

    if (!format_picked) {

        // Set new format.
        if ((flags & Z_MESH_HAS_NORMALS) && (flags & Z_MESH_HAS_TEXCOORDS))
            mesh->elem_size = 8;
        else if ( flags & Z_MESH_HAS_NORMALS )
            mesh->elem_size = 6;
        else if ( flags & Z_MESH_HAS_TEXCOORDS)
            mesh->elem_size = 5;
        else
            mesh->elem_size = 3;

        mesh->flags |= flags;
        format_picked = 1;
        return 1;

    } else {

        // Check of given format matches established one.
        if ( (mesh->flags & (Z_MESH_HAS_NORMALS | Z_MESH_HAS_TEXCOORDS )) == flags )
            return 1;
    }

    return 0;
}



// Dereference the indices to vertex, texcoord and normal data passed in by v, vt and vn and store
// the value in *v3v, *v3vt, and *v3vn respectively. v is required, vt and vn may be set to 0 so
// they will be ignored.
static inline int dereference_vertex(int v, int vt, int vn, ZVec3 *v3v, ZVec3 *v3vt, ZVec3 *v3vn)
{
    // Dereference vertex.
    if (v > 0 && v <= (int) vertex_count)
        *v3v = vertices[v-1]; // Positive index.
    else if (v < 0 && -v <= (int) vertex_count)
        *v3v = vertices[vertex_count+v]; // Negative index.
    else
        return 0; // 0 or out-of-range index.

    // Dereference texcoord but happily ignore if 0.
    if (vt) {

        if (vt > 0 && vt <= (int) texcoord_count)
            *v3vt = texcoords[vt-1];
        else if (vt < 0 && -vt <= (int) texcoord_count)
            *v3vt = texcoords[texcoord_count+vt];
        else
            return 0;
    }

    // Same for the normal.
    if (vn) {

        if (vn > 0 && vn <= (int) normal_count)
            *v3vn = normals[vn-1];
        else if (vn < 0 && -vn <= (int) normal_count)
            *v3vn = normals[normal_count+vn];
        else
            return 0;
    }

    return 1;
}



// Try to find a matching vertex so it can be reused. Returns 1 and writes index if match is found,
// or else 0 is returned. I'm searching backward as that should be more likely to find a match early
// on..
static inline int find_matching_vertex(float *new_vertex, unsigned int *index)
{
    unsigned int start = 0, end;
    float *curvert;
    size_t size = mesh->elem_size * sizeof(float);

    if (groups[cur_group].num_vertices == 0) return 0;

    if ( !(load_flags & Z_MESH_LOAD_THOROUGH) &&
        groups[cur_group].num_vertices > OBJ_VERTEX_SEARCH_WINDOW )

        start = groups[cur_group].num_vertices - OBJ_VERTEX_SEARCH_WINDOW;

    end = groups[cur_group].num_vertices-1;

    curvert = groups[cur_group].vertices+(mesh->elem_size*end);

    // Loop through some number of vertices and try to find a match
    do {
        if ( memcmp(curvert, new_vertex, size) == 0 ) {
            *index = end;
            return 1;
        }
        curvert -= mesh->elem_size;
    } while (end-- > start);

    return 0;
}



// Grow buffers if needed for current group.
static inline int grow_group_buffers(int type)
{
    assert(format_picked);

    if (type == OBJ_GROW_VERTICES) {

        assert(groups[cur_group].num_vertices <= groups[cur_group].vertices_size);

        if (groups[cur_group].num_vertices == groups[cur_group].vertices_size) {

            float *tmp = (float *) realloc(groups[cur_group].vertices,
                (groups[cur_group].vertices_size + 
                OBJ_VERTEX_BUFFER_INC) * mesh->elem_size * sizeof(float) );

            if (!tmp) {
                zWarning("Failed to allocate memory for mesh vertex buffer.");
                return 0;
            }

            groups[cur_group].vertices = tmp;
            groups[cur_group].vertices_size += OBJ_VERTEX_BUFFER_INC;
        }

    } else if (type == OBJ_GROW_INDICES) {

        assert(groups[cur_group].num_indices <= groups[cur_group].indices_size);

        if (groups[cur_group].num_indices == groups[cur_group].indices_size) {

            unsigned int *tmp = (unsigned int *) realloc(groups[cur_group].indices,
                (groups[cur_group].indices_size + OBJ_INDEX_BUFFER_INC) * sizeof(unsigned int) );

            if (!tmp) {
                zWarning("Failed to allocate memory for mesh index buffer.");
                return 0;
            }

            groups[cur_group].indices = tmp;
            groups[cur_group].indices_size += OBJ_INDEX_BUFFER_INC;
        }

    } else {
        assert(0 && "Invalid buffer type given.");
    }

    return 1;
}



// Add given vertex to mesh. Returns 1 if an error occured (makes it easier to check for errors from
// multiple calls in one go.
static inline int add_vertex_to_mesh(ZVec3 *v, ZVec3 *vt, ZVec3 *vn)
{
    float new_vertex[8];
    unsigned int match_index;

    if ( (mesh->flags & Z_MESH_HAS_NORMALS) && (mesh->flags & Z_MESH_HAS_TEXCOORDS) ) {
        new_vertex[0] = vt->x;  new_vertex[1] = vt->y;
        new_vertex[2] = vn->x;  new_vertex[3] = vn->y;  new_vertex[4] = vn->z;
        new_vertex[5] =  v->x;  new_vertex[6] =  v->y;  new_vertex[7] =  v->z;
    } else if ( mesh->flags & Z_MESH_HAS_NORMALS ) {
        new_vertex[0] = vn->x;  new_vertex[1] = vn->y;  new_vertex[2] = vn->z;
        new_vertex[3] =  v->x;  new_vertex[4] =  v->y;  new_vertex[5] =  v->z;
    } else if ( mesh->flags & Z_MESH_HAS_TEXCOORDS ) {
        new_vertex[0] = vt->x;  new_vertex[1] = vt->y;
        new_vertex[2] =  v->x;  new_vertex[3] =  v->y;  new_vertex[4] =  v->z;
    } else {
        new_vertex[0] =  v->x;  new_vertex[1] =  v->y;  new_vertex[2] =  v->z;
    }


    // Check for matching vertices (unless NOINDEX load flag is set), and if found, reuse index,
    // else add new.
    if ( !(load_flags & Z_MESH_LOAD_NOINDEX) && (find_matching_vertex(new_vertex, &match_index)) ) {

        // Add just the matched index.
        if ( !grow_group_buffers(OBJ_GROW_INDICES) ) {
            zDebug("%s: Failed to grow group index buffer.", __func__);
            return 1;
        }

        groups[cur_group].indices[groups[cur_group].num_indices] = match_index;
        groups[cur_group].num_indices++;

    } else {

        // Add new vertex / index, skip adding index if Z_MESH_LOAD_NOINDEX was set in load_flags.
        if ( !(load_flags & Z_MESH_LOAD_NOINDEX) ) {
            if ( !grow_group_buffers(OBJ_GROW_INDICES) ) {
                zDebug("%s: Failed to grow group index buffer.", __func__);
                return 1;
            }

            groups[cur_group].indices[groups[cur_group].num_indices] = groups[cur_group].num_vertices;
            groups[cur_group].num_indices++;
        }


        // Add vertex.
        if ( !grow_group_buffers(OBJ_GROW_VERTICES) ) {
            // TODO: Try to clean up added index.
            zDebug("%s: Failed to grow mesh vertex buffer.", __func__);
            return 1;
        }

        memcpy(groups[cur_group].vertices+(groups[cur_group].num_vertices*mesh->elem_size),
            new_vertex, mesh->elem_size*sizeof(float));
        groups[cur_group].num_vertices++;
    }

    return 0;
}



// Parses a face and dereferences the vertex, normal, and texcoord indices to store them in the
// mesh's vertex buffer.
static int parse_face(void)
{
    unsigned int face_vertex_count = 0, offset = 0, parsed_format = 0;
    int vertex_i = 0, texcoord_i = 0, normal_i = 0;

    // Temporary storage for up to 3 vertices. Once I have three vertices, I will be able to form a
    // tringle, and then another one for each additional vertex, by taking the first and then
    // previous vertex, in effect triangulating the polygon.
    ZVec3 face_vertices[3];
    ZVec3 face_texcoords[3];
    ZVec3 face_normals[3];

    // Parse an unlimited amount of vertices, but once we have more than two, start saving triangles
    // to the mesh.
    while ( parse_token() ) {

        face_vertex_count++;

        // If we've parsed 2 vertices, make sure we write into face_*[2].
        if (face_vertex_count > 2)
            offset = 2;
        else
            offset = face_vertex_count-1;

        // Try all the different formats (and also check if these are consistent with the already-
        // established format.
        if (sscanf(token, "%d/%d/%d", &vertex_i, &texcoord_i, &normal_i) == 3)
            parsed_format = Z_MESH_HAS_TEXCOORDS | Z_MESH_HAS_NORMALS;
        else if (sscanf(token, "%d//%d", &vertex_i, &normal_i) == 2)
            parsed_format = Z_MESH_HAS_NORMALS;
        else if (sscanf(token, "%d/%d", &vertex_i, &texcoord_i) == 2)
            parsed_format = Z_MESH_HAS_TEXCOORDS;
        else if (sscanf(token, "%d", &vertex_i) == 1)
            parsed_format = 0;
        else {
            zWarning("Invalid vertex format on line %d in \"%s\".", line_count, filename);
            return 0;
        }

        // Indices have been parsed, make sure format matches and dereference the indices.
        if (check_format(parsed_format)) {

            // Because the vertex format is always consistent, dereference_vertex can safely be used
            // like this because if normal_i/texcoord_i are left at 0 they won't be dereferenced.
            if ( !dereference_vertex(vertex_i, texcoord_i, normal_i,
                    face_vertices+offset, face_texcoords+offset, face_normals+offset ) ) {
                zWarning("Failed to dereference indices on line %u in \"%s\".", line_count, filename);
                return 0;
            }
        } else {
            if (!warned_inconsistency) {
                zWarning("Inconsistent vertex format on line %u in \"%s\" (this warning is only"
                    " printed once).", line_count, filename);
                warned_inconsistency = 1;
            }
            ignored_faces++;
            return 0;
        }

        // If I have now parsed more than 2 vertices, I can write a triangle to the mesh.
        if (face_vertex_count > 2) {

            int failed = 0;
            failed += add_vertex_to_mesh(&(face_vertices[0]), &(face_texcoords[0]), &(face_normals[0]));
            failed += add_vertex_to_mesh(&(face_vertices[1]), &(face_texcoords[1]), &(face_normals[1]));
            failed += add_vertex_to_mesh(&(face_vertices[2]), &(face_texcoords[2]), &(face_normals[2]));
            
            if (failed) {
                // Failed to add one or more vertices, just abort this face.
                // TODO: Should rollback the failed vertices..
                zWarning("Failed to add one more vertices while processing face on line %u in "
                    "\"%s\".", line_count, filename);
                return 0;
            }

            // Copy the last vertex to face_*[1] so that it will be used to form any additional
            // triangle if we are triangulating.
            face_texcoords[1] = face_texcoords[2];
              face_normals[1] =   face_normals[2];
             face_vertices[1] =  face_vertices[2];

            triangle_count++;
        }
    }

    // If, at this point, I haven't actually written any triangles, this face was malformed and we
    // should reset the format so I don't base the format on a malformed face statement.
    if (triangle_count == 0) {

        format_picked = 0;
        zWarning("Not enough vertices to form triangle on line %u in \"%s\".", line_count, filename);
        return 0;
    }

    return 1;
}



// Parses a (uniform) scale factor for the vertex coords.
static void parse_scale(void)
{
    float s;

    parse_token();

    if (sscanf(token, "%f", &s) == 1)
        scale = s;
    else
        zWarning("Unable to parse scale factor on line %d in \"%s\". Ignoring.", line_count,
        filename);
}



// Add copy of default material to local material list and rename it with given name. Returns a
// pointer to the added material or NULL on error.
static ZMaterial *add_new_material(const char *name)
{
    ZMaterial *mat;
    int namelen;

    assert(name);
    namelen = strlen(name);
    assert(namelen);

    if ( namelen > Z_RESOURCE_NAME_SIZE-1 ) {
        zWarning("Material name \"%s\" too long, ignoring.", name, filename);
        return NULL;
    }

    mat = zCopyMaterial(&default_material);

    if (mat) {

        // Make sure alpha values are sane, these can't be set in .OBJ models.
        mat->ambient_color[3]  = 1.0f;
        mat->specular_color[3] = 1.0f;
        mat->diffuse_color[3]  = 1.0f;
        mat->emission_color[3] = 1.0f;

        // Overwrite name.
        mat->name[0] = '\0';
        strncat(mat->name, name, Z_RESOURCE_NAME_SIZE-1);

        // Add material to this mesh's list.
        mat->next = mesh->materials;
        mesh->materials = mat;

        return mat;
    }

    return NULL;
}



// Parse a color ( 3 floats) from line_pos. If 3 floats were succesfully parsed, the RGB values are
// written to result[], else nothing is done. Result must be a pointer to an array of 3 floats.
static void parse_mtl_color(float *result)
{
    float color[3];
    int res;

    // If only one value is supplied I should set R, G and B to this value.
    res = sscanf(line_pos, "%f %f %f", color, color+1, color+2);

    if (res == 1) {
        result[0] = color[0];
        result[1] = color[0];
        result[2] = color[0];
    } else if (res == 3) {
        result[0] = color[0];
        result[1] = color[1];
        result[2] = color[2];
    } else {
        return;
    }
}



// Parse shininess value for material. If succesful, the parsed value is written to *result, else it
// remains untouched.
static void parse_mtl_shininess(float *result)
{
    float Ns;

    if ( !sscanf(line_pos, "%f", &Ns) )
        return;

    // TODO: Make sure this conversion is correct, since the MTL spec says values up to 1000 are
    // normal but OpenGL errors on >128.

    // For now I clamp to 0-128..
    if (Ns < 0.0f) Ns = 0.0f;
    if (Ns > 128.0f) Ns = 128.0f;

    *result = Ns;
}



// Write texname prefixed with tex_prefix to dest. dest must be a char array of size
// Z_RESOURCE_NAME_SIZE.
static void set_texname(char *dest, char *texname)
{
    int len;

    dest[0] = '\0';

    // Make sure the prefix and token lengths are < Z_RESOURCE_NAME_SIZE
    len = strlen(tex_prefix);
    len += strlen(texname);

    if (len >= Z_RESOURCE_NAME_SIZE) {
        zWarning("Texture name exceeded RESOURCE_NAME_SIZE, ignoring.");
        return;
    }

    strcat(dest, tex_prefix);
    strcat(dest, texname);
}



// Parse a material library.
static void parse_mtllib(void)
{
    const char *mtlpath;
    FILE *fd;
    unsigned int line_count = 0;
    ZMaterial *mat = NULL; // Pointer to most recently added material. Will be NULL initially or if
                           // there was an error parsing a newmtl directive.

    // Get filename.
    if ( !parse_token() ) {
        zWarning("Unable to parse material file name on line %d in \"%s\". Ignoring.", line_count,
            filename);
        return;
    }

    // Get full path for material lib, relative to the .obj file currently being parsed.
    if ( !(mtlpath = zGetSiblingPath(filename, token)) ) {
        zWarning("Unable to open material library \"%s\" while parsing \"%s\".", token,
            filename);
        return;
    }

    // Open file, start parsing lines.
    if ( (fd = fopen(mtlpath, "rb")) == NULL ) {
        zWarning("Failed to open material library \"%s\".", mtlpath);
        return;
    }

    // If this becomes false, I need to check size of token when handling *_shader tokens..
    assert(Z_RESOURCE_NAME_SIZE > OBJ_TOKEN_SIZE);

    while (fgets(line, OBJ_LINE_BUFFER_SIZE, fd)) {

        line_pos = line;
        line_count++;

        if (parse_token()) {

            if (strcmp("newmtl", token) == 0) {
                if (parse_token()) {
                    mat = add_new_material(token);
                } else {
                    zWarning("Failed to parse material name while parsing \"%s\" on line %u.",
                        mtlpath, line_count);
                    mat = NULL;
                }
            } else if (strcmp("tex_prefix", token) == 0) {
                // This is safe because sizeof(tex_prefix) == sizeof(token).
                if ( parse_token() ) {
                    tex_prefix[0] = '\0';
                    strcat(tex_prefix, token);
                }
            } else if (!mat)
                // No material is active at this point, so no point in parsing any material
                // attributes..
                continue;
            else if (strcmp("Ns", token) == 0) parse_mtl_shininess(&(mat->shininess));
            else if (strcmp("Ka", token) == 0) parse_mtl_color(mat->ambient_color);
            else if (strcmp("Kd", token) == 0) parse_mtl_color(mat->diffuse_color);
            else if (strcmp("Ks", token) == 0) parse_mtl_color(mat->specular_color);

            // Parsing the filename this way is not quite right since there may be options between
            // the token and filename.. Unfortunately I can't simply read the last token on the line
            // either as it might be part of a comment. For now I'm just going to leave it like this.
            else if (strcmp("map_Kd", token) == 0) {
                if ( parse_token() ) {
                    set_texname(mat->diffuse_map_name, token);
                }
            } else if (strcmp("map_Ks", token) == 0) {
                if ( parse_token() ) {
                    set_texname(mat->specular_map_name, token);
                }
            } else if (strcmp("bump", token) == 0) {
                if ( parse_token() ) {
                    set_texname(mat->normal_map_name, token);
                }
            } else if (strcmp("tex_wrap", token) == 0) {
                if ( parse_token() ) {
                    if (strcmp("clamp", token) == 0) {
                        mat->wrap_mode = Z_TEX_WRAP_CLAMP;
                    } else if (strcmp("clampedge", token) == 0) {
                        mat->wrap_mode = Z_TEX_WRAP_CLAMPEDGE;
                    } else if (strcmp("repeat", token) == 0) {
                        mat->wrap_mode = Z_TEX_WRAP_REPEAT;
                    }
                }
            } else if (strcmp("blend", token) == 0) {
                if ( parse_token() ) {
                    if (strcmp("alpha", token) == 0) {
                        mat->blend_type = Z_MTL_BLEND_ALPHA;
                    } else if (strcmp("add", token) == 0) {
                        mat->blend_type = Z_MTL_BLEND_ADD;
                    }
                }
            } else if (strcmp("vertex_shader", token) == 0) {
                if ( parse_token() ) {
                    mat->vertex_shader[0] = '\0';
                    strcat(mat->vertex_shader, token);
                }
            } else if (strcmp("fragment_shader", token) == 0) {
                if ( parse_token() ) {
                    mat->fragment_shader[0] = '\0';
                    strcat(mat->fragment_shader, token);
                }
            // Silently ignore a bunch of unsupported keywords:
            } else if (strcmp("#", token) == 0);
            else if (strcmp("illum", token) == 0);
            else if (strcmp("d", token) == 0);
            else if (strcmp("Ni", token) == 0);
            else {
                zWarning("Unknown keyword \"%s\" encountered on line %d in file \"%s\"."
                    " Ignoring.", token, line_count, mtlpath);
            }
        }
    }

    fclose(fd);

}



// Lookup material for given name. Try local material list first, if that fails use the global list
// (zLookupMaterial), if that fails too a pointer to the default material is returned.
static ZMaterial *lookup_material(const char *name)
{
    ZMaterial *cur = mesh->materials;

    // Lookup in local list.
    while (cur) {
        if ( strcmp(cur->name, name) == 0) {
            return cur;
        }
        cur = cur->next;
    }

    //zDebug("Failed to looking material \"%s\" for mesh %s in local list, trying global lookup.",
    //    name, filename);

    // FIXME: This look-up doesn't seem to work, maybe I am loaded meshes before loading materials?
    // If that fails use global list.
    if ( (cur = zLookupMaterial(name)) ) {
        return cur;
    }

    // If that fails too, use default material.
    zWarning("Failed to lookup material \"%s\" for mesh \"%s\", in either mesh-local or global"
        " list.. using default material.", name, filename);

    return &default_material;
}



// Parse a material name.
static void parse_usemtl(void)
{
    unsigned int i;
    ZMaterial *mat;

    // Lookup material.
    if ( !parse_token() ) {
        zWarning("Failed to parse material name while parsing \"%s\" on line %u.", filename,
            line_count);
        return;
    }

    mat = lookup_material(token);

    // Find if any existing group uses this material and switch to it if so,
    for (i = 0; i < num_groups; i++) {
        if (groups[i].material == mat) {
            cur_group = i;
            return;
        }
    }

    // Or if that fails, create a new group if the current one isn't still empty.
    if (groups[cur_group].num_vertices) {

        // Add group if there's room
        if (num_groups < Z_MESH_MAXGROUPS) {
            cur_group = num_groups++;
            groups[cur_group].material = mat;
        } else {
            zWarning("Unable to process material \"%s\" for \"%s\", reached MAXGROUPS.", token,
                filename);
        }
    } else {
        // Reuse current group since it's empty.
        groups[cur_group].material = mat;
    }
}



// Transform the vertex/index data in groups into a single array in mesh. Returns 1 on success or 0
// if an error occurs. After this function completes it is guaranteed that the group vertex/index
// buffers are freed.
static int transform_groups_to_mesh(void)
{
    unsigned int i, j;

    // Figure out how much memory needs to be allocated for the unified vertex/index buffers.
    for (i = 0; i < num_groups; i++) {
        mesh->vertices_size += groups[i].num_vertices;
        mesh->indices_size += groups[i].num_indices;
    }

    // Allocated the arrays.
    if (mesh->vertices_size) {
        mesh->vertices = malloc(mesh->vertices_size * mesh-> elem_size * sizeof(float));
        if (!mesh->vertices)
            goto error_cleanup;
    }

    if (mesh->flags & Z_MESH_VA_INDEXED && mesh->indices_size) {
        mesh->indices = malloc(mesh->indices_size * sizeof(unsigned int));
        if (!mesh->indices)
            goto error_cleanup;
    }

    // Iterate over all the groups, copy vertices / indices into arrays, update mesh group
    // start/counts.
    for (i = 0; i < num_groups; i++) {

        // Update mesh group.
        mesh->groups[i].material = groups[i].material;

        if (mesh->flags & Z_MESH_VA_INDEXED) {
            mesh->groups[i].start = mesh->num_indices;
            mesh->groups[i].count = groups[i].num_indices;
        } else {
            mesh->groups[i].start = mesh->num_vertices;
            mesh->groups[i].count = groups[i].num_vertices;
        }
        mesh->num_groups++;

        // Append vertices.
        memcpy(mesh->vertices+(mesh->num_vertices*mesh->elem_size), groups[i].vertices,
            groups[i].num_vertices*mesh->elem_size*sizeof(float));

        // Add indices, be sure to add group offset.
        if (mesh->flags & Z_MESH_VA_INDEXED) {
            for (j = 0; j < groups[i].num_indices; j++) {
                mesh->indices[mesh->num_indices++] = groups[i].indices[j] + mesh->num_vertices ;
                //mesh->indices[mesh->num_indices++] = groups[i].indices[j] + mesh->num_vertices;
            }
        }
        mesh->num_vertices += groups[i].num_vertices;

        // Free this group's buffers.
        free(groups[i].vertices);
        free(groups[i].indices);
    }
    return 1;

error_cleanup:

    zError("Failed to allocate mesh vertex or index buffer while parsing \"%s\".", filename);

    free(mesh->vertices);
    free(mesh->indices);
    mesh->indices = NULL;
    mesh->vertices = NULL;

    for (i = 0; i < num_groups; i++) {
        free(groups[i].vertices);
        free(groups[i].indices);
    }
    return 0;
}



// Load mesh from a Wavefront .obj file.
ZMesh *zLoadMeshObj(const char *file, unsigned int flags)
{
    FILE *fd;
    filename = file;
    vertices_size = normals_size = texcoords_size = OBJ_VEC3_BUFFER_INC;
    vertex_count = normal_count = texcoord_count = ignored_faces = 0;
    warned_inconsistency = triangle_count = line_count = format_picked = face_vertex_count = 0;
    scale = 0.0f;
    tex_prefix[0] = '\0';

    load_flags = flags;

    mesh      = malloc(sizeof(ZMesh));
    vertices  = malloc(OBJ_VEC3_BUFFER_INC * sizeof(ZVec3));
    normals   = malloc(OBJ_VEC3_BUFFER_INC * sizeof(ZVec3));
    texcoords = malloc(OBJ_VEC3_BUFFER_INC * sizeof(ZVec3));

    if ( (fd = fopen(file, "rb")) == NULL ) {
        zWarning("Failed to open OBJ mesh \"%s\".", file);
        return NULL;
    }


    if (!mesh || !vertices || !normals || !texcoords ) {

        zFatal("%s: Failed to allocate memory while parsing \"%s\". Aborting.", __func__, filename);
        free(mesh);
        free(vertices);
        free(normals);
        free(texcoords);
        return NULL;
    }

    memset(mesh, '\0', sizeof(ZMesh));

    // Initialize groups and setup initial group.
    memset(groups, '\0', sizeof(groups));
    num_groups = 1;
    cur_group = 0;
    groups[0].material = &default_material;

    // By default I use indexed vertex arrays, unless the NOINDEX load flags was given. If it later
    // turns out (after loading, see below) that no vertices were shared I remove the indices and
    // unset the VA_INDEXED bit again.
    if ( !(load_flags & Z_MESH_LOAD_NOINDEX) )
        mesh->flags |= Z_MESH_VA_INDEXED;

    // Process each line.
    while (fgets(line, OBJ_LINE_BUFFER_SIZE, fd)) {

        line_pos = line;
        line_count++;

        // Parse the data-type keyword (v, vn, vt, f, etc.).
        if ( parse_token() ) {

            if (strcmp("v", token) == 0)            parse_vec3(OBJ_DATATYPE_VERTEX);
            else if (strcmp("vn", token) == 0)      parse_vec3(OBJ_DATATYPE_NORMAL);
            else if (strcmp("vt", token) == 0)      parse_vec3(OBJ_DATATYPE_TEXCOORD);
            else if (strcmp("f", token) == 0)       parse_face();
            else if (strcmp("scale", token) == 0)   parse_scale();
            else if (strcmp("mtllib", token) == 0)  parse_mtllib();
            else if (strcmp("usemtl", token) == 0)  parse_usemtl();
            else if (strcmp("normalize", token) == 0) load_flags |= Z_MESH_LOAD_NORMALIZE;

            // These are silently ignored.
            else if (strcmp("s", token) == 0);
            else if (strcmp("o", token) == 0);
            else if (strcmp("g", token) == 0);
            else if (strcmp("#", token) == 0);
            else {
                zWarning("Unknown keyword encountered on line %d in file \"%s\". Ignoring.",
                    line_count, filename);
            }
        }
    }

    // We're now done with parsing and dereferencing v/vn/vt so I can close the file and get rid of
    // those buffers.
    free(vertices);
    free(normals);
    free(texcoords);
    fclose(fd);

    if (transform_groups_to_mesh()) {
        // Get rid of index array if no vertices were shared..
        // XXX: Is this actually safe? Maybe indices and vertices are equal but vertices might not
        // be ordered right?
        if ( (mesh->flags & Z_MESH_VA_INDEXED) && mesh->num_vertices == mesh->num_indices) {
            zDebug("While loading mesh \"%s\": mesh has no shared vertices, getting rid of index"
                " array.", file);
            mesh->flags &= ~Z_MESH_VA_INDEXED;
            free(mesh->indices);
            mesh->indices = NULL;
            mesh->num_indices = 0;
        }
    } else {
        mesh = NULL;
    }

    if (ignored_faces) {
        zWarning("%u faces were ignored due to parsing errors in \"%s\".", ignored_faces, filename);
    }

    return mesh;
}
