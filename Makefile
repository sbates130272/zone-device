ARGCONFIG=libargconfig
ARGCONFIG_INC=$(ARGCONFIG)/inc
LIBARGCONFIG=$(ARGCONFIG)/libargconfig.a

CPPFLAGS += -I$(ARGCONFIG_INC) -D_GNU_SOURCE
CFLAGS += -g -std=c99
LDLIBS += -lpthread

default: test perf

test: $(LIBARGCONFIG)
perf: $(LIBARGCONFIG)

$(LIBARGCONFIG):
	make -C $(ARGCONFIG) libargconfig.a

clean:
	rm -rf *.o test perf
	make -C $(ARGCONFIG) clean
