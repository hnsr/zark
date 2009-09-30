%{
#define _GNU_SOURCE // For strndup
#define _BSD_SOURCE // For strdup
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common.h"

// Silence some warnings in the generated code:
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#ifdef _MSC_VER
#pragma warning(disable:4018) // Signedness mismatch in comparison
#pragma warning(disable:4273) // Inconsistent DLL linkage
#endif


// As seen on http://www.gnu.org/software/hello/manual/automake/Yacc-and-Lex.html:
//
// Include this in the yacc source to prevent symbol conflicts when using multiple parsers in a
// single project.. Both flex and bison provide command line options for adding a prefix to the
// generated symbols, but that would probably make things less portable..
#define yymaxdepth  cmdstryy_maxdepth
#define yyparse     cmdstryy_parse
#define yylex       cmdstryy_lex
#define yyerror     cmdstryy_error
#define yylval      cmdstryy_lval
#define yychar      cmdstryy_char
#define yydebug     cmdstryy_debug
#define yypact      cmdstryy_pact
#define yyr1        cmdstryy_r1
#define yyr2        cmdstryy_r2
#define yydef       cmdstryy_def
#define yychk       cmdstryy_chk
#define yypgo       cmdstryy_pgo
#define yyact       cmdstryy_act
#define yyexca      cmdstryy_exca
#define yyerrflag   cmdstryy_errflag
#define yynerrs     cmdstryy_nerrs
#define yyps        cmdstryy_ps
#define yypv        cmdstryy_pv
#define yys         cmdstryy_s
#define yy_yys      cmdstryy_yys
#define yystate     cmdstryy_state
#define yytmp       cmdstryy_tmp
#define yyv         cmdstryy_v
#define yy_yyv      cmdstryy_yyv
#define yyval       cmdstryy_val
#define yylloc      cmdstryy_lloc
#define yyreds      cmdstryy_reds
#define yytoks      cmdstryy_toks
#define yylhs       cmdstryy_yylhs
#define yylen       cmdstryy_yylen
#define yydefred    cmdstryy_yydefred
#define yydgoto     cmdstryy_yydgoto
#define yysindex    cmdstryy_yysindex
#define yyrindex    cmdstryy_yyrindex
#define yygindex    cmdstryy_yygindex
#define yytable     cmdstryy_yytable
#define yycheck     cmdstryy_yycheck
#define yyname      cmdstryy_yyname
#define yyrule      cmdstryy_yyrule

// Some more symbols I had to add to the list myself. 
#define yyin                cmdstryy_in
#define yyout               cmdstryy_out
#define yywrap              cmdstryy_wrap
#define yylineno            cmdstryy_lineno
#define yy_flex_debug       cmdstryy_flex_debug
#define yy_create_buffer    cmdstryy_create_buffer
#define yy_flush_buffer     cmdstryy_flush_buffer
#define yy_switch_to_buffer cmdstryy_switch_to_buffer
#define yy_delete_buffer    cmdstryy_delete_buffer
#define yypush_buffer_state cmdstryy_push_buffer_state
#define yypop_buffer_state  cmdstryy_pop_buffer_state
#define yy_scan_buffer      cmdstryy_scan_buffer
#define yy_scan_string      cmdstryy_scan_string
#define yy_scan_bytes       cmdstryy_scan_bytes
#define yylex_destroy       cmdstryy_lex_destroy
#define yyget_lineno        cmdstryy_get_lineno
#define yyget_in            cmdstryy_get_in
#define yyget_out           cmdstryy_get_out
#define yyget_leng          cmdstryy_get_leng
#define yyget_text          cmdstryy_get_text
#define yyget_debug         cmdstryy_get_debug
#define yyset_lineno        cmdstryy_set_lineno
#define yyset_in            cmdstryy_set_in
#define yyset_out           cmdstryy_set_out
#define yyset_debug         cmdstryy_set_debug
#define yyrestart           cmdstryy_restart
#define yyalloc             cmdstryy_alloc
#define yyrealloc           cmdstryy_realloc
#define yyfree              cmdstryy_free

// Redefining this here because the default ECHO macro uses fwrite in a way that causes a compiler
// warning (warn_unused_result) and I don't need it anyway.
#define ECHO

// Prevent unistd.h from being included, this header doesn't exist on Windows.
#define YY_NO_UNISTD_H

// Keep track of the offset in the string being parsed.
#define YY_USER_ACTION buf_offset += yyleng;

#define UPDATE_ARG_OFFSETS if ( arg_start == 0 ) arg_start = buf_offset - yyleng; \
                           arg_end = buf_offset;


#define YY_USE_CONST

#define YYSTYPE char *

extern int yylineno;
extern FILE *yyin;
void yyrestart(FILE *fp);
int yylex(void);

// The lexer rules will record the start of the first argument, and the end of the last argument in
// these variables, so that I can make a copy of the entire argument string and tuck it onto
// ZParsedCommand. I'll also need the pointer to the cmdline itself to copy from.
static int arg_start;
static int arg_end;
static const char *cmdstring_ptr;
static int buf_offset; // Used by YY_USER_ACTION to keep track of where we are in cmdstring.

// Temp. vars used while parsing cmdline.
static char   *cmdname;
// .. and for every cmdarg in a cmdline
static char       *str_arg;
static int         int_arg;
static double    float_arg;
static ZVariable  *var_arg;

static unsigned int numcmds;
static ZParsedCommand *parsedcmds;
static ZParsedCommand *curpcmd;

// Called when EOF is reached. Could be used to start parsing next file for example. Not really
// needed in my case.
static int yywrap(void)
{
    // Return true, means no further scanning is done when EOF is reached.
    return 1;
}

static void yyerror(const char *str)
{
    //zDebug("Error while parsing cmdstring: %s.", str);
}

%}

