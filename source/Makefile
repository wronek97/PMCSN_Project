CC=gcc
FLAGS=-lm
LIBS=lib/
BINDIR=../bin/
RESDIR=../analysis/
TRANSIENT=transient/
STEADY_STATE=steady_state/

all:
	mkdir -p $(BINDIR)
	mkdir -p $(RESDIR)$(TRANSIENT)
	mkdir -p $(RESDIR)$(STEADY_STATE)
	$(CC) microservices.c $(LIBS)rngs.c $(LIBS)rvgs.c $(LIBS)rvms.c -o $(BINDIR)simulation $(FLAGS)

clean:
	rm -f -r $(BINDIR)
	rm -f -r $(RESDIR)

clean_bin:
	rm -f -r $(BINDIR)

clean_results:
	rm -f -r $(RESDIR)

