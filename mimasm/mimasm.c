#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

/**
04062012 1545 File creation
04062012 2200 Functionality pass
06062012 1955 Pretty much complete

Known Bugs
----------
GCC Compilation outputs weird stuff on stdout //FIXED 05062012 1132
HALT on last line not recognized //FIXED 05062012 1050
Preprocessor Constants dont work as expected //FIXED 05062012 1413
Initialize empty DS' to 0 //FIXED 05062012 1143
ORG's dont work properly without spaces //FIXED 05062012 2057

History
----------
Added JMS + JIND 06062012 1909
Fixed subtle bug with files not having a newline at the end 06062012 2231
Documentation update, example code fixes 20052013 2014
Fixed bug on line 276: "OR" actually _has_ length 2 - thanks Sinan! 04062013 0755
Fixed segfault on empty input file - Reported by drone| 06062013 2204
Fixed bug that allowed to jump to out-of-range addresses - Reported by Indidev & drone| 07062013 1120
Limited address output length to the standard 20 bits - reported by drone| 07062013 1222
C89 Compliance, Warnings fixed 09052014 1520
Code style upgrade rewrite 20112016 1700


About
----------
Had this idea and just had to implement it.
Reads a file of MIMA Assembler and creates an ASCII Memory
Map to be read by an interpreter. Should be pretty fault
tolerant, wont do anything flashy on encountering an error tho.
Might want to watch stdout closely for that.

If you're looking for an interpreter, I wrote one.
Check http://dev.cbcdn.com/kit/mimasim/

Comments, bug reports, feature requests:
	cbdev on irc://irc.freenode.net/
	cb@cbcdn.com

FJS 2012-2014
**/

#define MIMAWORD(a) ((a)&0xFFFFFF)
#define MIMAPARAM(a) ((a)&0xFFFFF)

typedef struct /*GLOBTABLE*/ {
	char* name;
	uint32_t value;
} GLOBAL;

GLOBAL** globals = NULL;

//global table handling prototypes
int global_create(char*, uint32_t);	//create global from name and value
bool global_exists(char*);		//query global existence with name from buffer
uint32_t global_value(char*);		//query global value with name from buffer
void globals_free();			//free global table entries

//parsing prototypes
uint32_t parse_integer(char*);		//parse integral values (hex/dec)
uint32_t parse_parameter(char*);	//get parameter value
uint32_t parse_op(char*);		//get opcode from sentence
size_t label_length(char*, bool);	//get label length

//assembler subroutines
size_t scan_input(char*, char**);	//scan input file into buffer
int scan_globals(char*, size_t);	//scan buffer for global definitions
int parse_line(FILE*, uint32_t, char*, size_t);	//parse single sentence
int parse_buffer(FILE*, char*, size_t);	//split buffer into sentences and process reorgs

uint32_t global_value(char* buffer){
	size_t u;
	size_t label_len = label_length(buffer, false);
	if(!globals){
		printf("W:GLVL: Global table empty\n");
		return 0;
	}

	for(u = 0; globals[u]; u++){
		if(strlen(globals[u]->name) == label_len && !strncmp(buffer, globals[u]->name, label_len)){
			return globals[u]->value;
		}
	}
	printf(" [E:GLVL: No such global %.*s]", (int)label_len, buffer);
	//sentinel value
	return 0xDEAD;
}

bool global_exists(char* buffer){
	size_t label_len = label_length(buffer, false);
	size_t u;

	if(!globals){
		return false;
	}

	for(u = 0; globals[u]; u++){
		//compare length too as we need to use strncmp
		if(strlen(globals[u]->name) == label_len && !strncmp(buffer, globals[u]->name, label_len)){
			return true;
		}
	}

	return false;
}

void globals_free(){
	size_t u;
	if(!globals){
		return;
	}

	for(u = 0; globals[u]; u++){
		printf(" M:GLFR: Freeing %10s @ %6X\n", globals[u]->name, globals[u]->value);
		free(globals[u]->name);
		free(globals[u]);
	}

	free(globals);
}

