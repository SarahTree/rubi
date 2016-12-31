CFLAGS = -Wall -m32 -g -mstackrealign -std=gnu99 -O2
C = $(CC) $(CFLAGS)


rubi: lex.o engine.o parser.o 
	$(C) -o $@ $^

minilua: dynasm/minilua.c
	$(CC) -Wall -std=gnu99 -O2 -o $@ $< -lm

engine.o: engine.c rubi.h
	$(C) -o $@ -c engine.c

lex.o: lex.c
	$(C) -o $@ -c lex.c

parser.o: parser.c
	$(C) -o $@ -c parser.c

clean:
	$(RM) a.out rubi minilua *.o *~ text
