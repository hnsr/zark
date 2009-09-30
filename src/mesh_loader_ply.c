#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"

/* Parser for Stanford PLY models. 
 *
 * More info: http://local.wasp.uwa.edu.au/~pbourke/dataformats/ply/
 *
 * The header specifies what kind of elements are listed (vertices, faces), the order in which they
 * are listed and how many elements there are in the list. Following the specification of an element
 * listing, it gives a number of properties that each element consists of, the property specifics a
 * storage type (float, list) and it's purpose (x,y, z for vertices, vertex_indices for faces would
 * be common examples).
 *
 * The order in which the 'element' and 'property' keywords are listed specifies the order in which
 * the data is organized.
 *
 * Although I may not recognize the kind of element being listed or the prupose of some properties
 * (since these can be user-defined), as long as I know the storage type I can still process the
 * file since I will know how much data to read (and ignore).
 */

#define PLY_LINEBUF_SIZE  512
#define PLY_TOKEN_SIZE    512

// Some (hopefully) reasonably limits on the number of element lists and properties. Parsing will
// fail if these are exceeded so they are liberal. Mainly materials could have lots of properties (4
// for each color for example).
#define PLY_MAX_PROPS     40
#define PLY_MAX_ELEMLISTS 10

#define PLY_FORMAT_ASCII 1
#define PLY_FORMAT_BINLE 2
#define PLY_FORMAT_BINBE 3

// The element list types that I know how to parse.
// TODO: Add normals, texcoords?
#define PLY_ELEMTYPE_UNRECOGNIZED   0
#define PLY_ELEMTYPE_VERTEX         1
#define PLY_ELEMTYPE_FACE           2


// Purpose (meaning) of the property that I know what to do with.
#define PLY_PURPOSE_UNRECOGNIZED 0
#define PLY_PURPOSE_XCOORD       1
#define PLY_PURPOSE_YCOORD       2
#define PLY_PURPOSE_ZCOORD       3
#define PLY_PURPOSE_VINDICES     4


// Storage type of the property.
enum ply_proptype {
    PLY_TYPE_INVALID = 0,
    PLY_TYPE_CHAR,
    PLY_TYPE_UCHAR,
    PLY_TYPE_SHORT,
    PLY_TYPE_USHORT,
    PLY_TYPE_INT,
    PLY_TYPE_UINT,
    PLY_TYPE_FLOAT,
    PLY_TYPE_DOUBLE,
    PLY_TYPE_LIST,
    PLY_TYPE_NUM
};

static const size_t type_size[PLY_TYPE_NUM] = { 0, 1, 1, 2, 2, 4, 4, 4, 8, 0 };


// For diagnostic messages..
static const char *filename;
static unsigned int line_count;


// Temp buffers to store lines being read, and to read tokens from/into.
static char line[PLY_LINEBUF_SIZE];
static char *line_pos;
static char token[PLY_TOKEN_SIZE];


// Some macros to prevent RSI :p
#define PLY_CURELEM                (ply_header.elemlists[ply_header.num_elemlists-1])
#define PLY_CURPROP                (PLY_CURELEM.props[PLY_CURELEM.num_props-1])
#define PLY_ELEM(elemnum)          (ply_header.elemlists[(elemnum)])
#define PLY_PROP(elemnum, propnum) (ply_header.elemlists[(elemnum)].props[(propnum)])


typedef struct ply_property
{
    int type;    // Storage type of the property (float, int, list)
    int purpose; // Purpose of the propery (x/y/z coordinate, indices)

    // Only used for PLY_TYPE_LIST:
    int list_length_type;
    int list_member_type;

} ply_property;


typedef struct ply_elemlist
{
    int type; // Type of element listed (verts, faces, texcoords..)
    unsigned int count; // Number of elements in the list.

    // Properties to be parsed for each element in this list.
    unsigned int num_props;
    ply_property props[PLY_MAX_PROPS];

} ply_elemlist;


static struct
{
    int format;
    int version_major;
    int version_minor;

    // Element lists that need to be parsed.
    unsigned int num_elemlists;
    ply_elemlist elemlists[PLY_MAX_ELEMLISTS];

    unsigned int size; // Size of the header in bytes.

} ply_header;



