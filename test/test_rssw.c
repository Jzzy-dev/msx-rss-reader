// test_rssw.c - runs RSSW.ROM's INIT and H.STKE routine on a fake MSX up to the
// moment it enters RSS (rom_start), then checks: pages 1 and 2 are RAM, RAM
// 4000h.. holds the RSS image byte for byte, H.STKE+1 = the RAM slot (RSS's own
// INIT found it), USRTAB, and VRAM held the packed data.  Then lets RSS "end"
// (a RET at rom_start) and checks that the routine restores pages 1 and 2 and
// returns to the H.STKE caller with an empty BASIC program.
// build: gcc -O2 -o test_rssw test_rssw.c z80.c -I<dir of z80.h> (github.com/superzazu/z80)
// usage: test_rssw RSSW.ROM rss_img.bin rom_start_hex [msx1]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "z80.h"

#define HZ 3579545UL
static z80 cpu;
static uint8_t mem[65536];          // page 0 and 3, and the RAM slot's pages 1-2
static uint8_t rom[0x4000];          // the cartridge, page 2 of 1-2
static uint8_t vram[0x20000]; static uint32_t va; static uint8_t vlat, vfirst, r14; static int msx1;
static uint8_t a8;                   // primary slot register
static uint8_t p_slot[4];            // slot id per page
#define S_BASIC 0x00
#define S_CART  (0x80 | (2 << 2) | 1)   // 1-2
#define S_RAM   (0x80 | (2 << 2) | 3)   // 3-2
static int writes_rom;

static uint8_t rd(void *u, uint16_t a)
{
    (void)u; int pg = a >> 14;
    if (pg == 1) return p_slot[1] == S_RAM ? mem[a] : 0xFF;
    if (pg == 2) return p_slot[2] == S_RAM ? mem[a] : p_slot[2] == S_CART ? rom[a - 0x8000] : 0xFF;
    return mem[a];
}
static void wr(void *u, uint16_t a, uint8_t v)
{
    (void)u; int pg = a >> 14;
    if ((pg == 1 || pg == 2) && p_slot[pg] != S_RAM) { writes_rom++; return; }
    if (pg == 0) return;
    mem[a] = v;
}
static uint8_t pin(z80 *z, uint8_t p)
{
    (void)z;
    if (p == 0xA8) return a8;
    if (p == 0x98) { uint8_t v = vram[va & (msx1 ? 0x3FFF : 0x1FFFF)]; va++; vfirst = 0; return v; }
    if (p == 0x99) { vfirst = 0; return 0; }
    return 0xFF;
}
static void pout(z80 *z, uint8_t p, uint8_t v)
{
    (void)z;
    if (p == 0x98) { vram[va & (msx1 ? 0x3FFF : 0x1FFFF)] = v; va++; vfirst = 0; return; }
    if (p == 0x99) {
        if (!vfirst) { vlat = v; vfirst = 1; return; }
        vfirst = 0;
        if (v & 0x80) { if ((v & 0x3F) == 14) { if (msx1) printf("[R#14 written on an MSX1!]\n"); r14 = vlat & 7; } return; }
        va = (uint32_t)r14 << 14 | (uint32_t)(v & 0x3F) << 8 | vlat;
    }
}
static void ret(void) { cpu.pc = mem[cpu.sp] | (mem[cpu.sp + 1] << 8); cpu.sp += 2; }
static void set_slot(int page, uint8_t id)
{
    p_slot[page] = id;
    a8 = (uint8_t)((a8 & ~(3 << (page * 2))) | ((id & 3) << (page * 2)));
    if (id & 0x80) { uint8_t *t = &mem[0xFCC5 + (id & 3)]; *t = (uint8_t)((*t & ~(3 << (page * 2))) | (((id >> 2) & 3) << (page * 2))); }
}
static unsigned long run_to(uint16_t stop, unsigned long max, uint16_t rom_start)
{
    unsigned long c0 = cpu.cyc;
    while (cpu.cyc - c0 < max)
    {
        uint16_t pc = cpu.pc;
        if (pc == stop) return 0;
        if (pc == rom_start && (p_slot[2] == S_RAM)) return 1;
        if (pc == 0x0024) { set_slot(cpu.h >> 6, cpu.a); cpu.iff1 = cpu.iff2 = 0; ret(); continue; }   // ENASLT
        if (pc == 0x0041) { ret(); continue; }                                                     // DISSCR
        if (pc == 0x00A2) { putchar(cpu.a); ret(); continue; }                                     // CHPUT
        if (pc < 0x4000 && pc != 0x0000) { printf("[unexpected BIOS call %04X]\n", pc); return 2; }
        if (pc == 0x0000) { printf("[jump to 0]\n"); return 2; }
        z80_step(&cpu);
    }
    printf("[time out at %04X]\n", cpu.pc); return 2;
}

