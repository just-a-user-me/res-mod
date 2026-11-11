// Fullbright for Minecraft Bedrock (arm64-v8a). Generated with ReSuite.
//
// Target (found with RES in libminecraftpe.so):
//     float DimensionBrightnessRamp::getBrightnessRampValue(const int index) const
// returns the light level (0..1) for a brightness index 0..15. Hooking it to always return 1.0 makes every block fully lit.
//
// How it loads: this library is put in front of libminecraftpe.so (RES repackager adds it as a dependency) or injected by
// an external injector. Our constructor runs BEFORE the game library finished loading, so the hook is installed from a
// worker thread that waits until it is ready.
#include <dlfcn.h>
#include <pthread.h>
#include <time.h>

#include "res_hook.h"

// ---- settings ----------------------------------------------------------------------------------------------------------
static const char* kGameLib = "libminecraftpe.so";
// Unique byte signature of the function start (RES > Mod tools > Make unique signature). Survives game updates that do not
// change this function.
static const char* kSigBrightness =
    "FD 7B BE A9 F4 4F 01 A9 FD ?? ?? 91 F3 03 01 2A 3F ?? ?? 71 F4 03 00 AA ?? ?? ?? 54 ?? ?? ?? ?? 00 ?? ?? 91";
static const bool kFullbright = true;

// ---- hooks -------------------------------------------------------------------------------------------------------------
// Original function, reachable through the trampoline (only used when fullbright is off).
typedef float (*BrightnessFn)(void* self, int index);
static BrightnessFn gOrigBrightness = nullptr;

// `this` is the first argument, the brightness index the second.
static float hookBrightness(void* self, int index) {
    if (kFullbright) return 1.0f;
    return gOrigBrightness ? gOrigBrightness(self, index) : 1.0f;
}

static void sleepMs(int ms) { struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L}; nanosleep(&ts, nullptr); }

static void* installThread(void*) {
    // wait until the game library is completely loaded (dlopen with RTLD_NOLOAD only succeeds then)
    for (int i = 0; i < 600; ++i) {
        void* h = dlopen(kGameLib, RTLD_NOW | RTLD_NOLOAD);
        if (h) { dlclose(h); break; }
        sleepMs(100);
    }
    sleepMs(500);   // let static initialisers finish
    res::Module m = res::findModule(kGameLib);
    if (!m.ok()) { LOGE("%s not found", kGameLib); return nullptr; }
    LOGI("%s at 0x%lx", kGameLib, static_cast<unsigned long>(m.base));

    const uintptr_t fn = res::scanSignature(m, kSigBrightness);
    if (!fn) { LOGE("brightness signature not found (different game version?)"); return nullptr; }
    LOGI("getBrightnessRampValue at 0x%lx (offset 0x%lx)", static_cast<unsigned long>(fn), static_cast<unsigned long>(fn - m.base));

    if (res::inlineHook(fn, reinterpret_cast<void*>(&hookBrightness), reinterpret_cast<void**>(&gOrigBrightness)))
        LOGI("fullbright hook installed");
    else
        LOGE("could not install the hook");
    return nullptr;
}

__attribute__((constructor)) static void modInit() {
    LOGI("RES mod loaded");
    pthread_t th;
    pthread_create(&th, nullptr, installThread, nullptr);
    pthread_detach(th);
}