int global_create(char* buffer, uint32_t value){
	size_t num_globals = 0;
	size_t label_len = label_length(buffer, true);
	if(label_len < 1){
		printf("  E:GLCR: No label found\n");
		return -2;
	}

	if(global_exists(buffer)){
		printf("  W:GLCR: Global %.*s exists, not overwriting it\n", (int)label_len - 1, buffer);
		return -1;
	}

	//count globals
	for(;globals && globals[num_globals]; num_globals++){
	}

	//reallocate
	globals = realloc(globals, sizeof(GLOBAL*) * (num_globals + 2));
	if(!globals){
		printf("  E:GLCR: ALLOCATION FAILED @ %d\n", __LINE__);
		return -3;
	}

	globals[num_globals] = calloc(1, sizeof(GLOBAL));
	if(!globals[num_globals]){
		printf("  E:GLCR: ALLOCATION FAILED @ %d\n", __LINE__);
		return -3;
	}

	//sentinel
	globals[num_globals + 1] = NULL;

	globals[num_globals]->value = value;
	globals[num_globals]->name = calloc(label_len + 1, sizeof(char));
	if(!globals[num_globals]->name){
		printf("  E:GLCR: ALLOCATION FAILED @ %d\n", __LINE__);
		return -3;
	}

	strncpy(globals[num_globals]->name, buffer, label_len);
	//printf("M:GLCR: %s=0x%X\n", globals[num_globals]->name, globals[num_globals]->value);
	return 0;
}

uint32_t parse_parameter(char* buffer){
	//skip opcode to find operand
	for(; *buffer != ' '; buffer++){
		if(*buffer == '\n' || !(*buffer)){
			printf("[ASSUMED] ");
			return 0;
		}
	}

	buffer++;

	//check for integral parameter
	if(*buffer == '$' || isdigit(*buffer)){
		return parse_integer(buffer);
	}

	//otherwise, check for a global
	if(global_exists(buffer)){
		return global_value(buffer);
	}

	printf("[INVALID] ");
	return 0;
}


uint32_t parse_op(char* buffer){
	size_t op_len = 0;
	//skip blanks
	for(; *buffer == ' '; buffer++){
	}

	if(*buffer == 0 || *buffer == '\n'){
		printf(" [OPRES FAILED]");
		return 0;
	}

	for(; buffer[op_len] && buffer[op_len] != ' ' && buffer[op_len] != '\n'; op_len++){
	}

	//printf(" [OPLEN %d] ",len);
	if(op_len >= 2){
		if(!strncmp(buffer, "LD", 2)){
			switch(buffer[2]){
				case 'C': //LDC
					return 0;
				case 'V': //LDV
					return 1;
				case 'I': //LDIV
					return 0xA;
			}
		}

		if(!strncmp(buffer, "ST", 2)){
			switch(buffer[2]){
				case 'V': //STV
					return 2;
				case 'I': //STIV
					return 0xB;
			}
		}

		if(*buffer == 'A'){
			switch(buffer[1]){
				case 'D': //ADD
					return 3;
				case 'N': //AND
					return 4;
			}
		}

		if(*buffer == 'O'){ //OR
			return 5;
		}

		if(*buffer == 'X'){ //XOR
			return 6;
		}

		if(*buffer == 'E'){ //EQL
			return 7;
		}

		if(!strncmp(buffer, "JI", 2)){
			return 13;
		}

		if(!strncmp(buffer, "JM", 2)){
			switch(buffer[2]){
				case 'P': //JMP
					return 8;
				case 'N': //JMN
					return 9;
				case 'S': //JMS
					return 12;
			}
		}

		if(*buffer == 'H' && op_len > 3){ //HALT
			return 0xF0;
		}

		if(*buffer == 'N'){ //NOT
			return 0xF1;
		}

		if(*buffer == 'R'){ //RAR
			return 0xF2;
		}
	}

	printf(" [OP INVALID %zu, %c] ", op_len, *buffer);
	return 0;
}
 
uint32_t parse_integer(char* buffer){
	int base = 10;
	for(; ; buffer++){
		switch(*buffer){
			case '$':
				base = 16;
				break;
			case 0:
			case '\n':
				return 0;
			case ' ':
				break;
			default:
				return strtol(buffer, NULL, base);
		}
	}
	return 0;
}

size_t label_length(char* buffer, bool strict_mode){
	size_t length = 0;
	for(; buffer[length]; length++){
		switch(buffer[length]){
			case '\n':
			case ' ':
				if(strict_mode){
					return 0;
				}
			case ':':
			case '=':
				return length;
			default:
				break;
		}
	}
	return 0;
}

