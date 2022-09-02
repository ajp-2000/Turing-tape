# Turing-tape
 tape.c — a command-line C program to simulate a Turing machine roughly as Turing proposed, with a device reading and writing from an indefinitely long length of tape. The tape consists in a series of binary digits which are read one at a time, and the machine is always in one of a finite number of internal states. The final component is a set of instructions built into the machine, which relate each pair of internal state and currently-read binary digit to a trio of operations; an internal state to change to, a binary digit to write to the current position, and a direction to move along the tape, with an optional STOP command.
 
 # Mechanics
 The machine starts at position 0 on the tape, with internal state 0. At every step, the present internal state and bit being read are printed, by default to stdout, along with the instruction to be executed. When the machine reaches STOP, the program exits.
 
 Text files representing a length of tape and an instruction set respectively must be given as command-line paramaters. Any changes made to the tape will be saved to the file; this won't necessarily all be at the STOP command, because the program only reads one buffer of tape at a time, and writes all changes to that buffer once a new section of tape is needed. The BUFFER_SIZE is 128 by default, which is much smaller than modern computers demand, but low enough to demonstrate the principle of a buffer within the small scale on which we are working. The number of possible internal states is capped at 128 (as the internal state is represented by a non-negative signed byte), and the number of instructions is capped accordingly. Equally, one instruction for every possible combination of internal state and bit currently read.

# Example Instruction Sets
 increment.txt - increments the first number found to the right of the zero-position by one, in unary notation. (From Penrose's The Emperor's New Mind)
 
 back_increment.txt - increments one to the first number found to the right of the zero-position (since there are only zeroes to the left), but places the new 1 to the left of this number. This demonstrates the program's ability to generate indefinitely manny zeroes to the left, and shift the rest of the tape right accordingly.
 
 infinite.txt - changes nothing on the tape, and moves right indefinitely. Demonstrates zero-generation to the right, and the program's safety check against instruction tables with no STOP command.
 
 euclid.txt - carries out Euclid's algorithm for finding the highest common divisor of two positive integers, each in unary form, separated by one zero. (From Penrose)
