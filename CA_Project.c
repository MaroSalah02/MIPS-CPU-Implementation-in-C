#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
int memory[2048];
int registers[33]; //  0 -> 32
// char debug[1024];  // debug tool

int instruction; // The instructoin fetched from the instruction memory
// cycle counter for printing
int cycle = 1;
// Our flags that we use to make the code simulate the CPU clock cycles correctly
bool instructionFetched = false; // flags used for when starting the execution, decodes, exectes, MEM and WB don't start from the first cycle, but
bool instructionDecoded = false; // only after the first instruction is fetched and the previous stage is done
bool instructionExecuted = false;
bool memoryAccessed = false;

bool THEFINALCOUNTDOWN = false;

int decodeRemainingCycles = 2; // splitting the decode and execute into 2 halfs (1st half print input, second half actually does logic)
int executeRemainingCycles = 2;

bool fetchingCycle = false; // toggleable flag (toggles each cycle and when jumoing) that tells us if we should fetch or not
bool flushFlag = false;

// Decode Output - Execute parameters
struct decodedValues
{
    // Register Values
    int opcode;
    int R1;
    int R2;
    int R3;
    int shamt;
    int immediate;
    int address;
    // Control signals
    bool RegWrite; // controls whether a result will be written back or not
    bool MemRead;
    bool MemWrite;
    bool MemtoReg;
    // bool RegDst; --> Not needed since all our instruction formats write data to the address of R1, unlike the CPU in the
    //              --> lecture where the R-format and the I-format get the RegWrite address from different parts of the instruction
    // PC Line: A line used to transfer the value of the PC from the ALU to the multiplexer that is controlled by the PCSrc
    bool PCSrc; // a boolean that tells us how we need to modify the value of the PC, triggers when the jumps are done
} decodedValues;

// Access info that is used by the memory access stage - output of the execute phase
struct memAccessInfo
{
    // Values concerning memAccess
    bool MemRead;
    bool MemWrite;
    int memWriteValue; // in the case we are writing to memory

    int executeResult; // result from the ALU, whether it is used for memory access or writeback depends on the control signals

    // Values concerning writeback
    int writeBackAddress;
    bool MemtoReg;
    bool regWrite;
} memAccessInfo;

struct writeBackInfo
{
    bool regWrite;
    int writeBackAddress;
    int writeBackValue;
} writeBackInfo;

int PCLine;

struct loopToPC
{
    bool PCSrc; // a boolean that tells us how we need to modify the value of the PC, triggers when the jumps are done
} loopToPC;

void fetch()
{
    printf("    Fetch Stage Input: PC = %i \n", registers[32]);

    if (registers[32] >= 1023) // edited from 33 to 32
    {
        printf("Instruction Memory Last Instructoin Slot Reached");
        THEFINALCOUNTDOWN = true;
        // return;
    }
    else
    {
        THEFINALCOUNTDOWN = false;
    }

    instruction = memory[registers[32]]; // placing the next instruction // edited from 33 to 32

    if (!loopToPC.PCSrc)
    {
        registers[32] += 1; // edited from 33 to 32
    }
    else
    {
        registers[32] = PCLine;
    }
    loopToPC.PCSrc = false;
    instructionFetched = true;
    // printf("    Fetch Stage: PC AFTER = %i \n", registers[32]);

    if (flushFlag)
    {
        instructionFetched = false;
    }
    if (instruction == -1)
    {
        THEFINALCOUNTDOWN = true;
        instructionFetched = false;
    }
    printf("    Fetch Stage Output: instruction = %i \n", instruction);
}

