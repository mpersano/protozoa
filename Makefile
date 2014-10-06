GCC	= gcc
CFLAGS	+= -Wall -g -O2
YFLAGS	= -d
YACC	= bison -y
LEX	= flex
MV	= mv

OBJDIR = $(shell uname --machine)

$(shell sh -c 'if ! test -d $(OBJDIR); then mkdir $(OBJDIR); fi')

OBJS = \
	$(OBJDIR)/main.o \
	$(OBJDIR)/panic.o \
	$(OBJDIR)/in_game.o \
	$(OBJDIR)/ship.o \
	$(OBJDIR)/missile.o \
	$(OBJDIR)/powerup.o \
	$(OBJDIR)/foe.o \
	$(OBJDIR)/arena.o \
	$(OBJDIR)/mesh.o \
	$(OBJDIR)/particle.o \
	$(OBJDIR)/explosion.o \
	$(OBJDIR)/hash_table.o \
	$(OBJDIR)/dict.o \
	$(OBJDIR)/image.o \
	$(OBJDIR)/matrix.o \
	$(OBJDIR)/gl_util.o \
	$(OBJDIR)/list.o \
	$(OBJDIR)/font.o \
	$(OBJDIR)/font_render.o \
	$(OBJDIR)/yyerror.o \
	$(OBJDIR)/fontdef_y_tab.o \
	$(OBJDIR)/fontdef_lex_yy.o \
	$(OBJDIR)/water.o \
	$(OBJDIR)/background.o \
	$(OBJDIR)/dna.o \
	$(OBJDIR)/menu.o \
	$(OBJDIR)/main_menu.o \
	$(OBJDIR)/settings_menu.o \
	$(OBJDIR)/in_game_menu.o \
	$(OBJDIR)/mt19937int.o \
	$(OBJDIR)/settings_y_tab.o \
	$(OBJDIR)/settings_lex_yy.o \
	$(OBJDIR)/in_game_text.o \
	$(OBJDIR)/levels_y_tab.o \
	$(OBJDIR)/levels_lex_yy.o \
	$(OBJDIR)/audio.o \
	$(OBJDIR)/laser.o \
	$(OBJDIR)/highscores.o \
	$(OBJDIR)/highscore_input.o \
	$(OBJDIR)/settings_file.o \
	$(OBJDIR)/bomb.o \
	$(OBJDIR)/bubbles.o \
	$(OBJDIR)/playback.o \
	$(OBJDIR)/credits.o \
	$(OBJDIR)/cursor.o

OUTPUT = $(OBJDIR)/game

# LIBS += ../siod/$(OBJDIR)/libsiod.a -lm -ldl `sdl-config --libs` -lGL -lGLU
LIBS += -lm -ldl `sdl-config --libs` -lGL -lGLU -lCg -lCgGL -lpng12 -lGLEW -lopenal -logg -lvorbis -lvorbisfile # -lSDL_mixer

CFLAGS += `sdl-config --cflags` -DWRITE_YUV -DUSE_CG -DFRAMESKIP -DUSE_OPENAL # -DDEBUG_SERIALIZE

$(OUTPUT): $(OBJS)
	$(CC) -o $(OUTPUT) $(OBJS) $(LIBS)

$(OBJS):
	$(CC) -c $(CFLAGS) $(subst $(OBJDIR)/,, $*.c) -o $*.o

clean:
	find \( -name core -o -name '*.o' -o -name '*.a' \) -exec rm -f {} \;
	rm -fr $(OBJDIR)
	rm -fr fontdef_y_tab.[ch] fontdef_lex_yy_i.h
	rm -fr settings_y_tab.[ch] settings_lex_yy_i.h
	rm -fr levels_y_tab.[ch] levels_lex_yy_i.h

fontdef_y_tab.c fontdef_y_tab.h: fontdef.y
	$(YACC) $(YFLAGS) -b $(<:.y=) $<
	$(MV) $(<:.y=).tab.c $(<:.y=_y_tab.c)
	$(MV) $(<:.y=).tab.h $(<:.y=_y_tab.h)

