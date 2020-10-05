export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
export CFLAGS="-O2 -pipe -g -feliminate-unused-debug-types"
export LDFLAGS="-Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed"
export CXXFLAGS="-O2 -pipe -g -feliminate-unused-debug-types"
export CPP="aarch64-linux-gnu-gcc -E --sysroot=/home/builduser/FS"
export CC="aarch64-linux-gnu-gcc --sysroot=/home/builduser/FS"
export CXX="aarch64-linux-gnu-g++ --sysroot=/home/builduser/FS"
export LD="aarch64-linux-gnu-ld --sysroot=/home/builduser/FS"
export CONFIGURE_FLAGS="--target=aarch64-linux-gnu --host=aarch64-linux-gnu --build=x86_64-linux --with-libtool-sysroot=/home/builduser/FS"