void decode()
{
    if (decodeRemainingCycles == 1) // The second cycle of the decode insruction
    {
        int opcode = instruction & (0b11110000000000000000000000000000); // 31:28
        opcode = (opcode >> 28) & (0b00000000000000000000000000001111);
        int R1 = instruction & (0b00001111100000000000000000000000); // 27:23
        R1 = R1 >> 23;
        int R2 = instruction & (0b00000000011111000000000000000000); // 22:18
        R2 = R2 >> 18;
        int R3 = instruction & (0b00000000000000111110000000000000); // 17:13
        R3 = R3 >> 13;
        int shamt = instruction & (0b00000000000000000001111111111111);     // 12:0
        int immediate = instruction & (0b00000000000000111111111111111111); // 17:0
        int signBit = (immediate & (0b100000000000000000)) >> 17;           // egtting the sign-extended immediate value
        if (signBit)
        {
            immediate = immediate | (0b11111111111111000000000000000000);
        }
        int address = instruction & (0b00001111111111111111111111111111); // 27:0

        // Assigned Decoded values to global variable
        decodedValues.opcode = opcode;
        decodedValues.R1 = R1;
        decodedValues.R2 = R2;
        decodedValues.R3 = R3;
        decodedValues.shamt = shamt;
        decodedValues.immediate = immediate;
        decodedValues.address = address;

        // Adjusting the control Singals
        decodedValues.PCSrc = false;
        switch (opcode)
        {
        case 0: // R type
        case 1:
        case 2:
        case 5:
        case 8:
        case 9:
            decodedValues.MemtoReg = 0;
            decodedValues.RegWrite = 1;
            decodedValues.MemRead = 0;
            decodedValues.MemWrite = 0;
            break;

        case 3: // MOVI
        case 6: // XORI
            decodedValues.MemtoReg = 0;
            decodedValues.RegWrite = 1;
            decodedValues.MemRead = 0;
            decodedValues.MemWrite = 0;
            break;

        case 4:
        case 7: // JEQ , JMP
            decodedValues.MemtoReg = 0;
            decodedValues.RegWrite = 0;
            decodedValues.MemRead = 0;
            decodedValues.MemWrite = 0;
            decodedValues.PCSrc = true;
            break;

        case 10: // MOVR
            decodedValues.MemtoReg = 1;
            decodedValues.RegWrite = 1;
            decodedValues.MemRead = 1;
            decodedValues.MemWrite = 0;
            break;

        case 11: // MOVM
            decodedValues.MemtoReg = 0;
            decodedValues.RegWrite = 0;
            decodedValues.MemRead = 0;
            decodedValues.MemWrite = 1;
            break;

        default:
            printf("You have entered default case in the decode switch!\n");
            break;
        }

        decodeRemainingCycles = 2;
        instructionDecoded = true; // if the flush flag is set to true, the instructionDecoded flag will
                                   // stay false in order to prevent the unwanted execution of an instruction below the
                                   // jump instruction
        instructionFetched = false;
        printf("    Decode Stage Input: Previous Cycle\n");
        // Decoded values prints
        printf("    Decode Register Outputs:\n");
        printf("        Opcode = %i\n", decodedValues.opcode);
        printf("        R1 = %i\n", decodedValues.R1);
        printf("        R2 = %i\n", decodedValues.R2);
        printf("        R3 = %i\n", decodedValues.R3);
        printf("        SHAMT = %i\n", decodedValues.shamt);
        printf("        IMMEDIATE = %i\n", decodedValues.immediate);
        printf("        ADDRESS = %i\n", decodedValues.address);
        // Control signals prints
        printf("    Decode Control Lines Outputs:\n");
        printf("        MemRead = %i\n", decodedValues.MemRead);
        printf("        MemWrite = %i\n", decodedValues.MemWrite);
        printf("        MemtoReg = %i\n", decodedValues.MemtoReg);
        printf("        RegWrite = %i\n", decodedValues.RegWrite);
        printf("        PCSrc = %i\n", decodedValues.PCSrc);
    }
    else
    {
        printf("    Decode Stage Input: %i\n", instruction);
        printf("    Decode Stage Output: Next Cycle\n");

        decodeRemainingCycles -= 1;
    }
    if (flushFlag)
    {
        instructionDecoded = false;
        decodeRemainingCycles = 2;
    }
}

