#!/bin/sh
# Invoked inside the local-CI ARM64 container, with /work as the checkout.
set -eu
cd /work
jobs="${WINE_NX_JOBS:-4}"
build_type="${AUTORUN_BUILD_TYPE:-Release}"
case "$build_type" in Debug|Release) ;; *) echo "Unsupported build type: $build_type" >&2; exit 1 ;; esac
profile_repo="${AUTORUN_PROFILE_REPOSITORY-PalmMuse/autorun-cn}"
profile_tag="${AUTORUN_PROFILE_TAG:-}"
if [ "$build_type" = Debug ] && [ -z "$profile_tag" ]; then profile_tag=profile-debug; fi
runtime_tag=
if [ "$build_type" = Debug ]; then runtime_tag="$profile_tag"; fi
profile_url=
if [ -n "$profile_repo" ]; then
    if [ -n "$profile_tag" ]; then
        profile_url="https://cnb.cool/$profile_repo/-/releases/download/$profile_tag/autorun-profiles.tsv"
    else
        profile_url="https://cnb.cool/$profile_repo/-/releases/latest/download/autorun-profiles.tsv"
    fi
fi
probe=/work/wine-nx-probe
out="$probe/build-local-ci"
pe="$out/pe"
runtime="$out/runtime"
mesa="$probe/build-mesa-switch/install/opt/devkitpro/portlibs/switch/lib"
mkdir -p "$pe" "$runtime"
# This setting exists only in this disposable container. Mounted macOS/Linux
# checkouts and their vendor submodules can have a different owner from root.
git config --global --add safe.directory '*'
sh "$probe/tools/bootstrap-box64-core.sh"
sh "$probe/tools/bootstrap-libusbhsfs.sh"
sh "$probe/tools/bootstrap-lsfg-vk.sh"
(
    cd "$pe"
    /work/configure --enable-archs=aarch64,arm64ec,i386 \
        --enable-winebox64=aarch64 --enable-winebox64ec=arm64ec \
        --disable-tests --without-x --without-freetype --without-alsa --without-pulse \
        --without-dbus --without-fontconfig --without-udev --without-usb \
        --without-gstreamer --without-vulkan
    make -j"$jobs" include/all
)
cmake -S "$probe" -B "$runtime" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$probe/cmake/switch-devkitA64.cmake" \
    -DWINE_NX_PE_BUILD_DIR="$pe" -DWINE_NX_AMD64=ON \
    -DWINE_NX_BOX64_INTERPRETER=ON -DWINE_NX_BOX64_DYNAREC=ON \
    -DWINE_NX_MESA_SWITCH_DIR="$mesa" -DWINE_NX_USB_STORAGE=ON \
    -DWINE_NX_LSFG=ON -DCMAKE_BUILD_TYPE="$build_type" \
    -DAUTORUN_PROFILE_INDEX_URL_OVERRIDE="$profile_url" \
    -DAUTORUN_RUNTIME_RELEASE_TAG_OVERRIDE="$runtime_tag"
cmake --build "$runtime" --target wine-nx-runtime-nro -j "$jobs"
python3 "$probe/tools/build-dxvk.py" --build "$out/dxvk-amd64" --jobs "$jobs"
python3 "$probe/tools/build-dxvk.py" --arch x86 --build "$out/dxvk-x86" --jobs "$jobs"
python3 "$probe/tools/build-vkd3d.py" --build "$out/vkd3d" --jobs "$jobs"
python3 "$probe/tests/check_dxvk_requirements.py"
python3 "$probe/tools/package-amd64.py" --pe "$pe" --build "$runtime" --jobs "$jobs" \
    --vulkan --dxvk "$out/dxvk-amd64/payload" --vkd3d "$out/vkd3d/payload"
# With no override, let the packager use the same default as the NRO.
# An explicitly empty variable still requests the offline profile catalog.
set --
if [ "${AUTORUN_PROFILE_REPOSITORY+x}" = x ]; then
    set -- --profile-repository "$AUTORUN_PROFILE_REPOSITORY"
fi
python3 "$probe/tools/package-autorun.py" --no-example-games \
    --amd64 "$runtime/wine-nx-amd64-box64-mesa-dxvk-vkd3d.zip" \
    --x86-dxvk "$out/dxvk-x86/payload" \
    "$@" \
    --profile-release-tag "$profile_tag"