fontdef_lex_yy_i.h: fontdef.l fontdef_y_tab.h
	$(LEX) $(LFLAGS) -o$@ $<

settings_y_tab.c settings_y_tab.h: settings.y
	$(YACC) $(YFLAGS) -b $(<:.y=) $<
	$(MV) $(<:.y=).tab.c $(<:.y=_y_tab.c)
	$(MV) $(<:.y=).tab.h $(<:.y=_y_tab.h)

settings_lex_yy_i.h: settings.l settings_y_tab.h
	$(LEX) $(LFLAGS) -o$@ $<

levels_y_tab.c levels_y_tab.h: levels.y
	$(YACC) $(YFLAGS) -b $(<:.y=) $<
	$(MV) $(<:.y=).tab.c $(<:.y=_y_tab.c)
	$(MV) $(<:.y=).tab.h $(<:.y=_y_tab.h)

levels_lex_yy_i.h: levels.l levels_y_tab.h
	$(LEX) $(LFLAGS) -o$@ $<

$(OBJDIR)/main.o:		main.c
$(OBJDIR)/panic.o:		panic.c
$(OBJDIR)/in_game.o:		in_game.c
$(OBJDIR)/ship.o:		ship.c
$(OBJDIR)/missile.o:		missile.c
$(OBJDIR)/powerup.o:		powerup.c
$(OBJDIR)/foe.o:		foe.c
$(OBJDIR)/arena.o:		arena.c
$(OBJDIR)/mesh.o:		mesh.c
$(OBJDIR)/particle.o:		particle.c
$(OBJDIR)/explosion.o:		explosion.c
$(OBJDIR)/image.o:		image.c
$(OBJDIR)/matrix.o:		matrix.c
$(OBJDIR)/gl_util.o:		gl_util.c
$(OBJDIR)/list.o:		list.c
$(OBJDIR)/font.o:		font.c
$(OBJDIR)/yyerror.o:		yyerror.c
$(OBJDIR)/font_render.o:	font_render.c
$(OBJDIR)/fontdef_y_tab.o:	fontdef_y_tab.c
$(OBJDIR)/fontdef_lex_yy.o:	fontdef_lex_yy.c fontdef_lex_yy_i.h fontdef_y_tab.h
$(OBJDIR)/water.o:		water.c
$(OBJDIR)/background.o:		background.c
$(OBJDIR)/dna.o:		dna.c
$(OBJDIR)/menu.o:		menu.c
$(OBJDIR)/main_menu.o:		main_menu.c
$(OBJDIR)/settings_menu.o:	settings_menu.c
$(OBJDIR)/in_game_menu.o:	in_game_menu.c
$(OBJDIR)/mt19937int.o:		mt19937int.c
$(OBJDIR)/settings_y_tab.o:	settings_y_tab.c
$(OBJDIR)/settings_lex_yy.o:	settings_lex_yy.c settings_lex_yy_i.h settings_y_tab.h
$(OBJDIR)/in_game_text.o:	in_game_text.c
$(OBJDIR)/levels_y_tab.o:	levels_y_tab.c
$(OBJDIR)/levels_lex_yy.o:	levels_lex_yy.c levels_lex_yy_i.h levels_y_tab.h
$(OBJDIR)/audio.o:		audio.c
$(OBJDIR)/laser.o:		laser.c
$(OBJDIR)/highscores.o:		highscores.c
$(OBJDIR)/highscore_input.o:	highscore_input.c
$(OBJDIR)/settings.o:	settings.c
$(OBJDIR)/bomb.o:	bomb.c
$(OBJDIR)/bubbles.o:	bubbles.c
$(OBJDIR)/playback.o:	playback.c
$(OBJDIR)/credits.o:	credits.c
$(OBJDIR)/cursor.o:	cursor.c
$(OBJDIR)/settings_file.o: settings_file.c
