FROM archlinux:latest

RUN pacman --sync --sysupgrade --refresh --refresh --noconfirm \
    pkgconf \
    clang \
    meson \
    wayland wayland-protocols \
    gtest \
    fontconfig \
    libxkbcommon \
    libavif \
    libexif \
    libheif \
    libjpeg-turbo \
    libjxl \
    librsvg \
    libtiff \
    libwebp \
    openexr

RUN useradd --create-home builder

ENV USER=builder
ENV HOME=/home/builder

ENV CC="clang"
ENV CXX="clang++"
ENV CFLAGS="-g -fsanitize=address"
ENV CXXFLAGS="-g -fsanitize=address"
ENV LDFLAGS="-fsanitize=address"

USER builder
