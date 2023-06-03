CC=gcc
FLAGS=-lm
LIBS=source/lib/
SRCDIR=source/
OUTDIR=bin/

all:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)MicroservicesApp_base.c $(LIBS)rngs.c $(LIBS)rvgs.c $(LIBS)rvms.c -o $(OUTDIR)simulation_base $(FLAGS)
	$(CC) $(SRCDIR)MicroservicesApp_resized.c $(LIBS)rngs.c $(LIBS)rvgs.c $(LIBS)rvms.c -o $(OUTDIR)simulation_resized $(FLAGS)
	$(CC) $(SRCDIR)MicroservicesApp_improved.c $(LIBS)rngs.c $(LIBS)rvgs.c $(LIBS)rvms.c -o $(OUTDIR)simulation_improved $(FLAGS)

initial:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)MicroservicesApp_base.c $(LIBS)rngs.c $(LIBS)rvgs.c $(LIBS)rvms.c -o $(OUTDIR)simulation_base $(FLAGS)
	
resized:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)MicroservicesApp_resized.c $(LIBS)rngs.c $(LIBS)rvgs.c $(LIBS)rvms.c -o $(OUTDIR)simulation_resized $(FLAGS)

improved:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)MicroservicesApp_improved.c $(LIBS)rngs.c $(LIBS)rvgs.c $(LIBS)rvms.c -o $(OUTDIR)simulation_improved $(FLAGS)

clean:
	rm -f -r $(OUTDIR)

