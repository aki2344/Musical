# Build

$ cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=./toolchain-aarch64.cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/ --debug-find-pkg=SDL2

$ cmake --build build -j

---
# Environment construction

## WSL install (Windows)
wsl --install Ubuntu

## arm64
$ sudo apt-get update

$ sudo dpkg --add-architecture arm64

$ sudo tee /etc/apt/sources.list.d/ubuntu-ports-arm64.list >/dev/null <<'EOF'

deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports noble main universe multiverse restricted

deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports noble-updates main universe multiverse restricted

deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports noble-backports main universe multiverse restricted

deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports noble-security main universe multiverse restricted

EOF

$ sudo vi /etc/apt/sources.list.d/ubuntu.sources

```
Types: deb
URIs: http://archive.ubuntu.com/ubuntu/
Suites: noble noble-updates noble-backports
Architectures: amd64
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

## Ubuntu security updates. Aside from URIs and Suites,
## this should mirror your choices in the previous section.
Types: deb
URIs: http://security.ubuntu.com/ubuntu/
Suites: noble-security
Architectures: amd64
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
```
## Install Tools and Modules

$ sudo apt-get install -y git cmake ninja-build pkg-config gcc-aarch64-linux-gnu g++-aarch64-linux-gnu binutils-aarch64-linux-gnu

$ sudo apt-get install -y libfreetype6-dev:arm64 libharfbuzz-dev:arm64

---
# Change the project

Modify the project name and source file list in CMakeLists.txt

The startup shell script only changes the project name.
