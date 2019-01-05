#******************************************************************************
# Copyright (C) 2018, Esperanto Technologies Inc.
# The copyright to the computer program(s) herein is the
# property of Esperanto Technologies, Inc. All Rights Reserved.
# The program(s) may be used and/or copied only with
# the written permission of Esperanto Technologies and
# in accordance with the terms and conditions stipulated in the
# agreement/contract under which the program(s) have been supplied.
#------------------------------------------------------------------------------

SPIKE=spike

OUTPUT_DIR ?= $(abspath .)
RISCVTESTS_DIR:=

TESTS_PATH = $(wildcard $(RISCVTESTS_DIR)/rv64*)
TEST_NAMES = $(basename $(notdir $(TESTS_PATH)))
CONFIGS_BASENAME = $(addprefix $(OUTPUT_DIR)/, $(TEST_NAMES))

RISCVEMU_DIR=$(dir $(realpath $(firstword) $(MAKEFILE_LIST)))/..
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

# # riscvemu32.testlog: $(RISCVEMU) $(patsubst %,%.riscvemu,$(RV32TESTS))
# # 	for t in $(patsubst %,%.riscvemu,$(RV64TESTS)); do echo $$t `grep Power $$t|head -1`;done > $@

# # riscvemu.testlog: $(RISCVEMU) $(patsubst %,%.riscvemu,$(RV64TESTS))
# # 	for t in $(patsubst %,%.riscvemu,$(RV64TESTS)); do echo $$t `grep Power $$t|head -1`;done > $@

$(OUTPUT_DIR)/%.riscvemu: $(OUTPUT_DIR)/%.cfg $(OUTPUT_DIR)/%.bin $(RISCVEMU)
	$(RISCVEMU) --trace 0 $< 2> $@

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
	pytest -v chpt_test_riscv_tests.py --output-dir $(OUTPUT_DIR)/ --riscvemu $(RISCVEMU) \
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
	xz -fd $(OUTPUT_DIR)/bbl-vmlinux0.bin.xz $(OUTPUT_DIR)/bbl-vmlinux0.cfg.xz
	date > $@

# Have a separate test-file per linux-boot-test
# Linux boot takes 6min and we need to be able to parallelize it
LINUX_BOOT_TESTS = \
	linux_boot_chkpt_test.py \
	linux_boot_test_csr_validation_write.py \
	linux_boot_prompt_message.py


LINUX_BOOT_TEST_OUTPUT=$(foreach test,$(patsubst %.py,%.xml,$(LINUX_BOOT_TESTS)),$(OUTPUT_DIR)/$(test))

$(OUTPUT_DIR)/linux_boot_%.xml: linux_boot_%.py $(OUTPUT_DIR)/firesim-golden-download.done
	pytest -s -v $< --output-dir $(OUTPUT_DIR)/ --riscvemu $(RISCVEMU) \
	  --junit-xml=$@ 1>$(patsubst %.xml,%.log,$@) 2>&1

.PHONY: linux_boot_tests
linux_boot_tests: $(LINUX_BOOT_TEST_OUTPUT)
