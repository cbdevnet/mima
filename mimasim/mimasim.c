#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <inttypes.h>

#ifndef _WIN32
	#include <termios.h>
	#include <unistd.h>

	//this emulates conio.h's getch().
	//via http://cboard.cprogramming.com/faq-board/27714-faq-there-getch-conio-equivalent-linux-unix.html

	int _getch(void){
		struct termios canon, raw;
		int ch;
		tcgetattr(STDIN_FILENO, &canon);
		raw = canon;
		raw.c_lflag &= ~(ICANON | ECHO);
		tcsetattr(STDIN_FILENO, TCSANOW, &raw);
		ch = getchar();
		tcsetattr(STDIN_FILENO, TCSANOW, &canon);
		return ch;
	}

	#define _GETCH_ _getch
#else
	#define _GETCH_ getch
#endif

/**

MIMA Simulator
Input file needs to be ASCII-Format Memory
Map. See mimasm http:/dev.cbcdn.com/kit/mimasm/

Comments, bug reports, feature requests:
	cbdev on irc://irc.freenode.net/
	cb@cbcdn.com

Known Bugs
----------
Interactive mode memory inspection is kinda buggy

History
-------
06062012 ~1200 Project begin
06062012 1734 Basic functionality pass
06062012 1924 Extended functionality pass
07062012 1632 Fixed some bugs
07062012 1750 Reworked output calls
08062012 1652 Implemented file output format, interactive 'm'
09062012 1435 Shortened setcell by a considerable amount. yay me.
09062012 1458 Compiles under linux now. Interactive mode not as pretty.
11062012 1034 Found a replacement for getch. Works just as fine.
23032013 2310 Fixed 2 small memory leaks
20052013 1904 Neatened up parts of the code
07062013 1232 Fixed address range to 20 bits / data range to 24 bits
09052014 1520 C89 compliance, Fixed warnings
15052014 1852 Breakpoint functionality
18052015 1829 Fix minor bug in JIND implementation
26112016 1804 Code style update
FJS 2012-2016
*/

#define OPCODE(a) (((a) & 0xF00000) >> 20)
#define EXTOPCODE(a) (((a) & 0xF0000) >> 16)
#define PARAMETER(a) ((a) & 0xFFFFF)
#define MIMAWORD(a) ((a) & 0xFFFFFF)

typedef struct _MEMCELL {
	struct _MEMCELL* next;
	struct _MEMCELL* prev;
	char* name;
	size_t mempos;
	uint32_t value;
} MEMCELL;

struct /*GLOBAL_OPTS*/ {
	bool verbose;
	bool dumpverbose;
	uint64_t maxsteps;
	bool breakpoints;
	bool interact;
	char* input;
	char* output;
} OPTIONS = {
	.verbose = false,
	.dumpverbose = false,
	.maxsteps = 0,
	.breakpoints = false,
	.interact = false,
	.input = NULL,
	.output = NULL
};

struct /*MEM*/ {
	MEMCELL bottom;
	MEMCELL* top;
} MEMORY = {
	.bottom = {0},
	.top = &MEMORY.bottom
};

struct /*MIMA*/{
	MEMCELL* ir;
	uint32_t iar;
	uint32_t akku;
	uint64_t steps;
	bool running;
} MIMA = {
	.ir = NULL,
	.iar = 0,
	.akku = 0,
	.steps = 1,
	.running = true
};

//Linked List Prototypes
int setcell(size_t, uint32_t, char*); //set cell to value
MEMCELL* getcell(size_t); //get cell
void freemem(); //free list

/*
Used some kind of special "always-sorted weirdly
pseudo-optimized" linked list because most programs
don't need the entire memory to be allocated at any
point in execution. Also I liked the challenge of
implementing it.
*/

