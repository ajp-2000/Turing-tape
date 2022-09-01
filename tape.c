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
 * The machine starts at position 0 on the tape, with internal state 0. At every step, the present 
 * internal state and bit being read are printed, by default to stdout, along with the instruction to 
 * be executed. When the machine reaches STOP, the program exits.
 *
 * Text files representing a length of tape and an instruction set respectively must be given
 * as command-line paramaters. Any changes made to the tape will be saved to the file; this won't 
 * necessarily all be at the STOP command, because the program only reads one buffer of tape at a 
 * time, and writes all changes to that buffer once a new section of tape is needed. The BUFFER_SIZE 
 * is 128 by default, which is much smaller than modern computers demand, but low enough to 
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

#define BUFFER_SIZE 128
FILE *LOG_STREAM;

// The instruction list is captured by a two-dimensional array, structured as [state][bit], of 
// operations to carry out. Each operation is a struct of a new state to enter, a digit to write,
// a bool representing left (0) or right (1), and a bool for stoppping
struct op{
	char state;
	bool val;
	bool dir;
	bool stop;
};

// Some global variables
struct op (*instructions)[2];
char max_states;
int position = 0;
int buf_pos = 0;
char state = 0;
bool buffer[BUFFER_SIZE];

// Print a formatted string to the log, i.e. either to stdout or do nothing
// LOG_STREAM is the stream that we print to, by default stdout or a file if specified with -o;
// But the user can also suppress the log altogether with -s. Since we can't fprint to NULL, we
// need to first check LOG_STREAM isn't null and go ahead only if it isn't.
void logprint(char *fmt, ...){
	extern FILE *LOG_STREM;

	if (LOG_STREAM){
		va_list args;
		va_start(args, fmt);
		vfprintf(LOG_STREAM, fmt, args);
		va_end(args);
	}
}

// Print the command-line usage text
void print_usg(){
	printf("USAGE: ./tape.c [INSTRUCTION SET] [TAPE] [OPTIONS]\n\n");
	printf("Options:\n\n\t-s\t\tsilence log\n");
	printf("\t-o [FILENAME]\twrite log to FILENAME\n\n");
}

