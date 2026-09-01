#pragma once

#include "picoincludes.h"

static const int PICO_ROM_MASK= 0xf01fffff;	//2MB of flash
static const int PICO_SRAM_MASK=0xf003ffff;	//256K of ram

static const int PICO_XIP_MASK= 0x0003ffff;
static const int PICO_APB_MASK= 0x0003ffff;

static const int PICO_ROM=0x00000000;
static const int PICO_XIP=0x10000000;
static const int PICO_SRAM=0x20000000;
static const int PICO_APB=0x40000000;

static const int PICO_NVM = XIP_BASE + 0x100000;

static const int PICO_PAGE = FLASH_PAGE_SIZE;
static const int PICO_SECTOR = FLASH_SECTOR_SIZE;

static void flash_erase(uint32_t offset){
	flash_range_erase(offset, PICO_SECTOR);
}

static void flash_memory(uint32_t offset, const uint8_t *data, int pagecount){
	flash_range_program(offset, data, FLASH_PAGE_SIZE * pagecount);
}
