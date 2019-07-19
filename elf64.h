//******************************************************************************
// Copyright (C) 2018,2019, Esperanto Technologies Inc.
// The copyright to the computer program(s) herein is the
// property of Esperanto Technologies, Inc. All Rights Reserved.
// The program(s) may be used and/or copied only with
// the written permission of Esperanto Technologies and
// in accordance with the terms and conditions stipulated in the
// agreement/contract under which the program(s) have been supplied.
//------------------------------------------------------------------------------

/*
 * Elf64 utilities
 *
 * Copyright (c) 2018,2019 Esperanto Technology
 */

#ifndef _ELF64_H
#define _ELF64_H 1

#include <elf.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef EM_RISCV
#define EM_RISCV          0xF3 /* Little endian RISC-V, 32- and 64-bit */
#endif

bool elf64_is_riscv64(const char *image, size_t image_size);
bool elf64_find_global(const char *image, size_t image_size,
                       const char *key, uint64_t *value);

uint64_t elf64_get_entrypoint(const char *image);

#endif
