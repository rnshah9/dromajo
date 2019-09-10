CC=riscv64-unknown-elf-gcc -mcmodel=medany
OPT=-g
OPT=-O
LDFLAGS=-nostdlib
CFLAGS=-std=gnu99 -Iinclude $(OPT)
TOP=main

top: $(TOP).out

$(TOP).dis: $(TOP)
	riscv64-unknown-elf-objdump -d $^ > $@


$(TOP): $(TOP).o
	riscv64-unknown-elf-gcc $(LDFLAGS) $^ -Wl,-Ttext=0x8000000000 -o $@

trace: $(TOP)
	../../riscvemu --memory_addr=0x8000000000 --trace=0 ./main |& ~/boom-template/scripts/commitlog-helper.py | spike-dasm

run: $(TOP)
	../../riscvemu --memory_addr=0x8000000000 ./main

$(TOP).bin: $(TOP)
	riscv64-unknown-elf-objcopy -O binary $< $@

$(TOP).cfg: $(TOP).bin
	echo "{version:1,machine:\"riscv64\",memory_size:256,bios:\"$(TOP).bin\",memory_base_addr:0x8000000000}" > $@

$(TOP).hex: $(TOP)
	riscv64-unknown-elf-elf2hex --bit-width 64 --input $< > $@

$(TOP).out: $(TOP).hex $(TOP).cfg ../../../verisim/simulator-boom.system-SmallBoomConfig
	../../../verisim/simulator-boom.system-SmallBoomConfig \
	   +dump-stats=$(TOP).stats.json \
	   +disable-dtm \
	   +riscvemu-cfg=$(TOP).cfg \
	   +bootramhex=../../../verisim/generated-src/cosim.hex \
	   +ramhex=$(TOP).hex \
           +max-cycles=10000000 \
	   +verbose \
           $(TOP) 3>&1 1>&2 2>&3 | \
	spike-dasm > $@

../../../verisim/simulator-boom.system-SmallBoomConfig:
	make -C ../../../verisim CONFIG=SmallBoomConfig
