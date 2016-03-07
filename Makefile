ARGCONFIG=libargconfig
ARGCONFIG_INC=$(ARGCONFIG)/inc
LIBARGCONFIG=$(ARGCONFIG)/libargconfig.a

CPPFLAGS += -I$(ARGCONFIG_INC)
CFLAGS += -g -std=c99 -D_GNU_SOURCE

default: test perf

test: $(LIBARGCONFIG)
perf: $(LIBARGCONFIG)

$(LIBARGCONFIG):
	make -C $(ARGCONFIG) libargconfig.a

clean:
	rm -rf *.o test perf
	make -C $(ARGCONFIG) clean
