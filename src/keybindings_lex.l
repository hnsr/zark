%option nounput noinput

%x sc_modkeys sc_cmdline

%%

(\/\/|#).* {
    /* Gobble up comments */
}

<*>[ \t]+ {
    /* Gobble up whitespace */;
}

:[ \t]* {
    // Match colon, absorb any trailing whitespace, return colon character as token.
    BEGIN(sc_cmdline);
    return yytext[0];
}

<sc_cmdline>.*[^ \t\n] {

    // Match any string with the last (which may be the only) char not being whitespace.

    // See note for KEYNAME on why this free() is needed.
    free(cmdline);
    cmdline = strdup(yytext);
    return CMDLINE;
}

\( {
    // Reset modmask.. (can be left at non-zero due to parse error)
    BEGIN(sc_modkeys);
    return yytext[0];
}


<sc_modkeys>CTRL  { return MODCTRL;  }
<sc_modkeys>SHIFT { return MODSHIFT; }
<sc_modkeys>SUPER { return MODSUPER; }
<sc_modkeys>LALT  { return MODLALT;  }
<sc_modkeys>RALT  { return MODRALT;  }

<sc_modkeys>\) {
    BEGIN(INITIAL);
    return yytext[0];
}

[a-zA-Z0-9_]+ {
    // Make sure modmask is 0 when we start parsing a new key.
    modmask = 0;

    // Need to free if not null, because on bad syntax the lexer might read two keyname tokens in a
    // row, and will have already overridden the previous strdupped string before the parser can do
    // anything about it...
    free(keyname);
    keyname = strdup(yytext);
    return KEYNAME;
}

<*>\n {
    yylineno++;

    // Always revert to initial state on newline.
    BEGIN(INITIAL);
    return END;
}

<sc_cmdline><<EOF>> {
    // Always revert to initial state on eof.
    BEGIN(INITIAL);
    return END;
}

<*>[-+,] {
    // Pass along a select number of characters that I want to match directly in the yacc grammer.
    // This lets me still keep the INVALID rule below, although I forgot why I wanted it in the
    // first place :( Something to do with not aborting on multi-byte unicode characters..
    return yytext[0];
}

<*>. {
    // Anything not matched at this point is unrecognized and gets returned as INVALID
    return INVALID;
}

%%