void execute()
{
    int result;
    if (executeRemainingCycles == 1)
    {

        switch (decodedValues.opcode)
        {
        case 0:
            result = registers[decodedValues.R2] + registers[decodedValues.R3]; // ADD R
            break;
        case 1:
            result = registers[decodedValues.R2] - registers[decodedValues.R3]; // SUB R
            break;
        case 2:
            result = registers[decodedValues.R2] * registers[decodedValues.R3]; // MUL R
            break;
        case 3:
            result = decodedValues.immediate; // Move Immediate (MOVI) I
            break;
        case 4:
            if (registers[decodedValues.R1] == registers[decodedValues.R2])
            { // Jump if equal (JEQ) I
                PCLine = decodedValues.immediate + registers[32] - 1;
                flushFlag = true;
            }
            break;
        case 5:
            result = registers[decodedValues.R2] & registers[decodedValues.R3]; // AND R
            break;
        case 6:
            result = registers[decodedValues.R2] ^ decodedValues.immediate; // XOR (XORI) I
            break;
        case 7:
            PCLine = (registers[32] & (0b111100000000000000000000000000)) + decodedValues.address; // Jump (JMP) J
            flushFlag = true;
            break;
        case 8:
            result = registers[decodedValues.R2] << decodedValues.shamt; // LSL R
            break;
        case 9:
            result = registers[decodedValues.R2] >> decodedValues.shamt; // LSR R
            break;
        case 10:
        case 11:
            if (registers[decodedValues.R2] + decodedValues.immediate < 1024)
            {
                printf("Attempted to access an address outside of memory bounds\n");

                // return;
            }; // Move to regsiter (MOVR) I , Move to Memory (MOVM) I
            result = registers[decodedValues.R2] + decodedValues.immediate;
            break;
        default:
            printf("Invalid OPCODE detected\n");
        }

        // Forwarding the flags and values needed for the writeback and memorcy access operations

        memAccessInfo.MemRead = decodedValues.MemRead;
        memAccessInfo.MemWrite = decodedValues.MemWrite;
        memAccessInfo.memWriteValue = registers[decodedValues.R1];

        memAccessInfo.executeResult = result;

        memAccessInfo.MemtoReg = decodedValues.MemtoReg;
        memAccessInfo.regWrite = decodedValues.RegWrite;
        memAccessInfo.writeBackAddress = decodedValues.R1;

        executeRemainingCycles = 2;
        instructionExecuted = true;
        instructionDecoded = false;
        printf("    Execute Stage Input: Previous Cycle \n");
        printf("    Values Forwarded To MEM \n");
        printf("        MemRead = %i\n", memAccessInfo.MemRead);
        printf("        MemWrite = %i\n", memAccessInfo.MemWrite);
        printf("        memWriteValue = %i\n", memAccessInfo.memWriteValue);
        printf("        ALUOutput = %i\n", memAccessInfo.executeResult);
        printf("        MemtoReg = %i\n", memAccessInfo.MemtoReg);
        printf("        regWrite = %i\n", memAccessInfo.regWrite);
        printf("        writeBackAddress = %i\n", memAccessInfo.writeBackAddress);
        printf("    Values Forwarded Back to PC \n");
        printf("        PCSrc = %i\n", decodedValues.PCSrc);
        printf("        PCLine = %i\n", PCLine);
        loopToPC.PCSrc = decodedValues.PCSrc;
    }
    else
    {
        printf("    Execute Stage Input: \n");
        // Decoded values prints
        printf("        Opcode = %i\n", decodedValues.opcode);
        printf("        R1 = %i\n", decodedValues.R1);
        printf("        R2 = %i\n", decodedValues.R2);
        printf("        R3 = %i\n", decodedValues.R3);
        printf("        SHAMT = %i\n", decodedValues.shamt);
        printf("        IMMEDIATE = %i\n", decodedValues.immediate);
        printf("        ADDRESS = %i\n", decodedValues.address);
        printf("    Control Flags Forwarded from Decode Stage: \n");
        printf("        MemRead = %i\n", decodedValues.MemRead);
        printf("        MemWrite = %i\n", decodedValues.MemWrite);
        printf("        MemtoReg = %i\n", decodedValues.MemtoReg);
        printf("        regWrite = %i\n", decodedValues.RegWrite);
        printf("        writeBackAddress = %i\n", decodedValues.R1);
        executeRemainingCycles -= 1;
    }
}

