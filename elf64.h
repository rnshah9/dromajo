/*
 * Elf64 utilities
 *
 * Copyright (c) 2018 Esperanto Technology
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

#endif
