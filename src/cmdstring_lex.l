%option nounput noinput

%x sc_cmdargs

%%

[a-zA-Z0-9_]+ {
    // Make sure we don't lose a pointer if this token happens to get matched twice.
    free(cmdname);
    cmdname = strdup(yytext);
    BEGIN(sc_cmdargs);
    return CMDNAME;
}

<*>[ \t\n]+ {
    /* absorb whitespace */
}


<sc_cmdargs>\"[^\"]*\" {

    free(str_arg);
    str_arg = strdup(yytext+1);
    str_arg[yyleng-2] = '\0';

    // Use str_arg since it has the quotation removed..
    int_arg = atoi(str_arg);
    float_arg = atof(str_arg);
    var_arg = zLookupVariable(str_arg);

    UPDATE_ARG_OFFSETS;
    return QSTR_ARG;
}

<sc_cmdargs>[-]*[0-9]*\.[0-9]+ {

    free(str_arg);
    str_arg = strdup(yytext);
    int_arg = atoi(yytext);
    float_arg = atof(yytext);

    UPDATE_ARG_OFFSETS;
    return FLOAT_ARG;
}

<sc_cmdargs>[-]*[0-9]+ {

    free(str_arg);
    str_arg = strdup(yytext);
    int_arg = atoi(yytext);
    float_arg = atof(yytext);

    UPDATE_ARG_OFFSETS;
    return INT_ARG;
}

<*>; {
    BEGIN(INITIAL);
    return CMDEND;
}

<sc_cmdargs><<EOF>> {
    BEGIN(INITIAL);
    return CMDEND;
}

<sc_cmdargs>[^ \t\n;]+ {

    free(str_arg);
    str_arg = strdup(yytext);
    int_arg = zLookupCmdArgSymbol(yytext);
    float_arg = (double) zLookupCmdArgSymbol(yytext);
    //float_arg = atof(yytext);
    var_arg = zLookupVariable(yytext);

    UPDATE_ARG_OFFSETS;
    return STR_ARG;
}

<*>. {
    return yytext[0];
}

%%
