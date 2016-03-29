History
=======

This project has been ongoing since 2003 or so. The source has been lost and rewritten several times since then :)

    16:22 <TomRyder> I'd just like to interject for a moment. What you're referring to as "Vidic" is 
                     in fact TNU/Vidic, or as I've recently taken to calling it, TNU plus Vidic.

Source directories
==================

* `kernel` containing the kernel proper
* `klegit` contains the minimal libc for the kernel (may be removed / merged with legit)
* `legit` contains the source to Legit C, our special libc
* `iso` is the basic filesystem used to build the test ISO
* `sysroot` is where the header files etc are placed for gcc to find

Build environment
=================

For now, an i686-elf cross gcc and binutils are required to build the kernel. These instructions assume your host system is Debian Jessie, and that the tools will be installed to `~/opt/cross-i686`.

Common
------

    export CROSS_PREFIX="${HOME}/opt/cross-i686"
    mkdir -p "${CROSS_PREFIX}"

binutils
--------

    apt-get source binutils
    cd binutils-2.25

    mkdir build
    cd build/
    ../configure --target=i686-elf --prefix="${CROSS_PREFIX}" --with-sysroot --disable-nls --disable-werror
    make
    make install

    export PATH="${CROSS_PREFIX}/bin:$PATH"

gcc
---

    aptitude install libgmp3-dev libmpfr-dev libmpc-dev 
    apt-get source gcc-4.9
    cd gcc-4.9-4.9.2

    tar xfv gcc-4.9.2-dfsg.tar.xz
    cd gcc-4.9.2
    patch -p2 < ../debian/patches/gcc-gfdl-build.diff # disable building some of the docs that don't work on debian

    mkdir build
    cd build/
    ../configure --target=i686-elf --prefix=$CROSS_PREFIX --enable-languages="c,c++" --disable-nls --without-headers 
    make all-gcc
    make all-target-libgcc
    make install-gcc
    make install-target-libgcc

Building
========

The supplied makefile assumes you have the build environment described above installed in `~/opt/cross-i686`.

The `emu` target of the makefile can launch either `qemu` or `bochs` for testing. A `bochsrc` is included.

Startup sequence
================

1. GRUB loads kernel to 0x100000
1. _start from early.S runs
1. ESP is set to the top of the 32k early boot stack
1. Compiler's global constructors are called
1. main() from kernel/main.c runs
1. Terminal is cleared and a message is printed
1. Set up a flat mapping of the whole physical address space in the GDT
1. Set up an ISR for interrupt 0x80 and call it
1. CPU halts
