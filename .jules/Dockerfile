FROM invent-registry.kde.org/sysadmin/ci-images/suse-qt610:latest

ENV TZ=Etc/UTC

# Switch to root to install packages if necessary, though ci-images usually run as root or allow sudo.
USER root

# In a SUSE image, use zypper. However, the KDE CI images often already contain the required tools.
# Let's ensure cmake and gcc are present just in case.
RUN zypper install -y \
    cmake \
    gcc-c++ \
    make \
    kf6-kwallet-devel

WORKDIR /app
COPY . /app

# Ensure correct permissions
RUN chown -R root:root /app

RUN cmake -B build -DBUILD_TESTING=ON && cmake --build build -j$(nproc)

CMD ["/app/build/KMagMux"]