void memoryAccess()
{
    printf("    memoryAccess Inputs:\n");
    printf("        ALU Ouptut = %i\n", memAccessInfo.executeResult);
    printf("        MemtoReg = %i\n", memAccessInfo.MemtoReg);
    printf("        memRead = %i\n", memAccessInfo.MemRead);
    printf("        memWrite = %i\n", memAccessInfo.MemWrite);
    printf("        memWriteValue = %i\n", memAccessInfo.memWriteValue);

    // Forward info into the writeback stage
    writeBackInfo.regWrite = memAccessInfo.regWrite;
    writeBackInfo.writeBackAddress = memAccessInfo.writeBackAddress;

    if (memAccessInfo.MemRead)
    {
        if (memAccessInfo.MemtoReg)
        {
            writeBackInfo.writeBackValue = memory[memAccessInfo.executeResult]; // The value written back to the register is from the memory access
            printf("    memoryAccess Output: Value (%i) Read from Address (%i)\n", writeBackInfo.writeBackValue, memAccessInfo.executeResult);
        }
        else
        {
            printf("MemoryRead Without Memory To Register, there is an issue, trace\n");
        }
    }
    else
    {
        if (memAccessInfo.MemWrite)
        {
            memory[memAccessInfo.executeResult] = memAccessInfo.memWriteValue;
            printf("    memoryAccess Output: Value (%i) Written to Address (%i)\n", memAccessInfo.memWriteValue, memAccessInfo.executeResult);
        }
        else
        {
            writeBackInfo.writeBackValue = memAccessInfo.executeResult;
        }
    }
    printf("    Values Forwarded to WriteBack:\n");
    printf("        regWrite = %i\n", writeBackInfo.regWrite);
    printf("        writeBackAddress = %i\n", writeBackInfo.writeBackAddress);
    printf("        writeBackValue = %i\n", writeBackInfo.writeBackValue);

    memoryAccessed = true;
    instructionExecuted = false;
}

void writeBack()
{
    printf("    WriteBack Input:\n");
    printf("        RegWrite = %i\n", writeBackInfo.regWrite);
    printf("        writeBackAddress = %i\n", writeBackInfo.writeBackAddress);
    printf("        writeBackValue = %i\n", writeBackInfo.writeBackValue);
    if (writeBackInfo.regWrite && writeBackInfo.writeBackAddress != 0)
    {
        registers[writeBackInfo.writeBackAddress] = writeBackInfo.writeBackValue;
        printf("    WriteBack Output: Value (%i) Written To Target Register (%i)\n", writeBackInfo.writeBackValue, writeBackInfo.writeBackAddress);
    }
    else
    {
        printf("    WriteBack Output: None\n");
    }
    memoryAccessed = false;
}