size_t scan_input(char* infile, char** buffer){
	FILE* input;
	long filesize;
	int character;
	bool comment = false, hadspace = true;
	size_t scanned_length = 0;

	printf(" M:SCAN: Opening %s for mode rb\n", infile);

	input = fopen(infile, "rb");
	if(!input){
		printf(" E:SCAN: Could not open input file\n");
		return 0;
	}

	//query in put file length
	fseek(input, 0, SEEK_END);
	filesize = ftell(input);
	rewind(input);

	printf(" M:SCAN: Input file is %ld bytes long\n", filesize);

	*buffer = calloc(filesize + 1, sizeof(char));
	if(!(*buffer)){
		printf(" E:SCAN: Failed to allocate memory for input buffer\n");
		fclose(input);
		return 0;
	}

	while(!feof(input)){
		character = fgetc(input);
		switch(character){
			case ';':
				comment = true;
				break;
			case ' ':
			case '\t':
				if(!hadspace && !comment){
					(*buffer)[scanned_length++] = ' ';
				}
				hadspace = true;
			case -1:
			case '\r':
				break;
			case '\n':
				if(scanned_length > 0 && (*buffer)[scanned_length - 1] == ' '){
					scanned_length--;
				}
				(*buffer)[scanned_length++] = '\n';
				comment = false;
				hadspace = true;
				break;
			case '=':
			case ':':
				if(!comment && scanned_length > 0 &&
						(*buffer)[scanned_length - 1] == ' '){
					scanned_length--;
				}
				//fall thru
			default:
				hadspace = false;
				if(!comment){
					(*buffer)[scanned_length++] = character;
				}
				break;
		}
	}

	fclose(input);

	printf(" M:SCAN: Scanner pass returned %zu bytes\n", scanned_length);
	return scanned_length;
}

int scan_globals(char* buffer, size_t buffer_length){
	unsigned errors = 0, warnings = 0;

	size_t current_offset = 0, sentence_start = 0, sentence_end = 0;
	unsigned current_sentence = 0;
	uint32_t memory_position = 0;
	size_t label_len = 0;

	while(current_offset < buffer_length){
		sentence_start = current_offset;
		sentence_end = current_offset;
		current_sentence++;

		for(; buffer[sentence_end] && buffer[sentence_end] != '\n'; sentence_end++){
		}

		//detect reorgs
		if(buffer[sentence_start] == '*'){
			if(sentence_end - sentence_start >= 3){
				//scan for reorg target
				for(; sentence_start < sentence_end && buffer[sentence_start] != '='; sentence_start++){
				}
				if(buffer[sentence_start] != '='){
					printf(" E:GSCN: Reorg syntax failure in sentence %u\n", current_sentence);
					errors++;
				}
				else{
					memory_position = MIMAPARAM(parse_integer(buffer + sentence_start + 1));
					printf(" M:GSCN: ORG to 0x%X\n", memory_position);
					//increased at end of loop
					memory_position--;
				}
			}
			else{
				printf(" E:GSCN: Short reorg in sentence %u\n", current_sentence);
				errors++;
			}
		}
		//check for labels
		else{
			label_len = label_length(buffer + sentence_start, true);
			if(label_len){
				printf(" M:GSCN: Creating %s %.*s\n", (buffer[sentence_start + label_len] == '=') ? "global":"label", (int)label_len, buffer + sentence_start);

				switch(global_create(buffer + sentence_start, 
							(buffer[sentence_start + label_len] == '=') ? parse_integer(buffer + sentence_start + label_len + 1) : memory_position)){
					case -1:
						warnings++;
						break;
					case -2:
						errors++;
						break;
					case -3:
						printf(" E:GSCN: Allocation failure\n");
						errors++;
						return errors;
					default:
						break;
				}

				if(buffer[sentence_start + label_len] == '='){
					//globals dont increase memory position
					memory_position--;
				}
			}
		}

		memory_position = MIMAPARAM(memory_position + 1);
		current_offset = sentence_end + 1;
	}

	printf(" M:GSCN: Global table built with %u warnings, %u errors\n", warnings, errors);
	return errors;
}

