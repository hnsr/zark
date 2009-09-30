#include <string.h>
#ifndef WIN32
#include <strings.h>
#endif
#include <assert.h>

#include "common.h"

// Generate definitions.
#define MAKE_DEFINITIONS
#include "variables.def"
#undef MAKE_DEFINITIONS


ZVariable variables[] = {

    // Generate array initializers.
    #define MAKE_ARRAYINITS
    #include "variables.def"
    #undef MAKE_ARRAYINITS

    // Terminating entry.
    { 0, NULL, NULL, NULL, NULL, 0, 0.0 }
};



// Return (statically allocated) string containing name of given type.
const char *zVariableType(unsigned int type)
{
    switch (type)
    {
        case Z_VAR_TYPE_INT:    return "integer";
        case Z_VAR_TYPE_FLOAT:  return "float";
        case Z_VAR_TYPE_STRING: return "string";
    }
    assert( NULL && "Invalid variable type given");

    return "invalid";
}



// Return pointer to variable with name given, or NULL if none found.
ZVariable *zLookupVariable(const char *name)
{
    // Go through list of variables return pointer if matching variable is found, NULL otherwise.
    int i;

    // Loop through every command and compare strings until there's a match.
    for (i = 0; variables[i].varptr != NULL; i++) {

        if (strcasecmp(variables[i].name, name) == 0) {
            return variables+i;
        }
    }

    return NULL;
}

