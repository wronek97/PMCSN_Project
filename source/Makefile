CC=gcc
FLAGS=-lm
LIBS=lib/
OUTDIR=../bin/

all:
	mkdir -p $(OUTDIR)
	$(CC) MicroservicesApp_base.c $(LIBS)rngs.c $(LIBS)rvgs.c $(LIBS)rvms.c -o $(OUTDIR)simulation_base $(FLAGS)
	$(CC) MicroservicesApp_resized.c $(LIBS)rngs.c $(LIBS)rvgs.c $(LIBS)rvms.c -o $(OUTDIR)simulation_resized $(FLAGS)
	$(CC) MicroservicesApp_improved.c $(LIBS)rngs.c $(LIBS)rvgs.c $(LIBS)rvms.c -o $(OUTDIR)simulation_improved $(FLAGS)

initial:
	mkdir -p $(OUTDIR)
	$(CC) MicroservicesApp_base.c $(LIBS)rngs.c $(LIBS)rvgs.c $(LIBS)rvms.c -o $(OUTDIR)simulation_base $(FLAGS)
	
resized:
	mkdir -p $(OUTDIR)
	$(CC) MicroservicesApp_resized.c $(LIBS)rngs.c $(LIBS)rvgs.c $(LIBS)rvms.c -o $(OUTDIR)simulation_resized $(FLAGS)

improved:
	mkdir -p $(OUTDIR)
	$(CC) MicroservicesApp_improved.c $(LIBS)rngs.c $(LIBS)rvgs.c $(LIBS)rvms.c -o $(OUTDIR)simulation_improved $(FLAGS)

clean:
	rm -f -r $(OUTDIR)