void parse(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        printf("Error opening file %s\n", filename);
        return;
    }
    char line[256];
    int instruction_index = 0;
    char type = '.';
    while (fgets(line, sizeof(line), file))
    {
        line[strcspn(line, "\n")] = '\0';
        char *token = strtok(line, " ");
        char *instruction[4];
        int token_count = 0;
        while (token != NULL && token_count < 4)
        {
            instruction[token_count] = strdup(token);
            token_count++;
            token = strtok(NULL, " ");
        }
        int32_t value_of_inst;
        if (strcmp(instruction[0], "ADD") == 0)
        {
            value_of_inst = 0b0000; // 0

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[1]); // R3

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[2]); // R1

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[3]); // R2

            value_of_inst <<= 13; // 13 0S FOR SHAMT
            type = 'R';
        }
        else if (strcmp(instruction[0], "SUB") == 0)
        {
            value_of_inst = 0b0001; // 1

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[1]); // R1

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[2]); // R2

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[3]); // R3

            value_of_inst <<= 13; // 13 0S FOR SHAMT
            type = 'R';
        }
        else if (strcmp(instruction[0], "MUL") == 0)
        {
            value_of_inst = 0b0010; // 2

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[1]); // R1

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[2]); // R2

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[3]); // R3

            value_of_inst <<= 13; // 13 0S FOR SHAMT
            type = 'R';
        }
        else if (strcmp(instruction[0], "AND") == 0)
        {
            value_of_inst = 0b0101; // 5

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[1]); // R1

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[2]); // R2

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[3]); // R3

            value_of_inst <<= 13; // 13 0S FOR SHAMT
            type = 'R';
        }
        else if (strcmp(instruction[0], "LSL") == 0)
        {
            value_of_inst = 0b1000; // 8

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[1]); // R1

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[2]); // R2

            value_of_inst <<= 5; // R3 0S

            value_of_inst <<= 13;
            int32_t shamt = atoi(instruction[3]);
            uint32_t maskedNumber;
            if (shamt < 0)
            {
                maskedNumber = (uint32_t)(shamt + (1 << 13)); // Convert to 13-bit two's complement
            }
            else
            {
                maskedNumber = (uint32_t)shamt;
            }
            value_of_inst |= maskedNumber; // SHAMT
            type = 'R';
        }
        else if (strcmp(instruction[0], "LSR") == 0)
        {
            value_of_inst = 0b1001; // 9

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[1]);

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[2]);

            value_of_inst <<= 5; // R3 0S

            value_of_inst <<= 13; // SHAMT
            int32_t shamt = atoi(instruction[3]);
            uint32_t maskedNumber;
            if (shamt < 0)
            {
                maskedNumber = (uint32_t)(shamt + (1 << 13)); // Convert to 13-bit two's complement
            }
            else
            {
                maskedNumber = (uint32_t)shamt;
            }
            value_of_inst |= maskedNumber;
            type = 'R';
        }
        else if (strcmp(instruction[0], "JMP") == 0)
        {
            value_of_inst = 0b0111; // 7

            value_of_inst <<= 28; // ADDRESS
            int32_t address = atoi(instruction[1]);
            value_of_inst |= address;

            type = 'J';
        }
        else if (strcmp(instruction[0], "MOVI") == 0)
        {
            value_of_inst = 0b0011; // 3

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[1]); // R1

            value_of_inst <<= 5; // R2 0S

            value_of_inst <<= 18; // IMM
            int32_t imm = atoi(instruction[2]); // atoi takes as input a string and return the integer inside it
            uint32_t maskedNumber;
            if (imm < 0)
            {
                maskedNumber = (uint32_t)(imm + (1 << 18)); // Convert to 18-bit two's complement
            }
            else
            {
                maskedNumber = (uint32_t)imm;
            }
            value_of_inst |= maskedNumber;
            type = 'I';
        }
        else if (strcmp(instruction[0], "JEQ") == 0)
        {
            value_of_inst = 0b0100; // 4

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[1]); // R1

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[2]); // R2

            value_of_inst <<= 18; // IMM
            int32_t imm = atoi(instruction[3]);
            uint32_t maskedNumber;
            if (imm < 0)
            {
                maskedNumber = (uint32_t)(imm + (1 << 18)); // Convert to 18-bit two's complement
            }
            else
            {
                maskedNumber = (uint32_t)imm;
            }
            value_of_inst |= maskedNumber;
            type = 'I';
        }
        else if (strcmp(instruction[0], "XORI") == 0)
        {
            value_of_inst = 0b0110; // 6

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[1]); // R1

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[2]); // R2

            value_of_inst <<= 18; // IMM
            int32_t imm = atoi(instruction[3]);
            uint32_t maskedNumber;
            if (imm < 0)
            {
                maskedNumber = (uint32_t)(imm + (1 << 18)); // Convert to 18-bit two's complement
            }
            else
            {
                maskedNumber = (uint32_t)imm;
            }
            value_of_inst |= maskedNumber;
            type = 'I';
        }
        else if (strcmp(instruction[0], "MOVR") == 0)
        {
            value_of_inst = 0b1010; // 10

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[1]); // R1

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[2]); // R2

            value_of_inst <<= 18; // IMM
            int32_t imm = atoi(instruction[3]);
            uint32_t maskedNumber;
            if (imm < 0)
            {
                maskedNumber = (uint32_t)(imm + (1 << 18)); // Convert to 18-bit two's complement
            }
            else
            {
                maskedNumber = (uint32_t)imm;
            }
            value_of_inst |= maskedNumber;
            type = 'I';
        }
        else if (strcmp(instruction[0], "MOVM") == 0)
        {
            value_of_inst = 0b1011; // 10

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[1]); // R1

            value_of_inst <<= 5;
            value_of_inst |= get_register_binary(instruction[2]); // R2

            value_of_inst <<= 18; // IMM
            int32_t imm = atoi(instruction[3]);
            uint32_t maskedNumber;
            if (imm < 0)
            {
                maskedNumber = (uint32_t)(imm + (1 << 18)); // Convert to 18-bit two's complement
            }
            else
            {
                maskedNumber = (uint32_t)imm;
            }
            value_of_inst |= maskedNumber;
            type = 'I';
        }
        // we will loop here to put the instructions in array of instructions
        memory[instruction_index] = value_of_inst;
        // debug[instruction_index] = type;
        instruction_index++;
    }

    if (instruction_index < 1023)
    {
        memory[instruction_index] = (0b11111111111111111111111111111111);
    }
    fclose(file);
}
int32_t get_register_binary(char *register_name)
{
    if (strcmp(register_name, "R0") == 0)
    {
        return 0b00000;
    }
    else if (strcmp(register_name, "R1") == 0)
    {
        return 0b00001;
    }
    else if (strcmp(register_name, "R2") == 0)
    {
        return 0b00010;
    }
    else if (strcmp(register_name, "R3") == 0)
    {
        return 0b00011;
    }
    else if (strcmp(register_name, "R4") == 0)
    {
        return 0b00100;
    }
    else if (strcmp(register_name, "R5") == 0)
    {
        return 0b00101;
    }
    else if (strcmp(register_name, "R6") == 0)
    {
        return 0b00110;
    }
    else if (strcmp(register_name, "R7") == 0)
    {
        return 0b00111;
    }
    else if (strcmp(register_name, "R8") == 0)
    {
        return 0b01000;
    }
    else if (strcmp(register_name, "R9") == 0)
    {
        return 0b01001;
    }
    else if (strcmp(register_name, "R10") == 0)
    {
        return 0b01010;
    }
    else if (strcmp(register_name, "R11") == 0)
    {
        return 0b01011;
    }
    else if (strcmp(register_name, "R12") == 0)
    {
        return 0b01100;
    }
    else if (strcmp(register_name, "R13") == 0)
    {
        return 0b01101;
    }
    else if (strcmp(register_name, "R14") == 0)
    {
        return 0b01110;
    }
    else if (strcmp(register_name, "R15") == 0)
    {
        return 0b01111;
    }
    else if (strcmp(register_name, "R16") == 0)
    {
        return 0b10000;
    }
    else if (strcmp(register_name, "R17") == 0)
    {
        return 0b10001;
    }
    else if (strcmp(register_name, "R18") == 0)
    {
        return 0b10010;
    }
    else if (strcmp(register_name, "R19") == 0)
    {
        return 0b10011;
    }
    else if (strcmp(register_name, "R20") == 0)
    {
        return 0b10100;
    }
    else if (strcmp(register_name, "R21") == 0)
    {
        return 0b10101;
    }
    else if (strcmp(register_name, "R22") == 0)
    {
        return 0b10110;
    }
    else if (strcmp(register_name, "R23") == 0)
    {
        return 0b10111;
    }
    else if (strcmp(register_name, "R24") == 0)
    {
        return 0b11000;
    }
    else if (strcmp(register_name, "R25") == 0)
    {
        return 0b11001;
    }
    else if (strcmp(register_name, "R26") == 0)
    {
        return 0b11010;
    }
    else if (strcmp(register_name, "R27") == 0)
    {
        return 0b11011;
    }
    else if (strcmp(register_name, "R28") == 0)
    {
        return 0b11100;
    }
    else if (strcmp(register_name, "R29") == 0)
    {
        return 0b11101;
    }
    else if (strcmp(register_name, "R30") == 0)
    {
        return 0b11110;
    }
    else if (strcmp(register_name, "R31") == 0)
    {
        return 0b11111;
    }
    else
    {
        return -1;
    }
}
// void printBinary32(int n, int debug_index)
// {
//     int groupr[] = {4, 5, 5, 5, 13};
//     int groupi[] = {4, 5, 5, 18};
//     int groupj[] = {4, 28};
//     int groupIndexr = 0;
//     int groupIndexi = 0;
//     int groupIndexj = 0;
//     int bitsPrinted = 0;
//     for (int i = 31; i >= 0; i--)
//     {
//         int bit = (n >> i) & 1;
//         printf("%d", bit);
//         bitsPrinted++;

