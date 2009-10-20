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
#define yymaxdepth  kbyy_maxdepth
#define yyparse     kbyy_parse
#define yylex       kbyy_lex
#define yyerror     kbyy_error
#define yylval      kbyy_lval
#define yychar      kbyy_char
#define yydebug     kbyy_debug
#define yypact      kbyy_pact
#define yyr1        kbyy_r1
#define yyr2        kbyy_r2
#define yydef       kbyy_def
#define yychk       kbyy_chk
#define yypgo       kbyy_pgo
#define yyact       kbyy_act
#define yyexca      kbyy_exca
#define yyerrflag   kbyy_errflag
#define yynerrs     kbyy_nerrs
#define yyps        kbyy_ps
#define yypv        kbyy_pv
#define yys         kbyy_s
#define yy_yys      kbyy_yys
#define yystate     kbyy_state
#define yytmp       kbyy_tmp
#define yyv         kbyy_v
#define yy_yyv      kbyy_yyv
#define yyval       kbyy_val
#define yylloc      kbyy_lloc
#define yyreds      kbyy_reds
#define yytoks      kbyy_toks
#define yylhs       kbyy_yylhs
#define yylen       kbyy_yylen
#define yydefred    kbyy_yydefred
#define yydgoto     kbyy_yydgoto
#define yysindex    kbyy_yysindex
#define yyrindex    kbyy_yyrindex
#define yygindex    kbyy_yygindex
#define yytable     kbyy_yytable
#define yycheck     kbyy_yycheck
#define yyname      kbyy_yyname
#define yyrule      kbyy_yyrule

// Some more symbols I had to add to the list myself.
#define yyin                kbyy_in
#define yyout               kbyy_out
#define yywrap              kbyy_wrap
#define yylineno            kbyy_lineno
#define yy_flex_debug       kbyy_flex_debug
#define yy_create_buffer    kbyy_create_buffer
#define yy_flush_buffer     kbyy_flush_buffer
#define yy_switch_to_buffer kbyy_switch_to_buffer
#define yy_delete_buffer    kbyy_delete_buffer
#define yypush_buffer_state kbyy_push_buffer_state
#define yypop_buffer_state  kbyy_pop_buffer_state
#define yy_scan_buffer      kbyy_scan_buffer
#define yy_scan_string      kbyy_scan_string
#define yy_scan_bytes       kbyy_scan_bytes
#define yylex_destroy       kbyy_lex_destroy
#define yyget_lineno        kbyy_get_lineno
#define yyget_in            kbyy_get_in
#define yyget_out           kbyy_get_out
#define yyget_leng          kbyy_get_leng
#define yyget_text          kbyy_get_text
#define yyget_debug         kbyy_get_debug
#define yyset_lineno        kbyy_set_lineno
#define yyset_in            kbyy_set_in
#define yyset_out           kbyy_set_out
#define yyset_debug         kbyy_set_debug
#define yyrestart           kbyy_restart
#define yyalloc             kbyy_alloc
#define yyrealloc           kbyy_realloc
#define yyfree              kbyy_free

// Redefining this here because the default ECHO macro uses fwrite in a way that causes a compiler
// warning (warn_unesed_result) and I don't need it anyway.
#define ECHO

// Prevent unistd.h from being included, this header doesn't exist on Windows.
#define YY_NO_UNISTD_H

#define YYSTYPE char *

extern int yylineno;
extern FILE *yyin;
void yyrestart(FILE *fp);
int yylex(void);

// Temporary storage for the keybinding currently being parsed.
static char *keyname;
static char *cmdline;
static unsigned int keystate;
static unsigned int modmask;

ZKeyEvent zkev;
ZKey key;

const char *cur_filename;

// Called when EOF is reached. Could be used to start parsing next file for example. Not really
// needed in my case.
static int yywrap(void)
{
    // Return true, means no further scanning is done when EOF is reached.
    return 1;
}

static void yyerror(const char *str)
{
    zWarning("Failed to parse keybinding in %s on line %d", cur_filename, yylineno);
}

%}

%token KEYNAME MODSHIFT MODSUPER MODCTRL MODLALT MODRALT CMDLINE END INVALID

%%

keybindings: | keybindings keybinding ;

keybinding: END | keystate KEYNAME modifier_keys ':' CMDLINE END
            {
                // Check that the key name is valid before adding it.
                if ( (key = zLookupKey(keyname)) != KEY_UNKNOWN ) {

                    zkev.key = key;
                    zkev.keystate = keystate;
                    zkev.modmask = modmask;

                    if ( !zAddKeyBinding(&zkev, cmdline) ) {
                        zWarning("Failed to process keybinding for \"%s\" while parsing %s on"
                            " line %d", keyname, cur_filename, yylineno);
                    }
                } else {
                    zWarning("Unknown key \"%s\" while parsing %s on line %d", keyname,
                    cur_filename, yylineno);
                    free(cmdline);
                }

                // Done with this keybinding, reinitialize for next.
                free(keyname);
                keyname = NULL;
                cmdline = NULL;
                keystate = 0;
                modmask = 0;
            }
          | error END
          ;

keystate:     { keystate = Z_KEY_STATE_PRESS; }
        | '+' { keystate = Z_KEY_STATE_PRESS; }
        | '-' { keystate = Z_KEY_STATE_RELEASE; }
        ;

modifier_keys: | '(' modkeylist ')' ;

modkeylist: modkey | modkeylist ',' modkey ;

modkey: MODSHIFT  { modmask |= Z_KEY_MOD_SHIFT; }
      | MODSUPER  { modmask |= Z_KEY_MOD_SUPER; }
      | MODCTRL   { modmask |= Z_KEY_MOD_CTRL;  }
      | MODLALT   { modmask |= Z_KEY_MOD_LALT;  }
      | MODRALT   { modmask |= Z_KEY_MOD_RALT;  }
      ;

%%

#include "keybindings_lex.c.inc"

// Parses keybindings, fp is filepointer from which input is read. Whenever a keybinding is
// succesfully parsed (meaning the given key and any optional modifier keys were valid and the
// cmdline was a non-empty string) it will call zAddKeyBinding, which will parse the commandline and
// add the keybinding if one or more commands were parsed. If a keybinding already exists it is
// overridden.
void zParseKeyBindings(FILE *fp, const char *filename)
{
    int parse_result;

    assert(cmdline == NULL);
    assert(keyname == NULL);
    assert(keystate == 0);
    assert(modmask == 0);

    cur_filename = filename;

    yyin = fp;
    yylineno = 1;

    // Run the yacc parser.
    parse_result = yyparse();

    //zDebug("%s: yyparse returned on line %d", __func__, yylineno);

    // Check is there was a an error (1 means there was an error).
    if ( parse_result == 1 ) {

        //zDebug("%s: yyparse exited with error status", __func__);

        // This yyrestart call shouldn't be neccesary according to the flex manual, but under some
        // conditions when a parse error occurs the input buffer doesn't get properly flushed or
        // something and despite re-assigning yyin to the new fp on the next invocation of
        // zParseKeyBindings, it would attempt to start parsing where the previous parse failed...
        yyrestart(fp);

        // Also make sure start condition is set to INITIAL.
        BEGIN(INITIAL);

        // Clean up temporary variables..
        free(keyname);
        keyname = NULL;

        free(cmdline);
        cmdline = NULL;

        keystate = 0;
        modmask = 0;
    }
}


