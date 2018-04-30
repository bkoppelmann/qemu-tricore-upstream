AS := tricore-as
LD := tricore-ld
HOST_CC = gcc

LDFLAGS = -Tlink.ld
ASFLAGS = --keep-locals

SIM = ../../../tricore-softmmu/qemu-system-tricore
SIMFLAGS = -M tricore_testboard -nographic -kernel

TESTCASES += test_abs.tst
TESTCASES += test_clz.tst
TESTCASES += test_ftoi.tst

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

clean:
	$(RM) -fr $(TESTCASES) $(CRT) linker.ld