//         if (debug[debug_index] == 'R')
//         {
//             if (bitsPrinted == groupr[groupIndexr])
//             {
//                 printf(" ");
//                 bitsPrinted = 0;
//                 groupIndexr++;
//             }
//         }
//         else if (debug[debug_index] == 'I')
//         {
//             if (bitsPrinted == groupi[groupIndexi])
//             {
//                 printf(" ");
//                 bitsPrinted = 0;
//                 groupIndexi++;
//             }
//         }
//         else if(bitsPrinted == groupj[groupIndexj])
//         {
//             printf(" ");
//             bitsPrinted = 0;
//             groupIndexj++;
//         }
//     }
//     printf("\n");
// }

void registerPrint()
{
    int size = sizeof(registers) / sizeof(registers[0]); // Calculate the size of the array
    printf("Elements of the array of Registers:\n");
    int start;
    int end;
    for (int i = 0; i < size - 1; i++)
    {

        if (registers[i] == 0)
        {
            start = i;
            for (; i < size - 1 && registers[i] == 0; i++)
            {
                // do nothing, just increment is
            }
            end = i - 1;

            if (start == end)
            {
                printf("        Register R%i = 0\n", start);
            }
            else
            {
                printf("        Registers R%i to R%i = 0\n", start, end);
            }
            i--;
        }
        else
        {
            printf("        Register R%i = %i\n", i, registers[i]);
        }
    }
    printf("        Register R32 (PC) = %i\n", registers[32]);
}

