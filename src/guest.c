/*
 * Wii memory for Kirby Access.
 *
 * gSUKE01_recomp.dll exports staticrecomp_get_module, whose descriptor has
 * dispatch(CPUState *, guest address). The runner calls it for every jump
 * between recompiled chunks, so hooking it once hands us the CPUState and,
 * through it, the host addresses of MEM1 and MEM2.
 */
#include "access.h"
#include <stdio.h>
#include <string.h>
#include "MinHook.h"

/* CPUState as built into this release (CPU ABI 2, 3504 bytes). Only the
 * fields we read are named; offsets are checked against the descriptor. */
#define CS_PC      0x280
#define CS_RAM     3456
#define CS_RAMSZ   3464
#define CS_EXRAM   3488
#define CS_EXRAMSZ 3496
#define CS_SIZE    3504

typedef struct {
    uint32_t abi_version, cpu_abi_version, cpu_state_size;
    char game_id[8];
    uint32_t entry_point;
    int (*dispatch)(void *state, uint32_t address);
} ModuleDesc;
typedef const ModuleDesc *(*get_module_fn)(void);

static int (*o_dispatch)(void *state, uint32_t address);
static uint8_t *volatile s_state;
static uint8_t *volatile s_mem1, *volatile s_mem2;
static volatile uint32_t s_mem1_size, s_mem2_size;
volatile uint32_t g_dispatch_count;

/* Entry hooks.
 *
 * Code is compiled in 16 KB chunks (one host function per chunk, guest
 * 0x80006a00 + n * 0x4000, table found through dispatch). A call into
 * another chunk sets pc and returns to the runner, which enters the target
 * chunk's host function with pc = the callee. So hooking every chunk entry
 * sees every cross-chunk call, with the guest registers as at the callee's
 * first instruction (r3 = this / first argument, lr = return address).
 * Calls inside one chunk are not seen; pick hook points accordingly. */
#define CHUNK_BASE 0x80006a00u
#define NCHUNK 449
typedef void (*chunk_fn)(void *state);
static chunk_fn s_chunk_orig[NCHUNK];
#define NHOOK 256
static volatile uint32_t s_hook_addr[NHOOK];
static guest_hook_fn s_hook_fn[NHOOK];
static volatile LONG s_nhooks;

void guest_on_entry(uint32_t addr, guest_hook_fn fn) {
    LONG i = InterlockedIncrement(&s_nhooks) - 1;
    if (i >= NHOOK) return;
    s_hook_fn[i] = fn;
    s_hook_addr[i] = addr;
}

static volatile uint32_t s_chunk_calls;

/* Dev watch: log registers when chosen guest functions are entered. */
#define NWATCH 8
static volatile uint32_t s_watch_addr[NWATCH];
static volatile LONG s_watch_left[NWATCH];
void guest_watch(uint32_t addr, int count) {
    for (int i = 0; i < NWATCH; i++)
        if (!s_watch_left[i] || s_watch_addr[i] == addr) { s_watch_addr[i] = addr; s_watch_left[i] = count; return; }
}

/* Dev histogram of entered addresses. */
#define HIST 8192
static volatile LONG s_hist_on;
static uint32_t s_hist_addr[HIST], s_hist_n[HIST];
void guest_hist(int on) {
    if (on) { memset(s_hist_n, 0, sizeof s_hist_n); memset(s_hist_addr, 0, sizeof s_hist_addr); s_hist_on = 1; return; }
    s_hist_on = 0; Sleep(50);
    log_line("dispatch calls so far: %u, chunk entries %u", g_dispatch_count, s_chunk_calls);
    for (int i = 0; i < HIST; i++) if (s_hist_n[i]) log_line("hist %08x %u", s_hist_addr[i], s_hist_n[i]);
}

static void h_chunk(void *state) {
    s_chunk_calls++;
    uint8_t *st = (uint8_t *)state;
    uint32_t pc = *(uint32_t *)(st + CS_PC);
    if (!s_state) s_state = st;
    if (s_hist_on) {
        uint32_t h = ((pc >> 2) * 2654435761u >> 19) & (HIST - 1);
        for (int k = 0; k < 32; k++, h = (h + 1) & (HIST - 1)) {
            if (s_hist_addr[h] == pc) { s_hist_n[h]++; break; }
            if (!s_hist_addr[h]) { s_hist_addr[h] = pc; s_hist_n[h] = 1; break; }
        }
    }
    for (int i = 0; i < NWATCH; i++) {
        if (s_watch_left[i] > 0 && s_watch_addr[i] == pc) {
            const uint32_t *g = (const uint32_t *)st;
            InterlockedDecrement(&s_watch_left[i]);
            log_line("hit %08x r3=%08x r4=%08x r5=%08x r6=%08x lr=%08x", pc, g[3], g[4], g[5], g[6], *(uint32_t *)(st + CS_PC + 4));
        }
    }
    LONG n = s_nhooks < NHOOK ? s_nhooks : NHOOK;
    for (LONG i = 0; i < n; i++) if (s_hook_addr[i] == pc && s_hook_fn[i]) s_hook_fn[i]((const uint32_t *)st);
    uint32_t idx = (pc - CHUNK_BASE) >> 14;
    s_chunk_orig[idx < NCHUNK ? idx : 0](state);
}

static int h_dispatch(void *state, uint32_t address) {
    g_dispatch_count++;
    if (!s_state) s_state = (uint8_t *)state;
    return o_dispatch(state, address);
}

/* The chunk table: dispatch loads it with "lea rax, [rip + rel32]" before
 * "call [rax + rdi * 8]". */
