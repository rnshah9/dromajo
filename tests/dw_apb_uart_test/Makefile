CC=riscv64-unknown-elf-gcc -mcmodel=medany
OPT=-g
OPT=-O
LDFLAGS=-nostdlib
CFLAGS=-std=gnu99 -Iinclude $(OPT)
TOP=main

$(TOP).dis: $(TOP)
	riscv64-unknown-elf-objdump -d $^ > $@


$(TOP): $(TOP).o DW_apb_uart.o
	riscv64-unknown-elf-gcc $(LDFLAGS) $^ -Wl,-Ttext=0x8000000000 -o $@

trace: $(TOP)
	../../riscvemu --memory_addr=0x8000000000 --trace=0 ./main |& ~/boom-template/scripts/commitlog-helper.py | spike-dasm

run: $(TOP)
	../../riscvemu --memory_addr=0x8000000000 ./main
