#ifndef __VARIABLES_H__
#define __VARIABLES_H__


// String variables are statically allocated, saves me some headache with memory management. I can
// probably change this to use dynamic memory but I won't bother until I need to
#define Z_VAR_STRING_SIZE 256


// Allows me to index variables[] with Z_VAR_varname.
typedef enum ZVariableName
{
    #define MAKE_ENUM
    #include "variables.def"
    #undef MAKE_ENUM
} ZVariableName;


typedef enum ZVariableType
{
    Z_VAR_TYPE_INVALID,
    Z_VAR_TYPE_INT,
    Z_VAR_TYPE_FLOAT,
    Z_VAR_TYPE_FLOAT3,
    Z_VAR_TYPE_FLOAT4,
    Z_VAR_TYPE_STRING

} ZVariableType;


// ZVariable - defines some properties (type, defaults, limits) for an internal variable exposed to
// the user.
typedef struct ZVariable
{
    ZVariableType type;
    void *varptr; // Pointer to actual variable storage
    const char *name;
    const char *description;

    int changed; // Number of times this variable was changed, makes it cheap and easy to check if
                 // a variable was changed without having to compare the value itself (mostly useful
                 // for strings and tuples).

    // The default values and limits for this variable. I would use unions here, but it seems I can
    // only initialize unions at run-time (C99 allows union initializers, but MSVC won't), which I
    // prefer not to do for now..
    const int      int_default;
    const float  float_default;
    const float float3_default[3];
    const float float4_default[4];
    const char    *str_default;

    // When a scalar value is set, it is clamped to these min/max values first.
    const int     int_min;
    const int     int_max;
    const float float_min;
    const float float_max;

} ZVariable;


extern ZVariable variables[];


// Generate extern declarations for the variables.
#define MAKE_EXTERNDECLS
#include "variables.def"
#undef MAKE_EXTERNDECLS


const char *zVariableType(ZVariableType type);

int zVarIsDefault(ZVariable *var);

ZVariable *zLookupVariable(const char *name);

void zLoadConfig(void);

void zSaveConfig(void);

// Make accessing variables[] less verbose.
#define zVar(name) (variables[Z_VAR_##name])

// Set variable to val, clamped to variable's min/max.
#define zVarSetFloat(var,val) do {\
    float ff = (val); /* in case val has side-effects */\
     *((float *)(var)->varptr) = ( ff > (var)->float_max ? (var)->float_max :\
                                 ( ff < (var)->float_min ? (var)->float_min : ff ));\
} while (0)

#define zVarSetInt(var,val) do {\
    int ii = (val);\
     *((int *)(var)->varptr) = ( ii > (var)->int_max ? (var)->int_max :\
                               ( ii < (var)->int_min ? (var)->int_min : ii ));\
} while(0)

#endif

