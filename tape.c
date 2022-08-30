/* tape.c — a command-line C program to simulate a Turing machine roughly as Turing proposed, with a 
 * device reading and writing from an indefinitely long length of tape. The tape consists in a series 
 * of binary digits which are read one at a time, and the machine is always in one of a finite 
 * number of internal states. The final component is a set of instructions built into the machine, 
 * which relate each pair of internal state and currently-read binary digit to a trio of 
 * operations; an internal state to change to, a binary digit to write to the current position, even 
 * if these are the same as the current internal state and the digit presently in that position; 
 * and a direction to move along the tape. The direction is either R, to move one digit to the right, 
 *, or L, and in addition it may command STOP to stop the machine.
 *
 * The machine starts at position 0 on the tape. At every step, the present internal state and bit
 * being read are printed, by default to stdout, along with the instruction to be executed. When the
 * machine reaches STOP, the program exits.
 *
 * Text files representing a length of tape and an instruction set respectively must be given
 * as command-line paramaters. Any changes made to the tape will be saved to the file; this won't 
 * necessarily all be at the STOP command, because the program only reads one buffer of tape at a 
 * time, and writes all changes to that buffer once a new section of tape is needed. The BUFFER_SIZE 
 * is 1024 by default, which is much smaller than modern computers demand, but low enough to 
 * demonstrate the principle of a buffer within the small scale on which we are working. The number 
 * of possible internal states is capped at 128 (as the internal state is represented by a non-negative)
 * signed byte), and the number of instructions is capped accordingly. Equally, one instruction for
 * every possible combination of internal state and bit currently read.
 *
 * Further details in README.md
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>

#include "tape.h"

// The instruction list is captured by a two-dimensional array, structured as [state][bit], of 
// operations to carry out. Each operation is a struct of a new state to enter, a digit to write,
// a bool representing left (0) or right (1), and a bool for stoppping
struct op{
	char state;
	bool val;
	bool dir;
	bool stop;
};

struct op (*instructions)[2];
char max_states;

// Print a formatted string to the log, i.e. either to stdout or do nothing
void logprint(char *fmt, ...){
	if (LOG_STREAM == stdout){
		va_list args;
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
}

// Print the command-line usage text
void print_usg(){
	printf("USAGE: ./tape.c [INSTRUCTION SET] [TAPE] [OPTIONS]\n\n");
	printf("Options:\n\n\t-p\t\tprint each operation in turn\n\n");
}

// Interpret char *instruc and add to the list
int parse_instruc(char *instruc){
	// The position in instructions to store the operation (when we get to it)
	int instate;
	int indigit;

	// The shortest a syntactically correct instruction can be is 11
	if (strlen(instruc) < 10) return 1;

	// The 1-3 characters before the first comma represent a state
	int i;
	char state[3];

	for (i=0; i<3 && instruc[i]!=','; i++)
		state[i] = instruc[i];
	if (instruc[i] != ',')
		return 1;

	instate = atoi(state);
	if (instate >= max_states)
		return 1;
	i++;

	// Then we have a 1 or a 0, followed immediately by ->
	if (instruc[i] == '0' || instruc[i] == '1'){
		indigit = instruc[i] - '0';

		if (instruc[i+1] != '-' || instruc[i+2] != '>')
			return 1;
	} else{
		return 1;
	}
	i += 3;

	// Now the operation, which comes after the "->", and takes the form state,bit,direction(STOP)
	struct op curr_op;
	// First, make a char array of just the chars after "->"
	char opchars[strlen(instruc) - i];
	for (int j=0; j+i<strlen(instruc); j++)
		opchars[j] = instruc[j+i];

	// Identify the state
	for (i=0; i<3 && opchars[i]!=','; i++){
		state[i] = opchars[i];
	}
	for (int j=i; j<3; j++)
		state[j] = '\0';

	curr_op.state = (char) atoi(state);
	if (curr_op.state >= max_states)
		return 1;
	i++;

	// Check the instruction contains enough characters for the remaining
	if (strlen(opchars) < i+4)
		return 1;
	
	// The next comma-delimited segment is either a 1 or a 0
	if ((opchars[i]=='0' || opchars[i]=='1') && opchars[i+1] == ','){
		curr_op.val = opchars[i] == '1';
	} else{
		return 1;
	}
	i += 2;

	// Then either R or L
	if (opchars[i] == 'L' || opchars[i] == 'R'){
		curr_op.dir = opchars[i] == 'R';
	} else{
		return 1;
	}
	i += 1;

	// And finally check if the instruction contains STOP
	if (opchars[i] == '\n' || opchars[i] == 127){
		curr_op.stop = false;
	} else{
		if (strlen(opchars) < i+4)
			return 1;
		if (opchars[i] != 'S' || opchars[i+1] != 'T' || opchars[i+2] != 'O' || opchars[i+3] != 'P')
			return 1;
	}

	// Then store the instruction in instructions
	instructions[instate][indigit] = curr_op;
	logprint("Loading operation %d, %d, %c %s to state %d and bit %d.\n", curr_op.state, curr_op.val, (curr_op.dir ? 'R' : 'L'), (curr_op.stop ? ", STOP " : " "), instate, indigit);
	
	return 0;
}

// Try to read and parse the intsruction set text file named by argv[1]
int load_instrucs(char *fname){
	FILE *fp = fopen(fname, "r");
	if (!fp){
		printf("Couldn't open file: %s.\n", fname);
		return 1;
	}

	// First line should be "STATES: [number between 1 and 127]"
	char first[12];
	if (fgets(first, 12, fp)){
		if (strncmp("STATES: ", first, 8) != 0){
			printf("%s should begin \"STATES: [number between 1 and 127]\".\n", fname);
			fclose(fp);
			return 1;
		}

		// Check the four characters after STATES: 
		char states[4];
		for (int i=8; ; i++){
			states[i-8] = first[i];
			if (first[i]=='\0') break;
		}

		max_states = (char) atoi(states);
		if (max_states <= 0){
			printf("%s should begin \"STATES: [number between 1 and 127]\".\n", fname);
			fclose(fp);
			return 1;
		}
	} else{
		printf("Error reading file: %s.\n", fname);
		fclose(fp);
		return 1;
	}
	instructions = malloc(sizeof(struct op[max_states][2]));

	// Then loop through the subsequent lines, which should number max_states * 2
	char line[21];
	for (int l=0; l<max_states*2; l++){
		if (!fgets(line, 21, fp)){
			printf("Error reading %s: %d instructions expected.\n", fname, max_states*2);
			fclose(fp);
			return 1;
		}
		if (parse_instruc(line) != 0){
			printf("Error parsing instruction: %s\n", line);
			fclose(fp);
			return 1;
		}
	}

	// Ignore the rest of the file if there is any
	if (fgetc(fp) != EOF){
		printf("WARNING: Ignoring %s from line %d.\n", fname, max_states*2+2);
	}

	fclose(fp);
	return 0;
}

int main(int argc, char *argv[]){
	// Parse command-line arguments
	if (argc <  3){
		print_usg();
		return 1;
	}

	if (load_instrucs(argv[1]) == 1){
		return 1;
	}

	free(instructions);
}