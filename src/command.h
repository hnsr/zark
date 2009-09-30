#ifndef __COMMAND_H__
#define __COMMAND_H__

#include "variables.h"


// Maximum number of arguments supported. I set this statically since making this dynamic is
// probably not worth the effort.
#define MAX_CMD_ARGS 4


// Recommended size for buffers that collect command strings, used for console, reading from
// autoexec.conf etc.
#define CMD_BUF_SIZE 1000



// ZParsedCommand - A parsed command string. Used with keybindings and maybe other things I
// haven't thought of yet (timers?) where having to reparse the command string might not be ideal
typedef struct ZParsedCommand
{
    const struct ZCommand *command; // Pointer to the command.
    char *argstr;                   // Pointer to unprocessed argument string for this command.
    unsigned int numargs;           // Number of arguments that were parsed.

    // The parsed command arguments. Intentionally not a union so that the different types are all
    // filled in and the command handler gets more freedom on handling the arguments.
    struct ZParsedCmdArg
    {
        int        int_arg;
        double   float_arg;
        char      *str_arg;
        ZVariable *var_arg;

    } args[MAX_CMD_ARGS];

} ZParsedCommand;



typedef int (*ZCommandFuncPtr)(const ZParsedCommand *pcmd);

// ZCommand - defines the properties of a function we want to expose as a command.
typedef struct ZCommand
{
    ZCommandFuncPtr cmdfunc;  // Pointer to the handler function.
    char *keyword;            // Keyword for the command, as used in command strings.
    unsigned int numargs;     // Number or arguments this command accepts.
    unsigned int numrequired; // Minimum number of args that must be given.
    char *description;

    // Descriptions for each of the arguments
    char *argdesc[MAX_CMD_ARGS];

} ZCommand;

extern ZCommand commands[];



int zLookupCmdArgSymbol(const char *symbolname);

void zLoadCommands(const char *filename);

// Actually defined in cmdstring_yacc.y.
int zParseCmdString(const char *cmdstring, ZParsedCommand **parsedcmds);

void zFreeParsedCmds(ZParsedCommand *parsedcmds, unsigned int numcmds);

int zExecParsedCmds(const ZParsedCommand *parsedcmds, unsigned int numcmds);

int zExecCmdString(const char *cmdstring);

const ZCommand *zLookupCommand(const char *cmdname);



#endif
