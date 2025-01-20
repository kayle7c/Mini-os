AS= nasm
CC= gcc
LD= ld
LDFLAGS= -m elf_i386 -Ttext 0xc0001500 -e main -o
ASFLAGS= -f elf
OBJS=$(pwd)/init.o $(pwd)/interrupt.o $(pwd)/kernel.o $(pwd)/main.o $(pwd)/print.o timer.o
pwd=./build
##编译汇编文件##
$(pwd)/kernel.o:kernel/kernel.S
	$(AS) $(ASFLAGS) $< -o $@
$(pwd)/print.o:lib/kernel/print.S
	$(AS) $(ASFLAGS) $< -o $@
##链接##
$(pwd)/kernel.bin:$(OBJS)
	$(LD) $(LDFLAGS) $@
build: $(pwd)/kernel.bin
all: build dd