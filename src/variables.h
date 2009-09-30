#ifndef __VARIABLES_H__
#define __VARIABLES_H__



#define Z_VAR_TYPE_INT    1
#define Z_VAR_TYPE_FLOAT  2
#define Z_VAR_TYPE_STRING 3

// String variables are statically allocated, saves me some headache with memory managament, since
// if I use dynamic memory instead, the default value shouldn't be freed since it points to a string
// literal, while subsequently assigned strings *should* be. There may still be ways around this
// but I'll just go with a fixed size until I find a compelling reason not to.
#define Z_VAR_STRING_SIZE 200


typedef struct ZVariable
{
    unsigned int type;       // Type of this variable.
    void *varptr;            // Pointer to internal variable for storage.
    const char *name;        // Short name that identifies the variable.
    const char *description; // Description of the variable's purpose.

    // The default value for this variable, I keep track of these so the user can revert to them at
    // any time. Also this should ideally be a union, but it seems I can't initialize it at run-time
    // (C99 allows it but is not supported by MSVC), so I waste some space instead..
    const char   *str_default;
    const int     int_default;
    const float float_default;

    const int int_min;
    const int int_max;

    const float float_min;
    const float float_max;

} ZVariable;


extern ZVariable variables[];


// Generate extern declarations for the variables.
#define MAKE_EXTERNDECLS
#include "variables.def"
#undef MAKE_EXTERNDECLS


const char *zVariableType(unsigned int type);

ZVariable *zLookupVariable(const char *name);


#endif

