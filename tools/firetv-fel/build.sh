#!/bin/bash
set -euo pipefail
umask 022
: "${KODI_SIGNING_DIR:=$HOME/.config/firetv-signing}"
export KODI_ANDROID_STORE_FILE="$KODI_SIGNING_DIR/kodi-fel.jks"
export KODI_ANDROID_KEY_ALIAS=kodi-fel
export KODI_ANDROID_STORE_PASSWORD="$(cat "$KODI_SIGNING_DIR/kodi-fel.password")"
export KODI_ANDROID_KEY_PASSWORD="$KODI_ANDROID_STORE_PASSWORD"
test -f "$KODI_ANDROID_STORE_FILE"
cd /workspace
export ANDROID_HOME=/workspace/sdk
export GRADLE_USER_HOME=/workspace/gradle
export ANDROID_USER_HOME=/home/ryan/.android
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
jobs=${BUILD_JOBS:-8}
mkdir -p sdk/cmdline-tools downloads tarballs artifacts "$ANDROID_USER_HOME"
if [ ! -x sdk/cmdline-tools/latest/bin/sdkmanager ]; then
    curl --fail --location --retry 3 -o downloads/android-cli.zip \
        https://dl.google.com/android/repository/commandlinetools-linux-16111833_latest.zip
    printf '%s  %s\n' e025545c62a8e64c7559119566a569fb1dec5f60 downloads/android-cli.zip | sha1sum -c -
    unzip -q downloads/android-cli.zip -d sdk/cmdline-tools
    mv sdk/cmdline-tools/cmdline-tools sdk/cmdline-tools/latest
fi
sdk/cmdline-tools/latest/bin/sdkmanager --sdk_root="$ANDROID_HOME" \
    'platforms;android-37.0' 'build-tools;36.0.0' 'ndk;28.2.13676358'
cd /workspace/kodi
python3 tools/firetv-fel/vendor-ppi.py
bash tools/firetv-fel/build-library.sh
cd tools/depends
if [ ! -f Makefile.include ]; then
    ./bootstrap
    ./configure --host=arm-linux-androideabi --with-sdk-path="$ANDROID_HOME" \
        --with-ndk-api=26 --prefix=/workspace/depends --with-tarballs=/workspace/tarballs
fi
make -j"$jobs"
target=/workspace/depends/arm-linux-androideabi-26-debug
cmake_bin=$(find /workspace/depends -path '*/bin/cmake' -type f | head -1)
if [ ! -f /workspace/kodi-build/build.ninja ]; then
make -C target/cmakebuildsys BUILD_DIR=/workspace/kodi-build GEN=Ninja Configuration=Release \
    CMAKE_EXTRA_ARGUMENTS="-DENABLE_ANDROID_DVFELPLAY=ON -DDVFELPLAY_ROOT=/workspace/fel-prefix -DAPP_PACKAGE=org.xbmc.kodi.fel -DAPP_NAME=Kodi -DENABLE_SMBCLIENT=ON"
fi
"$cmake_bin" -S /workspace/kodi -B /workspace/kodi-build \
    -DENABLE_ANDROID_DVFELPLAY=ON -DDVFELPLAY_ROOT=/workspace/fel-prefix \
    -DAPP_PACKAGE=org.xbmc.kodi.fel -DAPP_NAME=Kodi -DENABLE_SMBCLIENT=ON \
    "-DCMAKE_SHARED_LINKER_FLAGS=-L$target/lib -lgnutls -lnettle -lhogweed -lgmp"
"$cmake_bin" --build /workspace/kodi-build --target apk -j"$jobs"
apk=/workspace/artifacts/Kodi-Piers-Cube3-FEL-release.apk
cp /workspace/kodi/kodiapp-armeabi-v7a-release.apk "$apk"
/workspace/sdk/build-tools/36.0.0/apksigner verify --verbose --print-certs "$apk" > "$apk.certificate.txt"
expected_cert=$(openssl x509 -in /workspace/kodi/tools/firetv-fel/release-certificate.pem -outform DER | sha256sum | cut -d ' ' -f1)
grep -q "certificate SHA-256 digest: $expected_cert" "$apk.certificate.txt" || {
    echo "Release certificate does not match the checked-in public certificate" >&2
    exit 1
}
python3 /workspace/kodi/tools/firetv-fel/record-build.py "$apk"
sha256sum "$apk" > "$apk.sha256"
