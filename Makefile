CC=gcc
LIBS=-lm
SRCDIR=source/
OUTDIR=bin/

all:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)msq.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)msq $(LIBS)
	$(CC) $(SRCDIR)ssq3.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)ssq $(LIBS)
	$(CC) $(SRCDIR)msq_limited_cap.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)msq_limited_cap $(LIBS)
	$(CC) $(SRCDIR)msq_priority.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)msq_priority $(LIBS)
	$(CC) $(SRCDIR)msq_size.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)msq_size $(LIBS)
	$(CC) $(SRCDIR)msq_sjf.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)msq_sjf $(LIBS)
	$(CC) $(SRCDIR)MetalPartsProduction.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(SRCDIR)rvms.c $(OUTDIR)MetalPartsProduction $(LIBS)
ssq:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)ssq3.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)ssq $(LIBS)
msq:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)msq.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)msq $(LIBS)
limited_cap:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)msq_limited_cap.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)msq_limited_cap $(LIBS)
priority:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)msq_priority.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)msq_priority $(LIBS)
size:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)msq_size.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)msq_size $(LIBS)
sjf:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)msq_sjf.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c -o $(OUTDIR)msq_sjf $(LIBS)
project:
	mkdir -p $(OUTDIR)
	$(CC) $(SRCDIR)MetalPartsProduction.c $(SRCDIR)rngs.c $(SRCDIR)rvgs.c $(SRCDIR)rvms.c -o $(OUTDIR)MetalPartsProduction $(LIBS)

clean:
	rm -f -r $(OUTDIR)

