# res-mod

Minecraft Bedrock (arm64-v8a) fullbright native mod.

Hooks `DimensionBrightnessRamp::getBrightnessRampValue` in `libminecraftpe.so` so it always returns `1.0`.
The function was located with ReSuite (named from embedded strings, found by a unique byte signature).

## Build

CI builds the shared library on GitHub with the Android NDK:

- push to `main` / run the **build** workflow manually -> artifact `libresmod-arm64-v8a`
- push a tag like `v1.0.0` -> `libresmod.so` is attached to the release

Local build:

    export ANDROID_NDK=/path/to/ndk     # r25+ recommended
    ./build.sh                          # -> build/arm64-v8a/libresmod.so

## Load (pick one)

1. **Between the app's own libs (DT_NEEDED):** RES > Mod tools > "Add dependency + repackage". It adds
   `libresmod.so` as a dependency of `libminecraftpe.so`, aligns and re-signs the APK. The mod's constructor
   then runs before the game library. Re-signing needs the original app uninstalled first (different signature).
2. **External injector:** any injector that calls `dlopen("libresmod.so")` inside the game process (needs
   root/ptrace or a Frida-style tool). The constructor does the rest.
3. **Debuggable build:** set `wrap.<package>` = `LD_PRELOAD=/path/libresmod.so`.

## Change it

- Another game version: re-make the signature in RES (Mod tools > Signature) and edit `kSigBrightness` in `src/mod.cpp`.
- `kFullbright=false` passes through to the original via the trampoline.
- `res_hook.h` offers findModule / scanSignature / patchBytes / inlineHook for new mods.

## Honest limits

- Online/realm integrity checks may reject modified clients. Use offline worlds.
- The inline hook copies the first 16 bytes as-is; if they contain PC-relative instructions the original
  trampoline would break (use RES "hook check"). The signature's prologue (stp/mov/...) is position independent.