void print_state(char* cmd, bool memory_accessed, MEMCELL* modified, FILE* output){
	MEMCELL* cell = NULL;

	if(memory_accessed){
		cell = getcell(PARAMETER(MIMA.ir->value));
	}

	//steps & current instruction counter
	printf("[%5" PRIu64 "] [@0x%06zX] ", MIMA.steps, MIMA.ir->mempos);

	//labelling
	if(MIMA.ir->name && OPTIONS.verbose){
		printf("[%s] ", MIMA.ir->name);
	}
	else{
		printf("\t");
	}

	//instruction
	printf("%s ", cmd);

	//parameter
	if(memory_accessed){
		if(OPTIONS.verbose && cell->name){
			printf("%s (0x%06X)", cell->name, cell->value);
		}
		else{
			printf("0x%06zX(@0x%06X)", cell->mempos, cell->value);
		}
	}
	else if(!modified){
		printf("0x%06X", PARAMETER(MIMA.ir->value));
	}
	printf("\n");

	//file output
	if(output){
		//steps & label
		fprintf(output, " %8" PRIu64 "  %10s ", MIMA.steps, (OPTIONS.verbose && MIMA.ir->name) ? MIMA.ir->name:"");
		//ir & op
		fprintf(output, " 0x%06X  %6s ", MIMA.ir->value, cmd);

		//parameter
		if((memory_accessed && (!OPTIONS.verbose || (OPTIONS.verbose && !cell->name)))
				|| (!memory_accessed && !modified)){
			fprintf(output,"   0x%06X ", PARAMETER(MIMA.ir->value));
		}
		else{
			fprintf(output," %10s ", (memory_accessed && OPTIONS.verbose && cell->name) ? cell->name:"");
		}

		//registers
		fprintf(output," 0x%06X  0x%06X ", MIMA.akku, MIMA.iar);

		//memory accesses
		if(modified && memory_accessed){
			if(modified->name && OPTIONS.verbose){
				fprintf(output, " %s", modified->name);
			}
			else{
				fprintf(output, " 0x%06zX", modified->mempos);
			}
			fprintf(output, " = 0x%06X", modified->value);
		}
		fputs("\n", output);
	}
}

MEMCELL* getcell(size_t pos){
	MEMCELL* cell = &MEMORY.bottom, *insert = NULL;
	bool forward = true;

	if(pos > MEMORY.top->mempos){
		//create at top
		cell = calloc(1, sizeof(MEMCELL));
		cell->mempos = pos;

		//mount
		cell->prev = MEMORY.top;
		MEMORY.top->next = cell;
		MEMORY.top = cell;
		return cell;
	}

	if(MEMORY.top->mempos - pos < MEMORY.top->mempos / 2){
		//start from top
		forward = false;
		cell = MEMORY.top;
	}

	for(; cell; cell = forward ? cell->next:cell->prev){
		if(cell->mempos == pos){
			return cell;
		}

		if((forward && cell->next && cell->next->mempos > pos)
				|| (!forward && cell->prev && cell->prev->mempos < pos)){
			//insert
			insert = calloc(1, sizeof(MEMCELL));
			if(!insert){
				printf("Failed to allocate memory\n");
				return NULL;
			}
			insert->mempos = pos;

			//mount
			insert->next = forward ? cell->next:cell;
			insert->prev = forward ? cell:cell->prev;
			if(forward){
				cell->next->prev = insert;
				cell->next = insert;
			}
			else{
				cell->prev->next = insert;
				cell->prev = insert;
			}
			return insert;
		}
	}
	return NULL;
}

int setcell(size_t pos, uint32_t value, char* comment){
	MEMCELL* cell =	getcell(pos);
	size_t length;
	if(!cell){
		return -1;
	}

	cell->value = value;
	if(comment && *comment == ';'){
		comment++;

		for(length = 0; comment[length] && comment[length] != '\n'; length++){
		}

		//read comment and store
		if(cell->name){
			free(cell->name);
		}
		cell->name = calloc(length + 1, sizeof(char));
		if(!cell->name){
			printf("Failed to allocate memory\n");
			return -1;
		}
		strncpy(cell->name, comment, length);
	}
	return 0;
}

void freemem(){
	MEMCELL* cell = &MEMORY.bottom;

	for(; cell; cell = cell->next){
		if(OPTIONS.dumpverbose || cell->name){
			printf(" END: Cell 0x%05zX %s%s%swas at 0x%X\n", cell->mempos, cell->name ? "(":"", cell->name ? cell->name:"", cell->name ? ") ":"", cell->value);
		}

		if(cell->name){
			free(cell->name);
		}

		if(cell->prev && cell->prev != &MEMORY.bottom){
			free(cell->prev);
		}

		if(!(cell->next) && cell != &MEMORY.bottom){
			free(cell);
			break;
		}
	}
}

int args_parse(int argc, char** argv){
	unsigned a;

	for(a = 1; a < argc; a++){
		if(argv[a][0] == '-'){
			switch(tolower(argv[a][1])){
				case 'i':
					printf("Using interactive mode\n");
					OPTIONS.interact = true;
					break;
				case 'd':
					if(tolower(argv[a][2] == 'v')){
						printf("Setting verbose dump flag\n");
						OPTIONS.dumpverbose = true;
					}
					break;
				case 'v':
					printf("Increased verbosity\n");
					OPTIONS.verbose = true;
					break;
				case 'e':
					if(argc > a + 1){
						MIMA.iar = strtoul(argv[++a], NULL, 0);
						printf("Using entry point 0x%06X\n", MIMA.iar);
					}
					else{
						printf("Entry point flag was specified, but had no argument.\n");
						return 1;
					}
					break;
				case 'l':
					if(argc > a + 1){
						OPTIONS.maxsteps = strtoul(argv[++a], NULL, 0);
						printf("Doing %" PRIu64 " steps at max\n", OPTIONS.maxsteps);
					}
					else{
						printf("Limit steps flag was specified, but had no argument.\n");
						return 1;
					}
					break;
				case 'b':
					printf("Enabling breakpoint handling\n");
					OPTIONS.breakpoints = true;
					break;
				default:
					printf("Unknown flag \"%s\"\n", argv[a]);
					return 1;
			}
		}
		else{
			if(!OPTIONS.input){
				printf("Using input %s\n", argv[a]);
				OPTIONS.input = argv[a];
			}
			else if(!OPTIONS.output){
				printf("Using output %s\n", argv[a]);
				OPTIONS.output = argv[a];
			}
			else{
				printf("Could not assign parameter \"%s\"\n", argv[a]);
				return 1;
			}
		}
	}
	return 0;
}

