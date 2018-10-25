SPIKE=spike

OUTPUT_DIR ?= $(abspath .)
RISCVTESTS_DIR:=

TESTS_PATH = $(wildcard $(RISCVTESTS_DIR)/rv64*)
TEST_NAMES = $(basename $(notdir $(TESTS_PATH)))
CONFIGS_BASENAME = $(addprefix $(OUTPUT_DIR)/, $(TEST_NAMES))

RISCVEMU_DIR=$(dir $(realpath $(firstword) $(MAKEFILE_LIST)))/..
VHARNESS=$(RISCVEMU_DIR)/vharness
RISCVEMU=$(RISCVEMU_DIR)/riscvemu

all: configs riscv-tests-report checkpoint-tests linux_boot_tests

# FIXME: Add support for test-error reporting
# we should generate output that junit can consume and we can report in
# Jenkins

# # failures: riscvemu.testlog
# # 	grep -v "Power off" $<  > $@
# # 	wc -l $@

# # failures32: riscvemu32.testlog
# # 	grep -v "Power off" $<  > $@
# # 	wc -l $@

# # fp64.failures: failures
# # 	grep rv64ud $< > $@
# # 	wc -l $@

# # riscvemu32.testlog: $(VHARNESS) $(patsubst %,%.riscvemu,$(RV32TESTS))
# # 	for t in $(patsubst %,%.riscvemu,$(RV64TESTS)); do echo $$t `grep Power $$t|head -1`;done > $@

# # riscvemu.testlog: $(VHARNESS) $(patsubst %,%.riscvemu,$(RV64TESTS))
# # 	for t in $(patsubst %,%.riscvemu,$(RV64TESTS)); do echo $$t `grep Power $$t|head -1`;done > $@

$(OUTPUT_DIR)/%.riscvemu: $(OUTPUT_DIR)/%.cfg $(OUTPUT_DIR)/%.bin $(VHARNESS)
	$(VHARNESS) $< 2> $@

$(OUTPUT_DIR)/%.spike: $(RISCVTESTS_DIR)/%
	$(SPIKE) -l $< > $@ 2>&1

%.cmp: $(OUTPUT_DIR)/%.spike $(OUTPUT_DIR)/%.riscvemu
	diff -U100 $^ | less

# # I don't need this anymore as %.riscvemu depends on both
# .PRECIOUS: %.bin

$(OUTPUT_DIR)/%.bin: $(RISCVTESTS_DIR)/%
	riscv64-unknown-elf-objcopy -O binary $< $@

$(OUTPUT_DIR)/%.cfg: $(OUTPUT_DIR)/%.bin $(RISCVTESTS_DIR)/%
	echo "{version:1,machine:\"riscv64\",memory_size:128,bios:\"${<}\",htif_base_addr:0x"$$(nm $(word 2,$^)|grep 'D tohost'|grep tohost|cut -d' ' -f1)",}" > $@

configs: $(patsubst %,%.riscvemu,$(CONFIGS_BASENAME)) $(patsubst %,%.spike,$(CONFIGS_BASENAME))

$(OUTPUT_DIR)/riscvemu-test-report.xml: $(patsubst %,%.riscvemu,$(CONFIGS_BASENAME))
	./riscv_test_report.py --output-dir=$(OUTPUT_DIR) \
	  --output-xml=$(OUTPUT_DIR)/riscvemu-test-report.xml riscvemu

.PHONY: report
riscv-tests-report: $(OUTPUT_DIR)/riscvemu-test-report.xml

$(OUTPUT_DIR)/chpt-test-report.xml: $(OUTPUT_DIR)/rv64ui-p-simple.cfg  $(OUTPUT_DIR)/rv64ui-p-simple.bin
	pytest -s -v chpt_test_riscv_tests.py --output-dir $(OUTPUT_DIR)/ --vharness $(VHARNESS) \
	  --junit-xml=$@

.PHONY: checkpoint-test-riscv-test
checkpoint-test-riscv-test: $(OUTPUT_DIR)/chpt-test-report.xml

.PHONY: checkpoint-tests
checkpoint-tests: checkpoint-test-riscv-test

##### Linux boot related tests

# Download the reference image

KERNEL_VERSION=3facd4e6a0611bdab349ce28782a7c032c346855
KERNEL_S3_PATH=et-maxion-ci-inputs/bbl-vmlinux/$(KERNEL_VERSION)

$(OUTPUT_DIR)/firesim-golden-download.done:
	artifacts.py --pull $(KERNEL_S3_PATH)/bbl-vmlinux0.bin.xz $(OUTPUT_DIR)/bbl-vmlinux0.bin.xz
	artifacts.py --pull $(KERNEL_S3_PATH)/bbl-vmlinux0.cfg.xz $(OUTPUT_DIR)/bbl-vmlinux0.cfg.xz
	xz -d $(OUTPUT_DIR)/bbl-vmlinux0.bin.xz $(OUTPUT_DIR)/bbl-vmlinux0.cfg.xz
	date > $@

$(OUTPUT_DIR)/linux-boot-test-report.xml: $(OUTPUT_DIR)/firesim-golden-download.done
	pytest -s -v linux_boot_tests.py --output-dir $(OUTPUT_DIR)/ --vharness $(RISCVEMU) \
	  --junit-xml=$@

.PHONY: linux_boot_tests
linux_boot_tests: $(OUTPUT_DIR)/linux-boot-test-report.xml