static chunk_fn *find_chunk_table(HMODULE m, const uint8_t *code) {
    uint8_t *base = (uint8_t *)m;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    uint8_t *end = base + nt->OptionalHeader.SizeOfImage;
    for (int i = 0; i < 0x100; i++) {
        if (code[i] == 0x48 && code[i + 1] == 0x8d && code[i + 2] == 0x05) {
            chunk_fn *t = (chunk_fn *)(code + i + 7 + *(int32_t *)(code + i + 3));
            if ((uint8_t *)t < base || (uint8_t *)(t + NCHUNK) > end) continue;
            int ok = 1;
            for (int k = 0; k < NCHUNK && ok; k++) ok = (uint8_t *)t[k] > base && (uint8_t *)t[k] < end;
            if (ok) return t;
        }
    }
    return NULL;
}

int guest_init(void) {
    HMODULE m = NULL;
    for (int i = 0; i < 600 && !(m = GetModuleHandleW(L"gSUKE01_recomp.dll")); i++) Sleep(100);
    if (!m) { log_line("gSUKE01_recomp.dll is not loaded"); return 0; }
    get_module_fn gm = (get_module_fn)(void *)GetProcAddress(m, "staticrecomp_get_module");
    if (!gm) { log_line("staticrecomp_get_module missing"); return 0; }
    const ModuleDesc *d = gm();
    log_line("module abi %u cpu abi %u state %u game %.8s", d->abi_version, d->cpu_abi_version, d->cpu_state_size, d->game_id);
    if (d->cpu_abi_version != 2 || d->cpu_state_size != CS_SIZE || memcmp(d->game_id, "SUKE01", 6)) {
        log_line("unsupported port build"); return 0;
    }
    if (MH_Initialize() != MH_OK) return 0;
    if (MH_CreateHook((void *)d->dispatch, (void *)h_dispatch, (void **)&o_dispatch) != MH_OK) { log_line("dispatch hook failed"); return 0; }
    chunk_fn *table = find_chunk_table(m, (const uint8_t *)d->dispatch);
    if (!table) { log_line("chunk table not found; no entry hooks"); }
    else {
        int bad = 0;
        for (int k = 0; k < NCHUNK; k++)
            if (MH_CreateHook((void *)table[k], (void *)h_chunk, (void **)&s_chunk_orig[k]) != MH_OK) bad++;
        log_line("chunk hooks: %d of %d failed", bad, NCHUNK);
        if (bad) return 0;
    }
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) { log_line("enable hooks failed"); return 0; }
    return 1;
}

int guest_ready(void) {
    if (s_mem1) return 1;
    uint8_t *st = s_state;
    if (!st) return 0;
    uint8_t *ram = *(uint8_t **)(st + CS_RAM), *ex = *(uint8_t **)(st + CS_EXRAM);
    uint32_t rs = *(uint32_t *)(st + CS_RAMSZ), es = *(uint32_t *)(st + CS_EXRAMSZ);
    if (!ram || rs < 0x1000000) return 0;
    s_mem1_size = rs; s_mem2_size = ex ? es : 0; s_mem2 = ex; s_mem1 = ram;
    log_line("MEM1 %p size %x, MEM2 %p size %x", ram, rs, ex, es);
    return 1;
}

uint32_t guest_pc(void) { return s_state ? *(uint32_t *)(s_state + CS_PC) : 0; }

const uint8_t *guest_ptr(uint32_t a, uint32_t n) {
    uint32_t base = a & 0x3fffffff;         /* drop cached / uncached mirror bits */
    if (base < 0x10000000) { if (s_mem1 && base + n <= s_mem1_size) return s_mem1 + base; }
    else if (base >= 0x10000000 && s_mem2) { base -= 0x10000000; if (base + n <= s_mem2_size) return s_mem2 + base; }
    return NULL;
}

uint8_t r8(uint32_t a) { const uint8_t *p = guest_ptr(a, 1); return p ? *p : 0; }
uint16_t r16(uint32_t a) { const uint8_t *p = guest_ptr(a, 2); return p ? (uint16_t)(p[0] << 8 | p[1]) : 0; }
uint32_t r32(uint32_t a) {
    const uint8_t *p = guest_ptr(a, 4);
    return p ? (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3] : 0;
}
void guest_w32(uint32_t a, uint32_t v) {
    uint8_t *p = (uint8_t *)guest_ptr(a, 4);
    if (p) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
}
float rf32(uint32_t a) { uint32_t v = r32(a); float f; memcpy(&f, &v, 4); return f; }

int guest_str(uint32_t a, char *out, int cap) {
    int n = 0;
    while (n < cap - 1) { const uint8_t *p = guest_ptr(a + n, 1); if (!p || !*p) break; out[n++] = (char)*p; }
    out[n] = 0;
    return n;
}

int guest_wstr(uint32_t a, wchar_t *out, int cap) {
    int n = 0;
    while (n < cap - 1) { uint16_t c = r16(a + 2 * n); if (!c) break; out[n++] = c; }
    out[n] = 0;
    return n;
}

void guest_dump(const wchar_t *dir) {
    wchar_t p[MAX_PATH];
    struct { const wchar_t *name; uint8_t *mem; uint32_t size; } parts[2] = {
        { L"mem1.bin", s_mem1, s_mem1_size }, { L"mem2.bin", s_mem2, s_mem2_size } };
    for (int i = 0; i < 2; i++) {
        if (!parts[i].mem) continue;
        swprintf(p, MAX_PATH, L"%ls%ls", dir, parts[i].name);
        FILE *f = _wfopen(p, L"wb");
        if (f) { fwrite(parts[i].mem, 1, parts[i].size, f); fclose(f); }
    }
    log_line("dumped memory");
}