int scan_input(){
	FILE* input;
	long filesize;
	char* file;
	char* scan;
	size_t address;
	uint32_t value;

	input = fopen(OPTIONS.input, "r");
	if(!input){
		printf("Could not open input file\n");
		return 1;
	}

	fseek(input, 0, SEEK_END);
	filesize = ftell(input);
	rewind(input);

	if(filesize < 0){
		perror("Error reading input file: ");
		fclose(input);
		return 1;
	}

	file = calloc(filesize + 1, sizeof(char));
	if(!file){
		printf("Failed to allocate buffer\n");
		fclose(input);
		return 1;
	}

	if(fread(file, 1, filesize, input) != filesize){
		printf("Failed to read input file\n");
		fclose(input);
		free(file);
		return 1;
	}

	scan = file;
	while(scan - file < filesize){
		address = PARAMETER(strtoul(scan, &scan, 0));
		value = MIMAWORD(strtoul(scan, &scan, 0));

		//skip leading
		for(; *scan && *scan != ';' && *scan != '\n'; scan++){
		}

		if(setcell(address, value, scan)){
			return 1;
		}

		//skip to newline
		for(; *scan && *scan != '\n'; scan++){
		}
		scan++;
	}

	free(file);
	fclose(input);
	return 0;
}

int simulate_step(FILE* output){
	MIMA.ir = getcell(MIMA.iar);
	MIMA.iar = PARAMETER(MIMA.iar + 1);
	switch(OPCODE(MIMA.ir->value)){
		case 0x0: //LDC
			MIMA.akku = PARAMETER(MIMA.ir->value);
			print_state("LDC", false, NULL, output);
			break;
		case 0x1: //LDV
			MIMA.akku = getcell(PARAMETER(MIMA.ir->value))->value;
			print_state("LDV", true, NULL, output);
			break;
		case 0x2: //STV
			setcell(PARAMETER(MIMA.ir->value), MIMA.akku, NULL);
			print_state("STV", true, getcell(PARAMETER(MIMA.ir->value)), output);
			break;
		case 0x3: //ADD
			MIMA.akku = MIMAWORD(MIMA.akku + getcell(PARAMETER(MIMA.ir->value))->value);
			print_state("ADD", true, NULL, output);
			break;
		case 0x4: //AND
			MIMA.akku &= getcell(PARAMETER(MIMA.ir->value))->value;
			print_state("AND", true, NULL, output);
			break;
		case 0x5: //OR
			MIMA.akku |= getcell(PARAMETER(MIMA.ir->value))->value;
			print_state("OR", true, NULL, output);
			break;
		case 0x6: //XOR
			MIMA.akku ^= getcell(PARAMETER(MIMA.ir->value))->value;
			print_state("XOR", true, NULL, output);
			break;
		case 0x7: //EQL
			MIMA.akku = (MIMA.akku == getcell(PARAMETER(MIMA.ir->value))->value) ? MIMAWORD(-1):0;
			print_state("EQL", true, NULL, output);
			break;
		case 0x8: //JMP
			MIMA.iar = PARAMETER(MIMA.ir->value);
			print_state("JMP", true, NULL, output);
			break;
		case 0x9: //JMN
			if(MIMA.akku & 0x800000){
				MIMA.iar = PARAMETER(MIMA.ir->value);
			}
			print_state("JMN", true, NULL, output);
			break;
		case 0xA: //LDIV
			MIMA.akku = getcell(getcell(PARAMETER(MIMA.ir->value))->value)->value;
			print_state("LDIV", true, NULL, output);
			break;
		case 0xB: //STIV
			setcell(getcell(PARAMETER(MIMA.ir->value))->value, MIMA.akku, NULL);
			print_state("STIV", true, getcell(getcell(PARAMETER(MIMA.ir->value))->value), output);
			break;
		case 0xC: //JMS
			setcell(PARAMETER(MIMA.ir->value), MIMA.iar, NULL);
			MIMA.iar = PARAMETER(MIMA.ir->value) + 1;
			print_state("JMS", true, getcell(PARAMETER(MIMA.ir->value)), output);
			break;
		case 0xD: //JIND
			MIMA.iar = PARAMETER(getcell(PARAMETER(MIMA.ir->value))->value);
			print_state("JIND", true, NULL, output);
			break;
		case 0xF:
			switch(EXTOPCODE(MIMA.ir->value)){
				case 0x0: //HALT
					print_state("HALT", false, MIMA.ir, output);
					if(!OPTIONS.breakpoints){
						MIMA.running = false;
					}
					else{
						printf("M: Breakpoint hit, going interactive. Press 'c' to resume.\n");
						OPTIONS.interact = true;
					}
					break;
				case 0x1: //NOT
					MIMA.akku = MIMAWORD(~MIMA.akku);
					print_state("NOT", false, MIMA.ir, output);
					break;
				case 0x2: //RAR
					MIMA.akku = ((MIMA.akku & 1) << 23 | MIMA.akku >> 1);
					print_state("RAR", false, MIMA.ir, output);
					break;
				default: //FAIL
					printf("OP %X, EXOP %X ", OPCODE(MIMA.ir->value), EXTOPCODE(MIMA.ir->value));
					print_state("FAIL", false, MIMA.ir, output);
					break;
			}
			break;
	}
	return 0;
}

