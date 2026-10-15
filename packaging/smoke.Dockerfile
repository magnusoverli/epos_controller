FROM ubuntu:26.04
RUN apt-get update && apt-get install -y --no-install-recommends \
    libstdc++6 libudev1 libarchive13t64 ca-certificates libssl3t64 libgl1 libopengl0 libegl1 libfontconfig1 libxkbcommon0 \
    libdbus-1-3 libglib2.0-0t64 libgssapi-krb5-2 libxcb-cursor0 libxkbcommon-x11-0 \
    libsm6 libice6 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
    libxcb-render-util0 libxcb-shape0 libxcb-xfixes0 libxcb-sync1 \
    libxcb-randr0 libxcb-xinerama0 libxcb-xkb1 libxcb-glx0 libx11-xcb1 \
    libwayland-client0 libwayland-cursor0 libwayland-egl1 \
    && rm -rf /var/lib/apt/lists/*
ENV QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software LANG=C.UTF-8 HOME=/tmp XDG_CACHE_HOME=/tmp/.cache
USER 65534:65534
ENTRYPOINT ["/opt/headset/bin/headset-controller"]
