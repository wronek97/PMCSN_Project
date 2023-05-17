CC=gcc
LIBS=-lm
SRCDIR=source/
OUTDIR=bin/

all:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)msq.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)msq $(LIBS)
	$(CC) $(SRCDIR)msq_priority.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)msq_priority $(LIBS)
	$(CC) $(SRCDIR)msq_size.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)msq_size $(LIBS)
	$(CC) $(SRCDIR)msq_sjf.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)msq_sjf $(LIBS)
msq:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)msq.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)msq $(LIBS)
priority:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)msq_priority.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)msq_priority $(LIBS)
size:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)msq_size.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)msq_size $(LIBS)
sjf:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)msq_sjf.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)msq_sjf $(LIBS)

clean:
	rm -r $(OUTDIR)