%token CMDNAME STR_ARG QSTR_ARG INT_ARG FLOAT_ARG CMDEND


%%

cmdstring: | cmdstring cmdline ;

cmdline: CMDEND
       | CMDNAME 
{
    const ZCommand *cmd;

    // Reset arguments start/end offsets
    arg_start = arg_end = 0;

    // Lookup command, add it if it exists, cause parse error if it doesn't.
    cmd = zLookupCommand(cmdname);
    free(cmdname);
    cmdname = NULL;

    if ( cmd != NULL ) {

        ZParsedCommand *tmp;

        //zDebug("Found a match for \"%s\", adding to parsedcmds.", cmd->keyword);

        if ( (tmp = realloc(parsedcmds, sizeof(ZParsedCommand) * (numcmds+1))) ) {

            // Successfully allocated more memory, update parsedcmds pointer and numcmds count.
            parsedcmds = tmp;
            curpcmd = &(parsedcmds[numcmds++]);

            curpcmd->command = cmd;
            curpcmd->argstr = NULL;
            curpcmd->numargs = 0;

        } else {

            // Failed to allocate more memory to store new command, so error out.
            yyerror("failed to allocate memory for command");
            YYERROR;
        }

    } else {
        //zDebug("Unknown command \"%s\"", cmdname);
        yyerror("unrecognized command");
        YYERROR;
    }
}
         cmdargs
         CMDEND
{
    // Now that the command has been processed, I need to make a copy of the argument part for
    // command handler functions that want to bypass the parsed arguments. This is slightly tricky,
    // the lexer will record the position of the first argument, and the end of the last argument
    // seperately, and I need to copy the stuff in between...
    int arglen = arg_end-arg_start;

    if (arglen > 0 && (curpcmd->argstr = malloc(arglen+1)) ) {
        curpcmd->argstr[0] = '\0';
        strncat(curpcmd->argstr, cmdstring_ptr+arg_start, arglen);
        //zDebug("Copied argstr from offset %d, %d chars long (\"%s\").", arg_start,
        //    arglen, curpcmd->argstr);
    }

    // Make sure we have at least curpcmd->command->numrequired cmdargs, if not, trigger parse
    // error. This is not strictly neccesary as zKeyEventDispatch checks for this as well, but it's
    // probably better to not allow a key to be bound to a cmdstring that can't be executed. This
    // way the error becomes apparant while parsing the keybindings config.
    if ( curpcmd->numargs < curpcmd->command->numrequired ) {
        yyerror("not enough arguments given for command");
        YYERROR;
    }
}
       ;

cmdargs: | cmdargs cmdarg
{
    // Add this cmdarg unless we already have MAX_CMD_ARGS cmdargs.
    if (curpcmd->numargs < MAX_CMD_ARGS) {

        curpcmd->args[curpcmd->numargs].str_arg = str_arg;
        curpcmd->args[curpcmd->numargs].int_arg = int_arg;
        curpcmd->args[curpcmd->numargs].float_arg = float_arg;
        curpcmd->args[curpcmd->numargs].var_arg = var_arg;
        curpcmd->numargs += 1;

    } else {
        // No need to warn about this really, MAX_CMD_ARGS will always be large enough (I'll
        // increase it if I add a command that needs more arguments).
        //zWarning("One or more arguments for command \"%s\" exceeded MAX_CMD_ARGS and were ignored!",
        //    curpcmd->command->keyword);
        free(str_arg);
    }

    str_arg = NULL;
    int_arg = 0;
    float_arg = 0.0;
    var_arg = NULL;
}
       ;

cmdarg: STR_ARG
      | QSTR_ARG
      | INT_ARG
      | FLOAT_ARG
      ;

%%

#include "cmdstring_lex.c.inc"


// Parses given cmdstring into an array of ZParsedCommands. Returns number of commands parsed. The
// parsed commands are stored in an array and *parsedcmds will be set to point to it. The caller is
// responsible for freeing this array. Returns 0 and sets *parsedcmds to NULL when no commands could
// be parsed, or if there was a syntax error in the cmdstring. 
int zParseCmdString(const char *cmdstring, ZParsedCommand **caller_parsedcmds)
{
    int parse_result;
    YY_BUFFER_STATE bufstate;

    // Make sure I'll know if I don't clean up stuff correctly.
    assert(cmdname == NULL);
    assert(str_arg == NULL);
    assert(int_arg == 0);
    assert(float_arg == 0.0);

    numcmds = 0;
    buf_offset = 0;
    cmdstring_ptr = cmdstring;

    // Run the yacc parser.
    bufstate = yy_scan_string(cmdstring);
    parse_result = yyparse();
    yy_delete_buffer(bufstate);

    // Check is there was a an error (1 means there was an error).
    if ( parse_result == 1 ) {

        //zDebug("%s: Parser exited with error status.", __func__);

        // Make sure start condition is set to back INITIAL, this isn't done by defualt on error..
        BEGIN(INITIAL);

        // Clean up. Free everything allocated, since we pretend 0 commands were parsed if there was
        // a syntax error.
        zFreeParsedCmds(parsedcmds, numcmds);
        numcmds = 0;
        parsedcmds = NULL;

        free(str_arg);
        str_arg = NULL;

        float_arg = 0.0;
        int_arg = 0;
        var_arg = NULL;

        *caller_parsedcmds = NULL;

    } else {

        // Parse was successful.
        *caller_parsedcmds = parsedcmds;
    }

    parsedcmds = NULL;
    curpcmd = NULL;

    return numcmds;
}