// Return a char * detailing the given instruction
void print_instruc(int instate, int indigit){
	if (LOG_STREAM)
		fprintf(LOG_STREAM, "%d,%d->%d,%d,%c%s", instate, indigit, instructions[instate][indigit].state, instructions[instate][indigit].val, (instructions[instate][indigit].dir ? 'R' : 'L'), (instructions[instate][indigit].stop ? "STOP" : ""));
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
		if (strlen(opchars) < i+3)
			return 1;
		if (opchars[i] != 'S' || opchars[i+1] != 'T' || opchars[i+2] != 'O' || opchars[i+3] != 'P')
			return 1;

		curr_op.stop = true;
	}

	// Then store the instruction in instructions
	instructions[instate][indigit] = curr_op;
	logprint("Loading operation %d, %d, %c %sto state %d and bit %d.\n", curr_op.state, curr_op.val, (curr_op.dir ? 'R' : 'L'), (curr_op.stop ? ", STOP " : " "), instate, indigit);
	
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

	// Set all instructions to just go right, to begin with
	for (int s=0; s<max_states; s++){
		for (int d=0; d<2; d++){
			struct op default_op;
			default_op.state = s;
			default_op.val = d;
			default_op.dir = 1;
			default_op.stop = 0;
			instructions[s][d] = default_op;
		}
	}

	// Then loop through the subsequent lines, which should number max_states * 2
	char line[20];
	for (int l=0; l<max_states*2; l++){
		if (!fgets(line, 20, fp)){
			break;
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

/* The following three functions handle the tape and its buffer.
 * read_buf() reads BUFFER_SIZE binary digits into buffer, starting at the given digit
 * write_buf() writes the current contents of the buffer back to the relevant segment of tape
 * load_tape() opens the tape file and sets the global variable tapef to point to it
 */
FILE *tapef;

int read_buf(int start){
	extern FILE *tapef;

	// Generate indefinite zeroes to either side of the tape that already exists
	if (fseek(tapef, start, SEEK_SET) != 0){
		for (int i=0; i<BUFFER_SIZE; i++)
			buffer[i] = false;

		return 0;
	}

	for (int i=0; i<BUFFER_SIZE; i++){
		char c = fgetc(tapef);
		if (c == '0'){
			buffer[i] = false;
		} else if (c == '1'){
			buffer [i] = true;
		} else if (c == EOF){
			// Pad the buffer with zeroes
			for (; i<BUFFER_SIZE; i++)
				buffer[i] = false;
		} else{
			printf("Unrecognised character in tape: %c.\n", c);
			return 1;
		}
	}

	return 0;
}

int write_buf(int start){
	extern FILE *tapef;
	if (fseek(tapef, start, SEEK_SET) != 0){
		if (start < 0){
			// The start point is before the file begins, i.e. a new buffer-sized segment has been
			// added to the left. A real Turing machine would have infinite tape in both directions,
			// and our limitation is just because we are simulating one using a .txt file that has to
			// begin somewhere - so we can justifiably 'cheat' by moving the whole file to the right
			// in a chunk of a size that the machine we are simulating wouldn't be able to process
			// all at once.
			fseek(tapef, 0, SEEK_END);
			int flen = ftell(tapef);
			char curr_f[flen];

			fseek(tapef, 0, SEEK_SET);
			for (int i=0; i<flen; i++)
				curr_f[i] = fgetc(tapef);

			// Write the buffer to the beginning, and then append the rest of the tape
			write_buf(0);
			for (int i=0; i<flen; i++)
				fputc(curr_f[i], tapef);
		} else{
			// Then the problem was that start is beyond where the file ends, so we pad it with zeroes
			fseek(tapef, 0, SEEK_END);
			while (ftell(tapef) != start){
				fputc('0', tapef);
				fseek(tapef, 0, SEEK_END);
			}
		}
	}

	for (int i=0; i<BUFFER_SIZE; i++){
		int error = fputc((buffer[i] ? '1' : '0'), tapef);
		if (error == EOF){
			printf("Error writing to tape.\n");
			return 1;
		}
	}

	return 0;
}

int load_tape(char *fname){
	extern FILE *tapef;
	if (!(tapef = fopen(fname, "rb+"))){
		printf("Error: could not open file: %s.\n", fname);
		return 1;
	}

	if (read_buf(0))
		return 1;

	return 0;
}

int run(){
	struct op curr_op;
	logprint("Execution:\n");
	logprint("|Machine state | Position | Bit | Instruction\n");
	logprint("|=================================================\n");

	while (true){		// Print to log
		logprint("| %-13d| %-9d| %-4d| ", state, buf_pos*BUFFER_SIZE + position, buffer[position]);
		print_instruc(state, buffer[position]);
		logprint("\n");

		// Execute the operation
		curr_op = instructions[state][buffer[position]];
		state = curr_op.state;
		buffer[position] = curr_op.val;
		position += curr_op.dir ? 1 : -1;

		// Manage the position: move the buffer, stop. etc.
		if (curr_op.stop){
			logprint("STOP reached.\n");
			break;
		}

		if (position == BUFFER_SIZE){
			// Write the current buffer, then load the block one to the right
			if (write_buf(buf_pos*BUFFER_SIZE))
				return 1;

			position = 0;
			buf_pos++;
			if (read_buf(buf_pos*BUFFER_SIZE))
				return 1;
		} else if (position == -1){
			// Do the same but to the left
			if (write_buf(buf_pos*BUFFER_SIZE))
				return 1;
			
			position = 0;
			buf_pos--;
			if (read_buf(buf_pos*BUFFER_SIZE))
				return 1;
		}
	}

	// Finish up
	if (write_buf(buf_pos*BUFFER_SIZE))
		return 1;

	if (LOG_STREAM){
		printf("Final state: %d\n", state);
		printf("Final position: %d\n", buf_pos*BUFFER_SIZE + position);
		printf("Bit at final position: %d\n", buffer[position]);
	}

	return 0;
}

int main(int argc, char *argv[]){
	// Parse command-line arguments
	if (argc <  3){
		print_usg();
		return 1;
	}

	// Handle any optional args
	extern FILE *LOG_STREAM;
	LOG_STREAM = stdout;

	for (int a=3; a<argc; a++){
		if (strcmp(argv[a], "-o") == 0){
			if (a+1 == argc){
				printf("Please provide a filename after -o.\n");
				return 1;
			}
			if (!(LOG_STREAM = fopen(argv[a+1], "w"))){
				printf("Error: could not open file: %s.\n", argv[a+1]);
				return 1;
			}
		} else if (strcmp(argv[a], "-s") == 0){
			LOG_STREAM = NULL;
		}
	}

	// Parse the necessary args only after the log stream has been set
	if (load_instrucs(argv[1]) == 1){
		return 1;
	}

	if (load_tape(argv[2]) == 1){
		return 1;
	}

	// Run the machine
	if (run())
		return 1;

	free(instructions);
	if (LOG_STREAM != stdout)
		fclose(LOG_STREAM);

	return 0;
}