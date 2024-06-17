# Von-Neumann Pipelined CPU Implementation in C

## Overview

This project involves the creation of a virtual processor using C programming. The processor design simulates the execution of assembly instructions using the Von Neumann architecture. The primary objective was to mimic the functionality of a real CPU, including main memory, CPU registers, and the arithmetic and logic unit (ALU), as well as to implement a parallel execution pipeline for efficiency.

## Team Members

- Ahmed Khedr 
- Marawan Salah
- Mohammad Rageh 
- Omar Farouk 
- Omar Riad 

## Key Objectives

- Simulate a functional processor that can execute assembly instructions written in C.
- Emulate relevant hardware components such as Main Memory, CPU Registers, and ALU.
- Design a parallel execution pipeline to efficiently utilize hardware resources.

## Methodology

### 1. Global Variable Declaration
   We declared global variables for flags to control the program, as well as values and control signals to be forwarded between stages, mirroring actual CPU behavior. These include:

- Registers and Memory: Arrays for memory and registers.
- Control Flags: Flags indicating which stages should run in the current clock cycle.
- Forwarding Structures: Structs to forward necessary values and control flags to other stages without being overwritten.
### 2. Parsing
The program starts by parsing an instruction file, converting each instruction from a string to its integer representation, and filling the instruction memory field with these integers. The parsing method reads from a text file line by line, identifying opcodes and converting fields appropriately.

### 3. Instruction Execution Cycle
Fetch Block: Fetches instructions from memory and updates the Program Counter (PC).
Decode Block: Extracts fields and sets control signals for execution.
Execute Block: Performs arithmetic and logical operations based on the opcode.
Memory Access (MEM) Block: Reads from or writes to memory based on control signals.
Write-back (WB) Block: Writes results back to registers.
### 4. Main Loop
The main loop manages the instruction execution cycle and clock cycles, toggling flags to control which stages run in each cycle. It ensures that the pipeline functions correctly and handles data hazards.

### 5. Pipelining
Fetching is done once every two cycles, while decoding and executing take two cycles each. Memory access and write-back stages each take one cycle. This design ensures that fetching and memory access do not occur in the same clock cycle, improving efficiency.

### 6. Hazards
Control Hazard: Managed by flushing fetched and decoded instructions when a jump instruction is executed.
Data Hazard: Avoided by reversing the order of the instruction execution cycle to ensure values are written back before being read by subsequent instructions.

## Results

Program Iterations
PC Increment Issue: Resolved by introducing a control signal variable, pcSrc, to regulate PC incrementation during the fetch stage.
JEQ Instruction Inconsistency: Addressed by decrementing pcLine value during the execution phase.
Register Usage Error: Corrected logical shift operations to use the shamt variable appropriately.

## References

- David & John. Introduction to Computer Organization and Design. Edition 5. Online Available.
- W3Schools. C Programming Language. Online Available.