// Read a token from line into token. Returns pointer to token on success or NULL on failure. After
// parse_token() returns, token is guaranteed to either be the token read, or an empty string. If
// the token being read from line was too large, the entire token in line will be skipped, and token
// will be set to an empty string.
static char *parse_token(void)
{
    char *p = line_pos, *o = token;
    int count = 0, skipped = 0;

    // Skip whitespace.
    while (*p == ' ' || *p == '\t') p++;

    // Read chars to token and stop if we hit whitespace, newline, \0, or if we don't have enough
    // room in token to fit a char + \0.
    while ( *p != ' '  && *p != '\t' && *p != '\0' && *p != '\n' && *p != '\r') {

        // Only copy character if there's enough room left.
        //if ( (PLY_TOKEN_SIZE-count) > 1 ) {
        if ( o < (token+PLY_TOKEN_SIZE) ) {
            *o++ = *p++;
            count++;
        } else {
            skipped++;
        }
    }

    // Always update line position. This way even if a token was too long and ignored, we still move
    // ahead correctly so the next token on the line can be parsed..
    line_pos = p;

    // If token was too long and got truncated, make token an empty string and return 0.
    if (skipped) {
        zWarning("Unable to read entire token on line %u while parsing \"%s\", token buffer too"
            " small.", line_count, filename);
        token[0] = '\0';
        return NULL;
    }

    *o = '\0';
    return token;
}



// Parse format from header.
static void parse_format(void) 
{
    unsigned int version_major, version_minor;

    if (!parse_token())
        return;

    if (strcmp("binary_big_endian", token) == 0)
        ply_header.format = PLY_FORMAT_BINBE;
    else if (strcmp("binary_little_endian", token) == 0)
        ply_header.format = PLY_FORMAT_BINLE;
    else if (strcmp("ascii", token) == 0)
        ply_header.format = PLY_FORMAT_ASCII;

    if (!parse_token())
        return;

    if (sscanf(token, "%u.%u", &version_major, &version_minor) != 2)
        return;

    ply_header.version_major = version_major;
    ply_header.version_minor = version_minor;
}



// Translate string to type.
static int get_type(const char *name)
{
    if      (strcmp("char",   name) == 0) return PLY_TYPE_CHAR;
    else if (strcmp("uchar",  name) == 0) return PLY_TYPE_UCHAR;
    else if (strcmp("short",  name) == 0) return PLY_TYPE_SHORT;
    else if (strcmp("ushort", name) == 0) return PLY_TYPE_USHORT;
    else if (strcmp("int",    name) == 0) return PLY_TYPE_INT;
    else if (strcmp("uint",   name) == 0) return PLY_TYPE_UINT;
    else if (strcmp("float",  name) == 0) return PLY_TYPE_FLOAT;
    else if (strcmp("double", name) == 0) return PLY_TYPE_DOUBLE;
    else if (strcmp("list",   name) == 0) return PLY_TYPE_LIST;
    else return 0;
}



// Translate string to purpose.
static int get_purpose(const char *name)
{
    if      (strcmp("x",              name) == 0) return PLY_PURPOSE_XCOORD;
    else if (strcmp("y",              name) == 0) return PLY_PURPOSE_YCOORD;
    else if (strcmp("z",              name) == 0) return PLY_PURPOSE_ZCOORD;
    else if (strcmp("vertex_indices", name) == 0) return PLY_PURPOSE_VINDICES;
    else return 0;
}



// Read type/purpose from line into ply_header. Returns 0 on any error or 1 otherwise.
static int parse_property(void)
{
    // Make sure I don't exceed limits.
    if ( !ply_header.num_elemlists ) {
        zWarning("Malformed header - property without element on line %u while parsing \"%s\".",
            line_count, filename);
        return 0;
    }

    if ( PLY_CURELEM.num_props >= PLY_MAX_PROPS ) {
        zWarning("Exceeded maximum number of properties support on line %u while parsing \"%s\".",
            line_count, filename);
        return 0;
    }


    // Add new property to current elemlist and get a convenient pointer to it..
    PLY_CURELEM.num_props++;

    // Parse primary type.
    parse_token();
    if ( !(PLY_CURPROP.type = get_type(token)) )
        return 0;

    // If this was a list I need to parse two more types (for list-length and list-element).
    if (PLY_CURPROP.type == PLY_TYPE_LIST) {
        parse_token();
        PLY_CURPROP.list_length_type = get_type(token);
        parse_token();
        PLY_CURPROP.list_member_type = get_type(token);

        if ( !PLY_CURPROP.list_length_type ||
             !PLY_CURPROP.list_member_type)
            return 0;
    }

    // And finally the purpose
    parse_token();
    if ( !(PLY_CURPROP.purpose = get_purpose(token)) )
        return 0;

    return 1;
}



