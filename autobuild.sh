#!/bin/bash

i686-elf-gcc -c ./src/kernel.c -o ./bin/kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra 
i686-elf-gcc -T linker.ld -o ./bin/foxos.bin -ffreestanding -O2 -nostdlib ./bin/boot.o ./bin/kernel.o -lgcc
cp ./bin/foxos.bin foxiso/boot/ 
i686-elf-grub-mkrescue -o foxos.iso foxiso 
qemu-system-i386 -cdrom foxos.iso