void memoryPrint()
{
    printf("Elements of the Memory:\n");
    printf("    Instruction Memory:\n");
    int start;
    int end;
    int size = 1024;
    for (int i = 0; i < size; i++)
    {

        if (memory[i] == 0)
        {
            start = i;
            for (; i < size && memory[i] == 0; i++)
            {
                // do nothing, just increment is
            }
            end = i - 1;

            if (start == end)
            {
                printf("        Address Number %i = 0\n", start);
            }
            else
            {
                printf("        Addresses %i to %i = 0\n", start, end);
            }
            i--;
        }
        else
        {
            printf("        Address Number %i = %i\n", i, memory[i]);
        }
    }
    printf("    Data Memory:\n");
    size = 2048;
    for (int i = 1024; i < size; i++)
    {

        if (memory[i] == 0)
        {
            start = i;
            for (; i < size && memory[i] == 0; i++)
            {
                // do nothing, just increment is
            }
            end = i - 1;

            if (start == end)
            {
                printf("        Address Number %i = 0\n", start);
            }
            else
            {
                printf("        Addresses %i to %i = 0\n", start, end);
            }
            i--;
        }
        else
        {
            printf("        Address Number %i = %i\n", i, memory[i]);
        }
    }
}

int main()
{
    parse("instructions.txt");
    // void printArray(int arr[]) // this method was built for debugging purposes
    // {
    //     for (int i = 0; i < 5; i++)
    //     {
    //         printBinary32(arr[i], i);
    //     }
    // }
    // printArray(memory);

    char tmp = ' ';
    while (!THEFINALCOUNTDOWN || memoryAccessed || instructionExecuted || instructionDecoded || instructionFetched)
    {
        // At the start of a cycle, toggle the fetching cycle flag
        fetchingCycle = !fetchingCycle;
        // printf("%i\n", (fetchingCycle & !false));

        // Print the contents of the registers & memory of the previous cycle, if there is a previous cycle
        if (cycle == 1)
        {
            printf("EXECUTION START\n");
        }
        {
            printf(">>>>>>>>>>>>>>>>\n");
            // Memory and Regsiter Prints for the previous cycle
            registerPrint();
            memoryPrint();
            printf("<<<<<<<<<<<<<<<<\n");
        }
        printf("------------------------------\n");
        printf("Cycle: %i \n", cycle);
        // printf("THE FINAL COUNTDOWN: %i\n", THEFINALCOUNTDOWN);
        // printf("instructionFetched: %i\n", instructionFetched);
        // printf("instructionDecoded: %i\n", instructionDecoded);
        // printf("instructionExecuted: %i\n", instructionExecuted);
        // printf("memoryAccessed: %i\n", memoryAccessed);
        // printf("flushFlag: %i\n", flushFlag);

        // printf("FlushFlag is: %i \n", flushFlag);
        if (memoryAccessed)
        {
            writeBack();
        }

        if (instructionExecuted)
        {
            memoryAccess();
        }

        if (instructionDecoded)
        {
            execute();
        }
        if (instructionFetched)
        {
            decode();
        }

        if (fetchingCycle && !THEFINALCOUNTDOWN)
        {
            fetch();
        }

        if (cycle % 10 == 0)             // this if condition checks if 10 cycles have passed or not in order to pause the code
        {                                // this is done due to the fact that the terminal may not be able to accomodate all the
            printf("Continue? [Y/n]: "); // print statements required by the project description
            scanf("%c", &tmp);
            if (tmp == 'n' || tmp == 'N')
            {
                printf("program Terminated\n");
                return 0;
            }
            getchar();
        }

        cycle++;

        if (flushFlag && memoryAccessed)
        {
            flushFlag = false;
            THEFINALCOUNTDOWN = false;
        }
    }

    {
        printf(">>>>>>>>>>>>>>>>\n");
        // Memory and Regsiter Prints for the previous cycle
        registerPrint();
        memoryPrint();
        printf("<<<<<<<<<<<<<<<<\n");
    }

    // printf("Data Memory Address: %i\n", memory[1024]);
}
