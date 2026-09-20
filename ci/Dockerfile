# Local CI. Build context is ci/, never the game/workspace directory.
FROM ubuntu:24.04@sha256:224a1869083a311ef3f13648a154ba79832fbef6364d31493642ca03082da254 AS checks
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates git python3 gcc libc6-dev pkg-config libminizip-dev \
    libcurl4-openssl-dev libssl-dev libsdl2-dev libsdl2-ttf-dev \
    && rm -rf /var/lib/apt/lists/*
# Keep the existing test toolchain layer reusable when adding cover validation.
RUN apt-get update && apt-get install -y --no-install-recommends libpng-dev \
    && rm -rf /var/lib/apt/lists/*

FROM devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282 AS runtime
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates curl git python3 python3-pip python3-venv build-essential \
    clang flex bison cmake ninja-build pkg-config glslang-tools xz-utils \
    libminizip-dev libcurl4-openssl-dev libssl-dev libsdl2-dev libsdl2-ttf-dev libpng-dev \
    && rm -rf /var/lib/apt/lists/*
RUN python3 -m venv /opt/autorun-python \
    && /opt/autorun-python/bin/pip install --no-cache-dir meson==1.10.2 mako==1.3.10
# Same LLVM-MinGW release as the original macOS build, Linux ARM64 host.
RUN curl -fL --retry 3 -o /tmp/llvm.tar.xz \
    https://github.com/mstorsjo/llvm-mingw/releases/download/20260505/llvm-mingw-20260505-ucrt-ubuntu-22.04-aarch64.tar.xz \
    && echo '9fc82e4dc6598387dd3aafb642f41678f81bcd385c152a96dc74a973e7219d97  /tmp/llvm.tar.xz' | sha256sum -c - \
    && mkdir /opt/llvm-mingw \
    && tar -xJf /tmp/llvm.tar.xz -C /opt/llvm-mingw --strip-components=1 \
    && rm /tmp/llvm.tar.xz
ENV WINE_NX_LLVM_MINGW=/opt/llvm-mingw
ENV PATH="/opt/autorun-python/bin:/opt/llvm-mingw/bin:${PATH}"
# Fail image preparation early if a Switch dependency has disappeared.
RUN test -f /opt/devkitpro/portlibs/switch/lib/libminizip.a \
    && test -f /opt/devkitpro/portlibs/switch/lib/libzstd.a
