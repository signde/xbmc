# Kodi FEL maintainer build

The tested build runs on Linux x86-64 (Ubuntu 24.04, Java 17), with Android SDK
37, build-tools 36.0.0, NDK 28.2.13676358, API 26 and `armeabi-v7a`.
This is the NAS builder layout, not a promise of byte-identical APKs across hosts.

## Inputs

- `/workspace/kodi`: the release tag on the `Piers` branch.
- `/workspace/libdvfelplay`: `libdvfelplay/` from the matching release tag in the Gazelle
  [VLC FEL fork](https://github.com/signde/vlc-fel-android).
- `/workspace/libdovi3-arm`: libdovi **3.3.2**, built for
  `armv7-linux-androideabi` using cargo-c and the Android NDK.
- `/workspace/sdk`, `/workspace/depends`, `/workspace/tarballs`: SDK and normal
  [Kodi Android dependencies](README.Android.md).

Use the same reviewed library source as the player. `build-library.sh` forces
an ARM32 static C++ runtime and empty embedded-kernel-module options. Kernel
helpers are supplied separately by the Magisk module. FEL library GPL licensing
and libdovi MIT licensing remain applicable alongside Kodi’s GPL license.

## Build and signing

Run `bash /workspace/kodi/tools/firetv-fel/build.sh` inside the prepared builder.
It builds the FEL library, configures Kodi with FEL and SMB enabled using
`org.xbmc.kodi.fel` and the **Kodi FEL** launcher label, then creates
`/workspace/artifacts/Kodi-Piers-Cube3-FEL-release.apk`.

The script requires a private `kodi-fel.jks` and single-line `kodi-fel.password`
in `$KODI_SIGNING_DIR` (default `$HOME/.config/firetv-signing`), alias `kodi-fel`.
Keep the directory mode 0700 and files 0600. Never commit or distribute them.
Back up both securely; losing the key prevents updates to existing installations.
The public certificate is `tools/firetv-fel/release-certificate.pem`.

The build verifies the APK signature and records the certificate, APK SHA-256,
source manifest, working source patch and added-source archive next to the APK.
The manifest includes the Kodi commit, changed source hashes, FEL library input
hashes, libdovi archive/header hashes, tool versions, and packaged FEL ELF checks.
These sidecars are maintainer provenance, not additional end-user installation files.
Do not publish an APK without its matching source revisions and licenses.

The separate app ID allows installation alongside official Kodi. The build
script never uninstalls a player.

## Adapter tests

```sh
cmake -S xbmc/cores/VideoPlayer/DVDCodecs/Video/test/firetv-fel \
  -B build-fel-tests -DDVFELPLAY_INCLUDE_DIR=/workspace/libdvfelplay/include
cmake --build build-fel-tests
ctest --test-dir build-fel-tests --output-on-failure
```

These exercise timestamp pairing, flush, startup failure and shutdown with a
fake decoder API. Device playback qualification remains separate.
