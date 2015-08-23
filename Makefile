# This makefile needs a lot of work.

CC=~/opt/cross-i686/bin/i686-elf-gcc

build/kernel/kernel: $(shell find include/kernel) $(shell find src/kernel) $(shell find include/klegit) $(shell find src/klegit)
	# create output directories
	mkdir -p build/kernel
	mkdir -p build/klegit

	# build the "start file" stubs in to which the global constructor code is placed by gcc (I think)
	$(CC) -c -o build/crti.o src/crti.S
	$(CC) -c -o build/crtn.o src/crtn.S

	# assemble early boot handler
	$(CC) -c -o build/kernel/early.o src/kernel/early.S

	# build klegit
	$(CC) -ffreestanding -std=c11 -Wall -Wextra -Werror -c -Iinclude -o build/klegit/string.o src/klegit/string.c
	$(CC) -ffreestanding -std=c11 -Wall -Wextra -Werror -c -Iinclude -o build/klegit/mini-printf.o src/klegit/mini-printf.c 

	# build kernel drivers
	$(CC) -ffreestanding -std=c11 -Wall -Wextra -Werror -c -Iinclude -o build/kernel/intel.o src/kernel/intel.c 
	$(CC) -ffreestanding -std=c11 -Wall -Wextra -Werror -c -Iinclude -o build/kernel/terminal.o src/kernel/terminal.c 

	# build kernel
	$(CC) -ffreestanding -std=c11 -Wall -Wextra -Werror -c -Iinclude -o build/kernel/main.o src/kernel/main.c 

	# link it all together
	$(CC) -ffreestanding -nostdlib -T linker.ld -o build/kernel/kernel -lgcc \
		build/crti.o \
		$(shell $(CC) -print-file-name=crtbegin.o) \
		build/klegit/string.o \
		build/kernel/early.o \
		build/kernel/intel.o \
		build/kernel/mini-printf.o \
		build/kernel/terminal.o \
		build/kernel/main.o \
		$(shell $(CC) -print-file-name=crtend.o) \
		build/crtn.o

# remove all build files
clean: 
	find build -type f -delete

# build an iso for testing with an emulator
build/iso: build/kernel/kernel
	cp build/kernel/kernel iso/boot
	genisoimage -R \
		-b boot/grub/stage2_eltorito \
		-no-emul-boot \
		-boot-load-size 4 \
		-A Kernel \
		-input-charset utf8 \
		-quiet \
		-boot-info-table \
		-o build/iso \
		iso

# run test iso with either qemu or bochs
emu: build/iso
	# qemu example: qemu-system-i386 -monitor stdio -d int,cpu_reset -cdrom build/iso -boot d
	bochs -f bochsrc -q

.PHONY: clean emu