int parse_line(FILE* output, uint32_t memory_position, char* buffer, size_t line_length){
	uint32_t opcode, parameter;
	char* operation = buffer;

	//handle labelling
	size_t label_len = label_length(buffer, true);
	if(label_len){
		//if global assign, ignore line
		if(buffer[label_len] == '='){
			printf("  M:LINE: Global %.*s assigned, ignoring\n", (int)label_len, buffer);
			return 1;
		}
	}

	printf("  M:LINE: ");
	if(label_len){
		printf("[%.*s] ", (int)label_len, buffer);
		operation += label_len + 1;
	}

	for(; *operation == ' '; operation++){
	}

	printf("%.*s => ", (int)(line_length - (operation - buffer)), operation);

	if(operation[0] == 'D' && operation[1] == 'S'){
		//handle DS
		parameter = parse_integer(operation + 2);
		fprintf(output, "0x%05X 0x%06X%s%.*s\n", memory_position, MIMAWORD(parameter), label_len ? " ;":"", (int)label_len, buffer);
		printf("DS 0x%06X\n", MIMAWORD(parameter));
	}
	else{
		//parse opcode
		opcode = parse_op(operation);
		printf("OP 0x%X ", opcode);

		if(opcode <= 0xF){
			//parse parameter
			parameter = parse_parameter(operation);
			printf("PARAM 0x%05X\n", MIMAPARAM(parameter));

			fprintf(output, "0x%05X 0x%X%05X%s%.*s\n", memory_position, opcode, MIMAPARAM(parameter), label_len ? " ;":"", (int)label_len, buffer);
		}
		else{
			fprintf(output, "0x%05X 0x%06X%s%.*s\n", memory_position, opcode << 16, label_len ? " ;":"", (int)label_len, buffer);
			printf("\n");
		}
	}
	return 0;
}

int parse_buffer(FILE* output, char* buffer, size_t buffer_length){
	size_t current_offset = 0, sentence_start = 0, sentence_end = 0;
	unsigned current_sentence = 0;
	uint32_t memory_position = 0;

	size_t sentence_length = 0;

	while(current_offset < buffer_length){
		//sprintf(memstring,"0x%05lX ",currentmem);
		sentence_start = current_offset;
		sentence_end = current_offset;
		current_sentence++;

		for(; buffer[sentence_end] && buffer[sentence_end] != '\n'; sentence_end++){
		}

		//printf(" M:PRSR: Sentence spans %zu - %zu\n", sentence_start, sentence_end);
		sentence_length = sentence_end - sentence_start;

		//detect reorg
		if(buffer[sentence_start] == '*'){
			//read reorg target
			for(; sentence_start < sentence_end && buffer[sentence_start] != '='; sentence_start++){
			}
			memory_position = MIMAPARAM(parse_integer(buffer + sentence_start + 1));
			printf(" M:PRSR: ORG to 0x%X\n", memory_position);
			memory_position--;
		}
		//detect blanks
		else if(sentence_start == sentence_end){
			fprintf(output, "0x%05X 0x000000\n", memory_position);
			printf(" M:PRSR: Blank line -> LDC 0\n");
		}
		else{
			switch(parse_line(output, memory_position, buffer + sentence_start, sentence_length)){
				case -1:
					//failure
					printf(" E:PRSR: Parser failed in sentence %d\n", current_sentence);
					return -1;
				case 1:
					//ignore parsed line
					memory_position--;
				default:
					break;
			}
		}

		current_offset = sentence_end + 1;
		memory_position = MIMAPARAM(memory_position + 1);
	}
	printf(" M:PRSR: Parser done\n");
	return 0;
}

int main(int argc, char* argv[]){
	if(argc < 2){
		printf("No input file. Usage: mimasm <infile> [<outfile>]\n");
		printf("Standard output file is out.mima\n");
		return 0;
	}

	FILE *output;
	char* outfile = "out.mima";

	char* buffer = NULL;
	size_t scanned_length = 0;

	if(argc > 2){
		outfile = argv[2];
	}

	printf("M:MAIN: Scanning input file\n");
	scanned_length = scan_input(argv[1], &buffer);
	if(scanned_length < 2){
		printf("E:MAIN: Failed to read input file\n");
		free(buffer);
		return EXIT_FAILURE;
	}

	output = fopen(outfile, "wb");
	if(!output){
		printf("E:MAIN: Could not open output file\n");
		free(buffer);
		return EXIT_FAILURE;
	}

	printf("M:MAIN: Building global table\n");
	if(scan_globals(buffer, scanned_length)){
		printf("E:MAIN: Global scanning raised errors\n");
		free(buffer);
		fclose(output);
		return EXIT_FAILURE;
	}

	printf("M:MAIN: Parsing statements\n");
	if(parse_buffer(output, buffer, scanned_length)){
		printf("E:MAIN: Parser raised irrecoverable error\n");
		globals_free();
		free(buffer);
		fclose(output);
		return EXIT_FAILURE;
	}

	printf("M:MAIN: Cleaning up\n");
	globals_free();
	free(buffer);
	fclose(output);
	return EXIT_SUCCESS;
}
