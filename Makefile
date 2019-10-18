#******************************************************************************
# Copyright (C) 2018,2019, Esperanto Technologies Inc.
# The copyright to the computer program(s) herein is the
# property of Esperanto Technologies, Inc. All Rights Reserved.
# The program(s) may be used and/or copied only with
# the written permission of Esperanto Technologies and
# in accordance with the terms and conditions stipulated in the
# agreement/contract under which the program(s) have been supplied.
#------------------------------------------------------------------------------

#
# Dromajo, a RISC-V Emulator (based on Fabrice Bellard's RISCVEMU/TinyEMU)
#
# Copyright (c) 2016-2017 Fabrice Bellard
# Copyright (c) 2018,2019 Esperanto Technology
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

CXX=g++
CFLAGS_OPT=-Ofast
CXXFLAGS=$(CFLAGS_OPT) -Wall -std=c++11 -g -Werror -Wno-parentheses -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -MMD
CXXFLAGS+=-D_GNU_SOURCE -DCONFIG_VERSION=\"$(shell cat VERSION)\"
#CXXFLAGS+=-DLIVECACHE
LDFLAGS=

bindir=/usr/local/bin
INSTALL=install
PROGS=dromajo libdromajo_cosim.a dromajo_cosim_test

all: $(PROGS)

EMU_OBJS:=virtio.o pci.o fs.o cutils.o iomem.o dw_apb_uart.o \
    json.o machine.o elf64.o LiveCache.o fs_disk.o

EMU_LIBS=-lrt

dromajo: dromajo.o libdromajo_cosim.a
	$(CXX) $(LDFLAGS) -o $@ $^ $(DROMAJO_LIBS) $(EMU_LIBS)

dromajo_cosim_test: dromajo_cosim_test.o libdromajo_cosim.a
	$(CXX) $(LDFLAGS) -o $@ $^ $(DROMAJO_LIBS) $(EMU_LIBS)

dromajo.o: dromajo.c
	$(CXX) $(CXXFLAGS) -DMAX_XLEN=64 -c -o $@ $<

libdromajo_cosim.a: dromajo_cosim.o riscv_cpu64.o dromajo_main.o \
	riscv_machine.o softfp.o $(EMU_OBJS)
	ar rvs $@ $^

dromajo_cosim_test.o: dromajo_cosim_test.c
	$(CXX) $(CXXFLAGS) -c -o $@ $<

dromajo_cosim.o: dromajo_cosim.c
	$(CXX) $(CXXFLAGS) -DMAX_XLEN=64 -c -o $@ $<

dromajo_main.o: dromajo_main.c
	$(CXX) $(CXXFLAGS) -DCONFIG_CPU_RISCV -c -o $@ $<

riscv_cpu64.o: riscv_cpu.c
	$(CXX) $(CXXFLAGS) -DMAX_XLEN=64 -c -o $@ $<

install: $(PROGS)
	$(INSTALL) -m755 $(PROGS) "$(DESTDIR)$(bindir)"

%.o: %.c
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

debug:
	$(MAKE) CFLAGS_OPT=

build:
	mkdir -p build

clean:
	rm -f *.o *.d *~ $(PROGS)
	rm -rf build

-include $(wildcard *.d)

tags:
	etags *.[hc]

release:
	git archive HEAD | xz -9 > dromajo-$(shell date +%Y%m%d)-$(shell git rev-parse --short HEAD).tar.xz
	git tag release-$(shell date +%Y%m%d)
