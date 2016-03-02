EXE=test

ARGCONFIG=libargconfig
ARGCONFIG_SRC=$(ARGCONFIG)/src
ARGCONFIG_INC=$(ARGCONFIG)/inc/argconfig/

CFLAGS += -g -std=c99 -D_GNU_SOURCE -I$(ARGCONFIG_INC)

default: $(EXE)

$(EXE):argconfig.o suffix.o report.o

argconfig.o: $(ARGCONFIG_SRC)/argconfig.c $(ARGCONFIG_INC)/argconfig.h $(ARGCONFIG_INC)/suffix.h
	$(CC) -c $(CFLAGS) $(ARGCONFIG_SRC)/argconfig.c

suffix.o: $(ARGCONFIG_SRC)/suffix.c $(ARGCONFIG_INC)/suffix.h
	$(CC) -c $(CFLAGS) $(ARGCONFIG_SRC)/suffix.c

report.o: $(ARGCONFIG_SRC)/report.c $(ARGCONFIG_INC)/report.h $(ARGCONFIG_INC)/suffix.h
	$(CC) -c $(CFLAGS) $(ARGCONFIG_SRC)/report.c

clean:
	rm -rf $(EXE) *.o *~
