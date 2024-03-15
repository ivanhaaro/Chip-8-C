package main

import (
	"fmt"
	"os"
)

const MEMSIZE = 4096

type Chip8 struct {
	mem [4096]uint8
	pc  uint16

	v      [16]uint8
	i      uint16
	st, dt uint16

	stack [16]uint16
	sp    uint8
}

func initialize(machine *Chip8) {
	machine.pc = 0x200
	machine.i = 0x0
	machine.st = 0x0
	machine.dt = 0x0
	machine.sp = 0x0
}

func load_rom(machine *Chip8) {
	data, err := os.ReadFile("../roms/TETRIS")
	if err != nil {
		panic(err)
	}
	copy(machine.mem[0x200:], data)
}

func main() {

	var mac Chip8
	initialize(&mac)
	load_rom(&mac)

	// fmt.Println(mac.pc)
	// fmt.Println(mac.mem)

	// Infinite loop
	for {

		opcode := (uint16(mac.mem[mac.pc]) << 8) | uint16(mac.mem[mac.pc+1])

		if mac.pc+2 >= MEMSIZE {
			mac.pc = 0
		}

		fmt.Printf("UNO -> %b%b\n", mac.mem[mac.pc], mac.mem[mac.pc+1])
		fmt.Printf("DOS -> %b\n", opcode)
	}
}
