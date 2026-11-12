#!/usr/bin/env bash
# Builds a current, supported OpenSSL (3.5 LTS) for Android as libssl_3.so / libcrypto_3.so, the file names Qt loads.
# Replaces the end-of-life 3.1.8 libraries in 3rdparty/android_openssl/ssl_3/<abi>/ (SECURITY.md, finding F-2).
#
# Needs Linux / WSL / macOS with curl, perl, make and the Android NDK (the project uses r27c = 27.2.12479018):
#     export ANDROID_NDK_ROOT=$HOME/Android/Sdk/ndk/27.2.12479018
#     3rdparty/build_openssl_android.sh            # -> 3rdparty/openssl-out/ssl_3/{arm64-v8a,x86_64}/lib{ssl,crypto}_3.so
#     3rdparty/build_openssl_android.sh --prepare-only   # only download + verify + patch (no NDK needed)
# Or run the GitHub Action "openssl-android" (.github/workflows/openssl-android.yml) and download the artifact.
#
# Same method as KDAB/android_openssl (https://github.com/KDAB/android_openssl): the shared libraries get the suffix _3 and
# unversioned symbols; 16 KB page size for Android 15+ (arm64, x86_64); minimum Android API 23.
set -euo pipefail

OPENSSL_VERSION="3.5.9"
OPENSSL_SHA256="603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a"   # tarball from the GitHub release
ANDROID_API=23
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${OUT:-$HERE/openssl-out}"
WORK="${WORK:-$HERE/openssl-build}"
PREPARE_ONLY=0; [ "${1:-}" = "--prepare-only" ] && PREPARE_ONLY=1

mkdir -p "$WORK" "$OUT"
TARBALL="$WORK/openssl-$OPENSSL_VERSION.tar.gz"
if [ ! -f "$TARBALL" ]; then
    curl -fsSL -o "$TARBALL" "https://github.com/openssl/openssl/releases/download/openssl-$OPENSSL_VERSION/openssl-$OPENSSL_VERSION.tar.gz"
fi
echo "$OPENSSL_SHA256  $TARBALL" | sha256sum -c - || { echo "Checksum mismatch: not building"; exit 1; }

# KDAB's patch: suffix _3 on the library names and no version tags in the symbol map.
PATCH="$WORK/ssl_3.patch"
cat > "$PATCH" <<'EOF'
--- Configurations/15-android.conf
+++ Configurations/15-android.conf
@@ -192,6 +192,7 @@
         bin_lflags       => "-pie",
         enable           => [ ],
         shared_extension => ".so",
+        shlib_variant => "_3",
     },
     "android-arm" => {
         ################################################################
--- util/mkdef.pl
+++ util/mkdef.pl
@@ -258,14 +258,14 @@
             print <<"_____";
 }${prevversion_s};
 _____
-            $prevversion_s = " OPENSSL${SO_VARIANT}_$thisversion";
+            $prevversion_s = " OPENSSL_$thisversion";
             $thisversion = '';  # Trigger start of next section
         }
         unless ($thisversion) {
             $indent = 0;
             $thisversion = $_->version();
             $currversion_s = '';
-            $currversion_s = "OPENSSL${SO_VARIANT}_$thisversion "
+            $currversion_s = "OPENSSL_$thisversion "
                 if $thisversion ne '*';
             print <<"_____";
 ${currversion_s}{
EOF

build_abi() {   # $1 = OpenSSL target (arm64 | x86_64), $2 = Android ABI folder
    local target="$1" abi="$2"
    local src="$WORK/src-$abi"
    rm -rf "$src"; mkdir -p "$src"; tar xzf "$TARBALL" -C "$src" --strip-components=1
    ( cd "$src"
      patch -p0 < "$PATCH"
      if [ "$PREPARE_ONLY" = 1 ]; then echo "prepared $abi (checksum ok, patch applies)"; exit 0; fi
      : "${ANDROID_NDK_ROOT:?set ANDROID_NDK_ROOT to the NDK folder}"
      export ANDROID_NDK_HOME="$ANDROID_NDK_ROOT"
      local host; host="$(ls "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt")"
      export PATH="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$host/bin:$PATH"
      ./Configure shared "android-$target" -U__ANDROID_API__ -D__ANDROID_API__=$ANDROID_API \
          -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384
      make depend
      make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)" SHLIB_VERSION_NUMBER= build_libs
      llvm-strip --strip-all libcrypto_3.so libssl_3.so
      mkdir -p "$OUT/ssl_3/$abi"
      cp libcrypto_3.so libssl_3.so "$OUT/ssl_3/$abi/"
    )
}

build_abi arm64  arm64-v8a
build_abi x86_64 x86_64

if [ "$PREPARE_ONLY" = 0 ]; then
    echo; echo "Built OpenSSL $OPENSSL_VERSION:"; ls -l "$OUT"/ssl_3/*/
    for f in "$OUT"/ssl_3/*/libssl_3.so; do readelf -d "$f" | grep -E "SONAME|NEEDED" | sed "s#^#$f: #"; done
    echo "Next: copy ssl_3/arm64-v8a/*.so (and x86_64 for the emulator) into 3rdparty/android_openssl/ssl_3/<abi>/,"
    echo "rebuild, then test on the phone: a translation lookup, Tatoeba examples, a link import."
fi
