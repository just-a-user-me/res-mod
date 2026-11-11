// res_hook.h : tiny dependency-free helpers for native game mods (ARM64 / x86-64 Android), generated with ReSuite.
//   * findModule()   : base address and executable ranges of a loaded library
//   * scanSignature(): "FD 7B ?? A9" byte patterns (as made by RES > Mod tools > Signature)
//   * patchBytes()   : write bytes into code
//   * inlineHook()   : redirect a function, optionally keeping a way to call the original
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <elf.h>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>
#include <android/log.h>

#define MOD_TAG "RESMod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, MOD_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, MOD_TAG, __VA_ARGS__)

#if defined(__aarch64__) || defined(RES_FORCE_A64)
#define RES_A64 1
#endif

namespace res {

struct Range { uintptr_t begin, end; };
struct Module { uintptr_t base = 0; std::vector<Range> exec; bool ok() const { return base != 0; } };

inline Module findModule(const char* nameSubstr) {
    struct Ctx { const char* n; Module m; } c{nameSubstr, {}};
    dl_iterate_phdr([](dl_phdr_info* i, size_t, void* p) -> int {
        Ctx* c = static_cast<Ctx*>(p);
        if (!i->dlpi_name || !strstr(i->dlpi_name, c->n)) return 0;
        c->m.base = i->dlpi_addr;
        for (int k = 0; k < i->dlpi_phnum; ++k) {
            const ElfW(Phdr)& ph = i->dlpi_phdr[k];
            if (ph.p_type == PT_LOAD && (ph.p_flags & PF_X)) c->m.exec.push_back({i->dlpi_addr + ph.p_vaddr, i->dlpi_addr + ph.p_vaddr + ph.p_memsz});
        }
        return 1;
    }, &c);
    return c.m;
}

// pattern: hex bytes separated by spaces, "??" = any byte
inline uintptr_t scanSignature(const Module& m, const char* pattern, int wantIndex = 0) {
    std::vector<int> pat;                                   // -1 = wildcard
    for (const char* s = pattern; *s;) {
        if (*s == ' ') { ++s; continue; }
        if (*s == '?') { pat.push_back(-1); while (*s == '?') ++s; continue; }
        pat.push_back(static_cast<int>(strtol(std::string(s, 2).c_str(), nullptr, 16)));
        s += 2;
    }
    if (pat.empty()) return 0;
    int seen = 0;
    for (const Range& r : m.exec) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(r.begin);
        const size_t n = r.end - r.begin;
        for (size_t i = 0; i + pat.size() <= n; ++i) {
            size_t k = 0;
            while (k < pat.size() && (pat[k] < 0 || p[i + k] == pat[k])) ++k;
            if (k == pat.size() && seen++ == wantIndex) return r.begin + i;
        }
    }
    return 0;
}

inline bool makeWritable(void* addr, size_t len) {
    const uintptr_t pg = reinterpret_cast<uintptr_t>(addr) & ~static_cast<uintptr_t>(4095);
    const size_t span = ((reinterpret_cast<uintptr_t>(addr) + len + 4095) & ~static_cast<uintptr_t>(4095)) - pg;
    return mprotect(reinterpret_cast<void*>(pg), span, PROT_READ | PROT_WRITE | PROT_EXEC) == 0;
}

inline void flushCache(void* p, size_t n) { __builtin___clear_cache(static_cast<char*>(p), static_cast<char*>(p) + n); }

inline bool patchBytes(uintptr_t addr, const void* bytes, size_t len) {
    if (!makeWritable(reinterpret_cast<void*>(addr), len)) return false;
    memcpy(reinterpret_cast<void*>(addr), bytes, len);
    flushCache(reinterpret_cast<void*>(addr), len);
    return true;
}

// Redirect `target` to `replacement`. If `original` is given, it receives a trampoline that runs the replaced
// instructions and continues after them. The first bytes of the target must not be PC-relative (RES: "Can this function
// be hooked inline?" checks that); otherwise pass original == nullptr and only replace.
inline bool inlineHook(uintptr_t target, void* replacement, void** original = nullptr) {
    uint8_t* t = reinterpret_cast<uint8_t*>(target);
#ifdef RES_A64
    constexpr size_t kStolen = 16;                          // ldr x17, #8 ; br x17 ; .quad replacement
    const uint32_t jmp[2] = {0x58000051u, 0xD61F0220u};
    if (original) {
        uint8_t* tramp = static_cast<uint8_t*>(mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (tramp == MAP_FAILED) return false;
        memcpy(tramp, t, kStolen);
        memcpy(tramp + kStolen, jmp, 8);
        const uint64_t back = reinterpret_cast<uint64_t>(t + kStolen);
        memcpy(tramp + kStolen + 8, &back, 8);
        flushCache(tramp, kStolen + 16);
        *original = tramp;
    }
    uint8_t patch[16];
    memcpy(patch, jmp, 8);
    const uint64_t dst = reinterpret_cast<uint64_t>(replacement);
    memcpy(patch + 8, &dst, 8);
    return patchBytes(target, patch, sizeof patch);
#else
    constexpr size_t kStolen = 14;                          // jmp [rip+0] ; .quad replacement
    const uint8_t j[6] = {0xFF, 0x25, 0, 0, 0, 0};
    if (original) {
        uint8_t* tramp = static_cast<uint8_t*>(mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (tramp == MAP_FAILED) return false;
        memcpy(tramp, t, kStolen);
        memcpy(tramp + kStolen, j, 6);
        const uint64_t back = reinterpret_cast<uint64_t>(t + kStolen);
        memcpy(tramp + kStolen + 6, &back, 8);
        *original = tramp;
    }
    uint8_t patch[14];
    memcpy(patch, j, 6);
    const uint64_t dst = reinterpret_cast<uint64_t>(replacement);
    memcpy(patch + 6, &dst, 8);
    return patchBytes(target, patch, sizeof patch);
#endif
}

}  // namespace res
