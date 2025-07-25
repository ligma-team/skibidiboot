@echo off

REM build bootloader

nasm -f bin src/boot/bootloader.asm -o build/bootloader.bin
REM nasm -f bin src/kernel/stage2.asm -o stage2.bin
REM i686-elf-gcc -ffreestanding -m16 -c src/kernel/stage2.c -o stage2.o
REM i686-elf-ld -Ttext 0x0600 -o stage2.elf stage2.o
REM i686-elf-objcopy -O binary stage2.elf stage2.bin
REM cat bootloader.bin stage2.bin > boot.img

REM build mkimage

g++ -std=c++17 -o build/skibidiboot_mkimage src/mkimage/main.cpp