// Translate string to element type.
static int get_elemtype(const char *name)
{
    if (strcmp("vertex", token) == 0)
        return PLY_ELEMTYPE_VERTEX;
    else if (strcmp("face", token) == 0)
        return PLY_ELEMTYPE_FACE;
    else
        return PLY_ELEMTYPE_UNRECOGNIZED;
}



// Parse PLY header. Returns 0 on error, 1 on success. If this function fails, ply_header is
// may be useless and parsing should be aborted.
static int parse_header(FILE* fd)
{
    // Parse header.
    while (fgets(line, PLY_LINEBUF_SIZE, fd)) {

        line_count++;
        line_pos = line;

        if (parse_token()) {

            if (strcmp("format", token) == 0) {
                parse_format();

            } else if (strcmp("element", token) == 0) {

                if (ply_header.num_elemlists >= PLY_MAX_ELEMLISTS) {
                    zWarning("Exceeded maximum number of element lists supported on line %u while"
                        " parsing \"%s\".", line_count, filename);
                    return 0;
                }

                ply_header.num_elemlists++;
                parse_token();
                PLY_CURELEM.type = get_elemtype(token);
                parse_token();
                PLY_CURELEM.count = (unsigned int) atoi(token);

            } else if (strcmp("property", token) == 0) {

                if (!parse_property()) {
                    zWarning("Failed to parse property on line %u while parsing \"%s\".",
                        line_count, filename);
                    return 0;
                }

            } else if (strcmp("end_header", token) == 0) {
                break;

            } else if (strcmp("comment", token) == 0) { // Silently ignore comments.
            } else {
                // I warn and return on this to prevent attempting to parse the entire file as a
                // header if it is malformed in some way..
                zWarning("Header contains unrecognized keyword \"%s\" on line %u while parsing"
                    " \"%s\".", token, line_count, filename);
                return 0;
            }
        }
    }

    ply_header.size = (unsigned int) ftell(fd);
    return 1;
}



static void dump_header_info()
{
    unsigned int i, j;

    zDebug("PLY header:");
    zDebug("  format = %d, version = %d.%d", ply_header.format, ply_header.version_major,
        ply_header.version_minor);
    zDebug("  num_elemlists = %d", ply_header.num_elemlists);
    for (i = 0; i < ply_header.num_elemlists; i++) {
        zDebug("    element type = %d, count = %u, num_props = %u", ply_header.elemlists[i].type,
            ply_header.elemlists[i].count, ply_header.elemlists[i].num_props);
        for (j = 0; j < ply_header.elemlists[i].num_props; j++) {
            zDebug("      prop type = %d, purpose = %d", ply_header.elemlists[i].props[j].type,
                ply_header.elemlists[i].props[j].purpose);
            if (ply_header.elemlists[i].props[j].type == PLY_TYPE_LIST) {
                zDebug("        list types: length = %d, member = %d",
                        ply_header.elemlists[i].props[j].list_length_type,
                        ply_header.elemlists[i].props[j].list_member_type);
            }
        }
    }
}



ZMesh *zLoadMeshPly(const char *file, unsigned int load_flags)
{
    FILE *fd;
    filename = file;
    line[0] = '\0';
    memset(&ply_header, 0, sizeof ply_header);

    // Open file, check magic bytes, parse header.
    if ( (fd = fopen(file, "rb")) == NULL ) {
        zError("Failed to open PLY mesh file \"%s\".", file);
        return NULL;
    }

    if ( !fgets(line, PLY_LINEBUF_SIZE, fd) || strcmp(line, "ply\n") != 0 ) {
        zError("Failed to load \"%s\", this does not seem to be a PLY mesh.", file);
        goto load_mesh_error_0;
    }
    line_count++;


    if (!parse_header(fd)) {
        zError("Failed to parse header for \"%s\".", filename);
        goto load_mesh_error_0;
    }

    dump_header_info(0);

    // Make sure we support the version. For now support everything, just emit a warning if it's not
    // 1.0, so that I can still read future backward-compatible versions..
    if (ply_header.version_major != 1 || ply_header.version_minor != 0)
        zWarning("Unsupported version (%u.%u) for PLY file format while parsing \"%s\", trying"
            " anyway.", ply_header.version_major, ply_header.version_minor, filename);


load_mesh_error_0:
    fclose(fd);
    return NULL;
}

