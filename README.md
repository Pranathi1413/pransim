# pransim
Instructions to use this simulator:
1. The program asks for the assembly file that you would like to run. Make sure the file is in the same directory as the .cpp file.
2. You can run the whole code or step by step.
3. The registers are numbered from r0 to r31. Notations like $s0 are NOT allowed.
4. Any data that you want to store in memory must be put under the .data section only. The name of the chunk of data (arr in input.asm) is NOT followed by a colon(:)
5. The assembly code must be written under .text section only. 
6. Having a 'main:' label is compulsory. Execution begins from this line only.
7. Instructions are numbered from 0 from the line containing the main label.
8. Keywords allowed are: add, sub, slt, addi, sll, lw, sw, bne, beq, j, la, li
They all have the usual meaning (like mips). sll can take negative immediate values, which would mean srl of the absolute value.
9. Only single line comments are allowed (starting with '#')

Note1: Kindly make sure you have the necessary header files and libraries that have been included in the code (like the boost library)

Note2: input.asm contains example code for bubble sort
