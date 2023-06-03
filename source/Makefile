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
	$(CC) MicroservicesApp_base.c $(LIBS)rngs.c $(LIBS)rvgs.c $(LIBS)rvms.c -o $(BINDIR)simulation_base $(FLAGS)
	$(CC) MicroservicesApp_resized.c $(LIBS)rngs.c $(LIBS)rvgs.c $(LIBS)rvms.c -o $(BINDIR)simulation_resized $(FLAGS)
	$(CC) MicroservicesApp_improved.c $(LIBS)rngs.c $(LIBS)rvgs.c $(LIBS)rvms.c -o $(BINDIR)simulation_improved $(FLAGS)

base:
	mkdir -p $(BINDIR)
	mkdir -p $(RESDIR)$(TRANSIENT)
	mkdir -p $(RESDIR)$(STEADY_STATE)
	$(CC) MicroservicesApp_base.c $(LIBS)rngs.c $(LIBS)rvgs.c $(LIBS)rvms.c -o $(BINDIR)simulation_base $(FLAGS)
	
resized:
	mkdir -p $(BINDIR)
	mkdir -p $(RESDIR)$(TRANSIENT)
	mkdir -p $(RESDIR)$(STEADY_STATE)
	$(CC) MicroservicesApp_resized.c $(LIBS)rngs.c $(LIBS)rvgs.c $(LIBS)rvms.c -o $(BINDIR)simulation_resized $(FLAGS)

improved:
	mkdir -p $(BINDIR)
	mkdir -p $(RESDIR)$(TRANSIENT)
	mkdir -p $(RESDIR)$(STEADY_STATE)
	$(CC) MicroservicesApp_improved.c $(LIBS)rngs.c $(LIBS)rvgs.c $(LIBS)rvms.c -o $(BINDIR)simulation_improved $(FLAGS)

clean:
	rm -f -r $(BINDIR)
	rm -f -r $(RESDIR)

clean_bin:
	rm -f -r $(BINDIR)

clean_results:
	rm -f -r $(RESDIR)

