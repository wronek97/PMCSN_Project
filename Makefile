CC=gcc
LIBS=-lm
SRCDIR=source/
OUTDIR=bin/

all:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)MicroservicesApp.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c $(SRCDIR)rvms.c -o $(OUTDIR)MicroservicesApp $(LIBS)
	$(CC) $(SRCDIR)MicroservicesApp_resized.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c $(SRCDIR)rvms.c -o $(OUTDIR)MicroservicesApp_resized $(LIBS)
	$(CC) $(SRCDIR)MicroservicesApp_improved.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c $(SRCDIR)rvms.c -o $(OUTDIR)MicroservicesApp_improved $(LIBS)

initial:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)MicroservicesApp.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c $(SRCDIR)rvms.c -o $(OUTDIR)MicroservicesApp $(LIBS)
	
resized:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)MicroservicesApp_resized.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c $(SRCDIR)rvms.c -o $(OUTDIR)MicroservicesApp_resized $(LIBS)

improved:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)MicroservicesApp_improved.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c $(SRCDIR)rvms.c -o $(OUTDIR)MicroservicesApp_improved $(LIBS)

clean:
	rm -f -r $(OUTDIR)

