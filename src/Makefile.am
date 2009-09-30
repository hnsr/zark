
DEFS = -DASSET_DIR='"$(ASSET_DIR)"' @DEFS@

bin_PROGRAMS = zark

zark_SOURCES = variables.def\
			   cmdsymbols.def\
			   keys.def\
			   common.h\
			   renderer.h\
			   renderer.c\
			   camera.h\
			   camera.c\
			   main.h\
			   main.c\
			   zmath.h\
			   zmath.c\
			   image.h\
			   image.c\
			   material.h\
			   material.c\
			   shader.h\
			   shader.c\
			   mesh.h\
			   mesh.c\
			   mesh_loader_obj.c\
			   mesh_loader_ply.c\
			   os.h\
			   os.c\
			   util.h\
			   util.c\
			   input.h\
			   input.c\
			   command.h\
			   command.c\
			   variables.h\
			   variables.c\
			   textrender.h\
			   textrender.c\
			   console.h\
			   console.c\
			   scene.h\
			   scene.c\
			   script.h\
			   script.c\
			   keybindings_yacc.y\
			   cmdstring_yacc.y\
			   keybindings_lex.c.inc\
			   cmdstring_lex.c.inc

# Using explicit rules for producing the lexer C source, since I can't find any other way to make
# this work well with automake... automake insists on compiling the output of lex, which is a
# problem because that will only work if I can set the (non-portable) prefix option, and I'll also
# need to include the header from yacc, which introduces extra hurdles... So instead I include the C
# output from lex directly in the yacc sources.
#
# http://sources.redhat.com/ml/automake/2005-03/msg00072.html
# http://sources.redhat.com/ml/automake/1999-11/msg00127.html
#  \ http://sources.redhat.com/ml/automake/1999-11/msg00130.html
# Note 2 at http://flex.sourceforge.net/manual/Makefiles-and-Flex.html
# http://www.archivum.info/automake@gnu.org/2008-07/msg00037.html
# http://lists.gnu.org/archive/html/automake/2009-05/msg00069.html

# Because of the explicit make rules I need this EXTRA_DIST to make sure the lex sources are
# indluded in the dist. Since the lex output is included in the dist as well, they aren't actually
# used, but make would otherwise error out because the dependency needs to be satisfied.
EXTRA_DIST = cmdstring_lex.l keybindings_lex.l


keybindings_yacc.c: keybindings_lex.c.inc

keybindings_lex.c.inc: keybindings_lex.l
	$(LEX) -okeybindings_lex.c.inc keybindings_lex.l

cmdstring_yacc.c: cmdstring_lex.c.inc

cmdstring_lex.c.inc: cmdstring_lex.l
	$(LEX) -ocmdstring_lex.c.inc cmdstring_lex.l


