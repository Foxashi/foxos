#!/bin/bash

# Create bin directory if it doesn't exist
mkdir -p bin

# Compile all C files with correct include paths
i686-elf-gcc -c ./src/kernel.c -o ./bin/kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra -I./src/include
i686-elf-gcc -c ./src/fs.c -o ./bin/fs.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra -I./src/include
i686-elf-gcc -c ./src/io.c -o ./bin/io.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra -I./src/include
i686-elf-gcc -c ./src/keyboard.c -o ./bin/keyboard.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra -I./src/include
i686-elf-gcc -c ./src/shell.c -o ./bin/shell.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra -I./src/include
i686-elf-gcc -c ./src/string.c -o ./bin/string.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra -I./src/include
i686-elf-gcc -c ./src/vga.c -o ./bin/vga.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra -I./src/include

# Compile boot assembly (if exists)

# Link all objects
i686-elf-gcc -T linker.ld -o ./bin/foxos.bin -ffreestanding -O2 -nostdlib ./bin/*.o -lgcc

# Prepare ISO
cp ./bin/foxos.bin foxiso/boot/ 
i686-elf-grub-mkrescue -o foxos.iso foxiso 

echo "BOOTING UP FOXOS"
qemu-system-i386 -cdrom foxos.iso