int main(int argc, char **argv)
{
    if (argc < 4) return 2;
    msx1 = argc > 4;
    FILE *f = fopen(argv[1], "rb"); fread(rom, 1, 0x4000, f); fclose(f);
    static uint8_t img[0x8000]; f = fopen(argv[2], "rb"); size_t n = fread(img, 1, sizeof img, f); fclose(f);
    uint16_t rom_start = (uint16_t)strtol(argv[3], 0, 16);
    memset(mem + 0x4000, 0x55, 0x8000);          // BASIC's RAM in pages 1-2 (old contents)
    mem[0x002D] = msx1 ? 0 : 2;                   // MSXVER
    mem[0xFCC1] = 0x00; mem[0xFCC2] = 0x80; mem[0xFCC3] = 0x00; mem[0xFCC4] = 0x80;   // EXPTBL
    for (int i = 0xFD9A; i < 0xFFCA; i++) mem[i] = 0xC9;
    mem[0xFC4A] = 0xAC; mem[0xFC4B] = 0xE4;     // HIMEM E4AC (as on the real Pico+, Disk BASIC 2)
    mem[0xF39A] = 0x5A; mem[0xF39B] = 0x47;     // USRTAB = FCERR
    set_slot(0, S_BASIC); set_slot(1, S_BASIC); set_slot(2, S_CART); set_slot(3, S_RAM);
    z80_init(&cpu); cpu.read_byte = rd; cpu.write_byte = wr; cpu.port_in = pin; cpu.port_out = pout;
    // INIT (as the BIOS calls it: page 2 = the cartridge)
    cpu.sp = 0xF000; mem[0xF000] = 0x00; mem[0xF001] = 0xF8;
    cpu.pc = rom[2] | rom[3] << 8;
    if (run_to(0xF800, HZ, rom_start)) { printf("FAIL INIT\n"); return 1; }
    printf("INIT: H.STKE %02X %02X %02X%02X %02X, USRTAB %02X%02X\n", mem[0xFEDA], mem[0xFEDB], mem[0xFEDD], mem[0xFEDC], mem[0xFEDE], mem[0xF39B], mem[0xF39A]);
    int ok = mem[0xFEDA] == 0xF7 && mem[0xFEDB] == S_CART && (mem[0xF39A] | mem[0xF39B] << 8) != 0x475A;
    printf("%s INIT hooks H.STKE with slot 1-2 and changes USRTAB\n", ok ? "OK  " : "FAIL");
    // H.STKE: CALLF to our slot (page 2 already ours), from a stack in page 3 like BASIC's
    set_slot(1, S_BASIC); set_slot(2, S_CART);
    cpu.sp = 0xE300; mem[0xE300] = 0x00; mem[0xE301] = 0xF9;
    cpu.pc = mem[0xFEDC] | mem[0xFEDD] << 8;
    int r = (int)run_to(0xF900, HZ * 20, rom_start);
    if (r != 1) { printf("FAIL did not reach rom_start (%d)\n", r); return 1; }
    printf("at rom_start %.2f s: page 1 %02X page 2 %02X, H.STKE+1 %02X, USRTAB %02X%02X\n", cpu.cyc / (double)HZ, p_slot[1], p_slot[2], mem[0xFEDB], mem[0xF39B], mem[0xF39A]);
    int same = !memcmp(&mem[0x4000], img, n);
    if (!same) { for (size_t i = 0; i < n; i++) if (mem[0x4000 + i] != img[i]) { printf("first difference at %04zX: %02X, image %02X\n", 0x4000 + i, mem[0x4000 + i], img[i]); break; } }
    printf("%s RSS image unpacked to 4000h, %zu bytes identical\n", same ? "OK  " : "FAIL", n);
    printf("%s pages 1 and 2 are the RAM slot\n", p_slot[1] == S_RAM && p_slot[2] == S_RAM ? "OK  " : "FAIL");
    printf("%s RSS's INIT found the RAM as its slot (H.STKE+1 = %02X)\n", mem[0xFEDB] == S_RAM ? "OK  " : "FAIL", mem[0xFEDB]);
    printf("%s no writes to ROM pages\n", writes_rom ? "FAIL" : "OK  ");
    // RSS ends at once: RET at rom_start
    ret();
    r = (int)run_to(0xF900, HZ, 0xFFFF);
    printf("%s back at the H.STKE caller, page 1 %02X page 2 %02X, SP %04X\n", r == 0 && p_slot[1] == S_BASIC && p_slot[2] == S_CART && cpu.sp == 0xE302 ? "OK  " : "FAIL", p_slot[1], p_slot[2], cpu.sp);
    printf("%s empty BASIC program (8000h-8002h = %02X %02X %02X)\n", !mem[0x8000] && !mem[0x8001] && !mem[0x8002] ? "OK  " : "FAIL", mem[0x8000], mem[0x8001], mem[0x8002]);
    return 0;
}
