FROM docker.io/archlinux:latest

ENV DEBIAN_FRONTEND=noninteractive

RUN pacman -Syu --noconfirm \
    base-devel \
    cmake \
    git \
    qt6-base \
    qt6-svg \
    qt6-tools \
    qtkeychain-qt6 \
    kxmlgui \
    kconfig \
    kconfigwidgets \
    kcoreaddons \
    ki18n \
    xorg-server-xvfb \
    imagemagick \
    xorg-xwd

WORKDIR /app
COPY . /app

RUN cmake -B build -S . && cmake --build build -j$(nproc)

CMD ["/bin/bash"]