int main(int argc, char* argv[]){
	FILE* output = NULL;
	size_t query_addr = 0;
	if(argc<2){
		printf("Usage: mimasim [-i] [-dv] [-v] [-b] [-e <entrypoint>] [-l <maxsteps>] <infile> [<outfile>]\n");
		printf("\t-i\tInteractive execution\n");
		printf("\t-v\tVerbose mode (resolve labels)\n");
		printf("\t-dv\tVerbose memory dump (all active cells)\n");
		printf("\t-e\tSpecify execution entry point\n");
		printf("\t-l\tLimit execution steps (force abort)\n");
		printf("\t-b\tEnable breakpoints (Drop into interactive mode upon HALT)\n");
		return 0;
	}

	//parse arguments
	if(args_parse(argc, argv)){
		return EXIT_FAILURE;
	}

	if(!OPTIONS.input){
		printf("Missing an input file\n");
		return EXIT_FAILURE;
	}

	if(OPTIONS.output){
		output = fopen(OPTIONS.output, "w");
		if(!output){
			printf("Could not open output file\n");
			return 1;
		}
		fprintf(output, "Reading memory from %s...", OPTIONS.input);
	}

	printf("Reading input file %s... ", OPTIONS.input);
	if(scan_input()){
		return EXIT_FAILURE;
	}

	printf("done\n");
	if(OPTIONS.output){
		fputs(" OK\n", output);
		fprintf(output, "[%8s][%10s][%8s][%6s][%10s][%8s][%8s][%-20s]\n", "STEP", "LABEL", "IR   ", "OP  ", "PARAMETER", "AKKU  ", "IAR  ", " MEM");
	}

	//print interactive mode help
	if(OPTIONS.interact){
		printf("Interactive mode commands:\n");
		printf("\tq\t\tstop execution\n");
		printf("\ta\t\tprint accumulator\n");
		printf("\ti\t\tprint iar\n");
		printf("\tm\t\tquery memory contents\n");
		printf("\tc\t\tcontinue execution\n");
		printf("\tn\texecute next step\n");
	}

	while(MIMA.running){
		//interactive shell
		if(OPTIONS.interact){
			bool next = false, quit = false;

			while(!next){
				printf("> ");
				switch(tolower(_GETCH_())){
					case 'm':
						printf("Addr ?> 0x");
						fflush(stdin);
						if(scanf("%zx", &query_addr) != EOF){
							printf("0x%06zX @ 0x%06X\n", query_addr, getcell(query_addr)->value);
						}
						else{
							printf(" failed\n");
						}
						break;

					case 'q':
						printf("\n");
						next = true;
						quit = true;
						break;

					case 'n':
						next = true;
						break;

					case 'a':
						printf("Akku: 0x%06X\n", MIMA.akku);
						break;

					case 'i':
						printf(" IAR: 0x%06X\n", MIMA.iar);
						break;

					case 'c':
						printf("\n");
						OPTIONS.interact = false;
						next = true;
						break;
					case 10:
					case 13:
						printf("\n");
						break;
					default:
						printf("Unrecognized input\n");
						break;
				}
			}

			if(quit){
				break;
			}
		}

		simulate_step(output);

		if(MIMA.steps == OPTIONS.maxsteps){ //wont fire if 0
			printf("> Reached allowed step limit, aborting\n");
			break;
		}
		MIMA.steps++;
	}

	if(OPTIONS.interact){
		printf("> Execution stopped\n");
		_GETCH_();
	}

	freemem();
	if(output){
		fclose(output);
	}
	return EXIT_SUCCESS;
}
