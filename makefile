INCLUDES:=include/rsrc.h include/iot_doubly_linked_list.h
COREFILES:=rsrc.c
TESTS_C:=$(wildcard tests/*.c)
SOURCES:=$(TESTS_C) rsrc.c
TESTS:=$(TESTS_C:.c=)
CFLAGS:= -I include -std=c99

all: maketests runtests

maketests: $(TESTS) tests/test1_oom

$(TESTS): % : %.c $(COREFILES) $(INCLUDES) makefile
	$(CC) $(CFLAGS) $< $(COREFILES) -o $@

runtests: $(TESTS) tests/test1_oom
	$(TESTS:%=%;) tests/test1_oom

# tests/test1.c has a special-case test in it to try a resource double-free
# typically, it's not part of a normal run since it fails.
tests/testdubfree: tests/test1.c $(COREFILES) $(INCLUDES) makefile
	$(CC) $(CFLAGS) -DrsrcDOUBLE_FREE tests/test1.c $(COREFILES) -o tests/test1_dubfree
	@echo "====Testing double-free -- this test SHOULD abort(). That's SUCCESS."
	-tests/test1_dubfree

# tests/test1.c and rsrc.c have special-case code that simulates a NULL return
# from malloc().  Set rsrcTEST_FORCE_OOM to a modestly large number.
tests/test1_oom: tests/test1.c $(COREFILES) $(INCLUDES) makefile
	$(CC) $(CFLAGS) -DrsrcTEST_FORCE_OOM=500 tests/test1.c $(COREFILES) -o tests/test1_oom

clean:
	-rm $(TESTS) tests/test1_oom tests/test1_dubfree
