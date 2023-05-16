CC=gcc
LIBS=-lm
OUTDIR=output/

all: msq priority size sjf

msq:
	$(CC) msq.c rngs.c rvgs.c -o $(OUTDIR)msq $(LIBS)
priority:
	$(CC) msq_priority.c rngs.c rvgs.c -o $(OUTDIR)msq_priority $(LIBS)
size:
	$(CC) msq_size.c rngs.c rvgs.c -o $(OUTDIR)msq_size $(LIBS)
sjf:
	$(CC) msq_sjf.c rngs.c rvgs.c -o $(OUTDIR)msq_sjf $(LIBS)

clean:
	rm -f $(OUTDIR)msq $(OUTDIR)msq_priority $(OUTDIR)msq_size msq_sjf
