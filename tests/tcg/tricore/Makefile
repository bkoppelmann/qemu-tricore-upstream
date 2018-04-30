AS := tricore-as
LD := tricore-ld
HOST_CC = gcc

LDFLAGS = -Tlink.ld
ASFLAGS =

TESTCASES += test_abs.tst

all: build

%.pS: %.S
	$(HOST_CC) -E -o $@ $<

%.o: %.pS
	$(AS) $(ASFLAGS) -o $@ $<

%.tst: %.o link.ld
	$(LD) $(LDFLAGS) $< -o $@

build: $(TESTCASES)

check: $(addprefix run-, $(TESTCASES))

run-%.tst: %.tst
	$(SIM) $(SIMFLAGS) ./$<

run-test_fail.tst: test_fail.tst
	! $(SIM) $(SIMFLAGS) ./$<

debug-%.tst: %.tst
	$(SIM) $(SIMDEBUG) $(SIMFLAGS) ./$<

host-debug-%.tst: %.tst
	gdb --args $(SIM) $(SIMFLAGS) ./$<

clean:
	$(RM) -fr $(TESTCASES) $(CRT) linker.ld
