INCLUDES:=include/rsrc.h include/iot_doubly_linked_list.h
COREFILES:=rsrc.c
TESTS_C:=$(wildcard tests/*.c)
SOURCES:=$(TESTS_C) rsrc.c
TESTS:=$(TESTS_C:.c=)
CFLAGS:= -I include

all: maketests runtests

maketests: $(TESTS)
	@echo "======================================================================== Made all tests"

$(TESTS): % : %.c $(COREFILES)
	$(CC) $(CFLAGS) $< $(COREFILES) -o $@

runtests: $(TESTS)
	@echo "======================================================================== Running all tests"
	$(TESTS:%=%;)

clean:
	-rm $(TESTS)
