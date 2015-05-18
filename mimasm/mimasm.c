#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

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

#define ULONG unsigned long
#define MIMAWORD(a) ((a)&0xFFFFFF)
#define MIMAPARAM(a) ((a)&0xFFFFF)

#define STATE_WARN 1
#define STATE_ERR 2

typedef struct /*GLOBTABLE*/ {
	char* name;
	ULONG value;
} GLOBAL;

GLOBAL** globals=NULL;

//global table handling prototypes
ULONG globalcreate(char*, ULONG, ULONG);//create global from buffer, namelength, value
bool globalexists(char*, ULONG);//query global existence with name from buffer
ULONG globalvalue(char*, ULONG);//query global value with name from buffer
void freeglobals();//free global table entries

//parsing prototypes
ULONG parseint(char*,ULONG,ULONG);//parse integral values (hex/dec)
ULONG getparam(char*,ULONG);//get parameter from sentence
ULONG getopcode(char*, ULONG);//get opcode from sentence
ULONG label(char*, ULONG);//get label length or 0 if none

//auxiliary prototypes
void printlen(char*, ULONG, ULONG);

void printlen(char* mem, ULONG start, ULONG len){
	ULONG i;
	if(len==0){
		//print until line end
		for(;mem[start]!=0&&mem[start]!='\n';start++){
			putc(mem[start],stdout);
		}
	}
	else{
		//print length
		for(i=0;i<len;i++){
			putc(mem[start+i],stdout);
		}
	}
}

ULONG globalvalue(char* mem, ULONG start){
	if(!globals){
		printf(" [G:EMPTY] ");
		return 0;
	}
	
	ULONG llen=0, i;
	bool end=false;
	while(!end){
		switch(mem[start+llen]){
			case '\n':
			case ' ':
			case ':':
			case '=':
			case 0:
				end=true;
				break;
			default:
				llen++;
		}
	}
	
	for(i=0;globals[i];i++){
		if(llen==strlen(globals[i]->name)&&!strncmp(mem+start,globals[i]->name,llen)){
			return globals[i]->value;
		}
	}
	printf(" [G:NOSUCH:");
	printlen(mem,start,llen);
	printf("]");
	
	return 0xDEAD;
}

bool globalexists(char* mem, ULONG start){
	if(!globals){
		printf("[G:EMPTY] ");
		return false;
	}
	
	ULONG llen=0, i;
	bool end=false;
	while(!end){
		switch(mem[start+llen]){
			case ' ':
			case ':':
			case '\n':
			case '=':
			case 0:
				end=true;
				break;
			default:
				llen++;
		}
	}
	
	//printf("[LEN%d]",llen);
	
	for(i=0;globals[i];i++){
		if(strlen(globals[i]->name)==llen&&!strncmp(mem+start,globals[i]->name,llen)){
			//printf("[G:FOUND] ");
			return true;
		}
	}
	
	//printf("[G:NOEXST:");
	//printlen(mem,start,llen);
	//printf("] ");
	
	return false;
}

void freeglobals(){
	if(!globals){
		return;
	}

	int i;
	
	for(i=0;globals[i]!=NULL;i++){
		printf(" G:CLEAN: Freeing %10s @ %6lX\n",globals[i]->name,globals[i]->value);
		free(globals[i]->name);
		free(globals[i]);
	}
	free(globals);
}

ULONG globalcreate(char* mem, ULONG start, ULONG value){

	ULONG length=label(mem,start);
	if(length<1){
		printf(" [G:ERR: NO LABEL] ");
		return STATE_ERR;
	}
	
	//printf(" [G:LENGTH: %d] ",length);
	
	if(!globals){//lazy init
		globals=(GLOBAL**)malloc(sizeof(GLOBAL*));
		if(!globals){
			printf("M:CRITICAL: ALLOCATION FAILED @ %d",__LINE__);
			exit(1);
		}
		globals[0]=NULL;
	}
	
	if(globalexists(mem,start)){
		printf("!G:WARN: ");
		printlen(mem,start,length-1);
		printf(" EXISTS");
		return STATE_WARN;
	}
	
	ULONG num_globs=0;
	for(;globals[num_globs]!=NULL;num_globs++){
	}
	
	globals=(GLOBAL**)realloc(globals,sizeof(GLOBAL*)*(num_globs+2));
	if(!globals){
		printf("M:CRITICAL: ALLOCATION FAILED @ %d",__LINE__);
		exit(1);
	}
	globals[num_globs]=(GLOBAL*)malloc(sizeof(GLOBAL));
	if(!globals[num_globs]){
		printf("M:CRITICAL: ALLOCATION FAILED @ %d",__LINE__);
		exit(1);
	}
	globals[num_globs+1]=NULL;
	
	globals[num_globs]->value=value;
	globals[num_globs]->name=malloc((length+1)*sizeof(char));
	if(!globals[num_globs]->name){
		printf("M:CRITICAL: ALLOCATION FAILED @ %d",__LINE__);
		exit(1);
	}
	strncpy(globals[num_globs]->name,mem+start,length-1);
	
	globals[num_globs]->name[length-1]=0; //ensure proper termination. fixes GCC output issues
	//trim trailing spaces
	ULONG i;
	for(i=strlen(globals[num_globs]->name)-1;globals[num_globs]->name[i]==' ';i--){
		globals[num_globs]->name[i]=0;
	}
	printf(" G:STORE: %s=0x%lX",globals[num_globs]->name,globals[num_globs]->value);
	return 0;
}

ULONG getparam(char* mem,ULONG start){
	//skip blanks
	for(;mem[start]==' ';start++){
	}
	//find next blank (skip op)
	for(;mem[start]!=' ';start++){
		if(mem[start]==0||mem[start]=='\n'){
			return 0;
		}
	}
	start++;
	
	ULONG end=start;
	for(;mem[end]!='\n'&&mem[end]!=0;end++){
	}
	
	if(mem[start]=='$'||isdigit(mem[start])){
		return parseint(mem,start,end);
	}
	
	if(globalexists(mem,start)){
		return(globalvalue(mem,start));
	}
	
	printf("[P:ERR: NOPARAM] ");
	
	return 0;
}


ULONG getopcode(char* mem, ULONG start){
	//skip blank
	for(;mem[start]==' ';start++){
	}
	if(mem[start]==0||mem[start]=='\n'){
		return 0;
	}
	
	int len=0;
	for(;mem[start+len]!=' '&&mem[start+len]!=0&&mem[start+len]!='\n';len++){
	}
	
	//printf(" [OPLEN %d] ",len);
	if(len>=2){
		if(!strncmp(mem+start,"LD",2)){
			switch(mem[start+2]){
				case 'C'://LDC
					return 0;
				case 'V'://LDV
					return 1;
				case 'I'://LDIV
					return 0xA;
			}
		}
		
		if(!strncmp(mem+start,"ST",2)){
			switch(mem[start+2]){
				case 'V'://STV
					return 2;
				case 'I'://STIV
					return 0xB;
			}
		}
		
		if(mem[start]=='A'){
			switch(mem[start+1]){
				case 'D'://ADD
					return 3;
				case 'N'://AND
					return 4;
			}
		}
		
		if(mem[start]=='O'){//OR
			return 5;
		}
		
		if(mem[start]=='X'){//XOR
			return 6;
		}
		
		if(mem[start]=='E'){//EQL
			return 7;
		}
		
		if(!strncmp(mem+start,"JI",2)){
			return 13;
		}
		
		if(!strncmp(mem+start,"JM",2)){
			switch(mem[start+2]){
				case 'P'://JMP
					return 8;
				case 'N'://JMN
					return 9;
				case 'S'://JMS
					return 12;
			}
		}
		
		if(mem[start]=='H'&&len>3){//HALT
			return 0xF0;
		}
		
		if(mem[start]=='N'){//NOT
			return 0xF1;
		}
		
		if(mem[start]=='R'){//RAR
			return 0xF2;
		}
	}
	
	printf(" [NOP %d, %c] ",len,mem[start]);	
	return 0;
}
 
ULONG parseint(char* mem,ULONG start,ULONG end){
	int base=10;
	for(;start<end;start++){
		switch(mem[start]){
			case '$':
				base=16;
				break;
			case 0:
			case '\n':
				return 0;
				break;
			case ' ':
				break;
			default:
				return strtol(mem+start,NULL,base);
				break;
		}
	}
	return 0;
}

ULONG label(char* mem, ULONG start){
	ULONG _start=start;
	for(;mem[start]!=0;start++){
		switch(mem[start]){
			case '\n':
				return 0;
			case ':':
			case '=':
				return start-_start+1;
			default:
				break;
		}
	}
	return 0;
}

int main(int argc, char* argv[]){
	if(argc<2){
		printf("No input file. Usage: mimasm <infile> [<outfile>]\n");
		printf("Standard output file is out.mima\n");
		return 0;
	}
	
	printf("M:INIT: Opening %s for mode rb\n",argv[1]);
	
	FILE *input,*output;
	long filesize;
	ULONG linecount=1;
	char* buffer=NULL;
	ULONG buflen=0;
	int inBuf;
	char* outfile="out.mima";
	bool comment=false,hadspace=false;
	
	if(argc>2){
		outfile=argv[2];
	}
	
	input=fopen(argv[1],"rb");
	if(!input){
		printf("M:ERR: Could not open input file\n");
		return 1;
	}
	
	output=fopen(outfile,"wb");
	if(!output){
		fclose(input);
		printf("M:ERR: Could not open output file\n");
		return 1;
	}
	
	fseek(input,0,SEEK_END);
	filesize=ftell(input);
	rewind(input);
	
	printf("M:INFO: File is %ld bytes long\n",filesize);
	
	if(filesize<2){
		printf("M:INFO: Invalid input, aborting\n");
	}
	else{
		buffer=malloc(filesize+1);
		
		while(!feof(input)){
			inBuf=fgetc(input);
			switch(inBuf){
				case ';':
					comment=true;
					break;
				case ' ':
				case '\t':
					if(!hadspace&&!comment&&(buflen>1&&*(buffer+buflen-1)!='\n')){
						*(buffer+buflen++)=' ';
					}
					hadspace=true;
				case -1://fixes missing newline bug
				case '\r':
					break;
				case '\n':
					//if(!hadnewline&&(buflen>1&&*(buffer+buflen-1)!='\n')){
						*(buffer+buflen++)='\n';
						linecount++;
					//}
					comment=false;
					break;
				
				default:
					hadspace=false;
					if(!comment){
						*(buffer+buflen++)=inBuf;
					}
					break;
			}
			*(buffer+buflen+1)=0;
		}	
		printf("M:INFO: Read %lu sentences\n",linecount);
		
		ULONG i=0;
		ULONG currentmem=0;
		ULONG warns=0;
		char memstring[20],bufstring[20];
		
		printf("M:INFO: Building global table...\n");
		while(i<buflen-1){
			ULONG sent_start=i;
			ULONG sent_end=i;
			for(;sent_end<buflen-1;sent_end++){
				if(buffer[sent_end]=='\n'){
					break;
				}
			}
			
			ULONG slen=sent_end-sent_start;
			
			//detect directives
				if(buffer[sent_start]=='*'){
					if(slen>=3){
						//to int
						for(;buffer[sent_start]!='=';sent_start++){
						}
						currentmem=MIMAPARAM(parseint(buffer,sent_start+1,sent_end));
						printf(" M:INFO: ORG to 0x%lX\n",currentmem);
						currentmem--;
					}
				}
				else{
					ULONG labelpos=label(buffer,sent_start);
					if(labelpos>0){
						printf(" [%c]",buffer[sent_start+labelpos-1]);
						if(buffer[sent_start+labelpos-1]=='='){
							switch(globalcreate(buffer,sent_start,parseint(buffer,sent_start+labelpos,sent_end))){
								case STATE_WARN:
									warns++;
									break;
								case STATE_ERR:
									break;
							}
							currentmem--;
						}
						else{
							switch(globalcreate(buffer,sent_start,currentmem)){
								case STATE_WARN:
									warns++;
									break;
								case STATE_ERR:
									break;
							}
						}
						printf("\n");
					}
				}

			currentmem=MIMAPARAM(currentmem+1);
			i=sent_end+1;
		}
		printf("M:INFO: Global Table built with %lu warnings\n",warns);
		
		currentmem=0;
		i=0;
		//parse
		
		printf("M:INFO: Parsing\n");
		while(i<buflen-1){
			sprintf(memstring,"0x%05lX ",currentmem);
			ULONG sent_start=i;
			ULONG sent_end=i;
			for(;sent_end<buflen-1;sent_end++){
				if(buffer[sent_end]=='\n'||buffer[sent_end]==0){
					break;
				}
			}
			
			printf(" P:LINE: %lu - %lu: ",sent_start,sent_end);
			ULONG slen=sent_end-sent_start;
			
			//parse here
				//detect orgs & metas
				if(buffer[sent_start]=='*'){
					if(slen>=3){
						//to int
						for(;buffer[sent_start]!='=';sent_start++){
						}
						currentmem=MIMAPARAM(parseint(buffer,sent_start+1,sent_end));
						printf("ORG to 0x%lX\n",currentmem);
						currentmem--;
					}
				}
				//detect blanks => LDC 0
				else if(sent_start==sent_end){
					fputs(memstring,output);
					fputs("0x000000\n",output);
					printf("BLANK -> LDC 0\n");
				}
				
				else{
					//handle labelling
					ULONG labelpos=label(buffer,sent_start);
					if(labelpos>0){
						if(buffer[sent_start+labelpos-1]=='='){
							//ignore line
							printf("SKIP [META]\n");
							i=sent_end+1;
							continue;
						}
						else{
							printf("[LABEL@+%lu] ",labelpos);
						}
					}
					
					sent_start+=labelpos;
					sent_start+=(buffer[sent_start]==' ')?1:0;
					
					//TODO if now blank, ldc 0 //should work
		
					if(buffer[sent_start]=='D'&&buffer[sent_start+1]=='S'){
						//handle DS
						ULONG storage=parseint(buffer,sent_start+2,sent_end);
						fputs(memstring,output);
						sprintf(bufstring,"0x%06lX",MIMAWORD(storage));
						fputs(bufstring,output);
						if(labelpos>0){
							fputs(" ;",output);
							fwrite(buffer+(sent_end-slen),labelpos-1,1,output);
						}
						fputc('\n',output);
						printf("DS 0x%06lX\n",MIMAWORD(storage));
					}
					else{
						//parse opcode
						ULONG opcode=getopcode(buffer,sent_start);
						
						printf("OP 0x%lX ",opcode);
						fputs(memstring,output);
						
						if(opcode<=0xF){
							//parse parameter
							ULONG parameter=getparam(buffer,sent_start);
							
							printf("PARAM 0x%05lX",MIMAPARAM(parameter));
							sprintf(bufstring,"0x%lX%05lX",opcode,MIMAPARAM(parameter));
							fputs(bufstring,output);
						}
						else{
							sprintf(bufstring,"0x%06lX",(opcode<<16));
							fputs(bufstring,output);
						}
						
						printf(" <= ");		
						printlen(buffer,sent_start,sent_end-sent_start+(buffer[sent_end]=='\n'?0:1));//weird construct, but fixes an output bug
						
						if(labelpos>0){
							fputs(" ;",output);
							fwrite(buffer+(sent_end-slen),labelpos-1,1,output);
						}
						fputc('\n',output);
						putc('\n',stdout);
					}
				}
				
			i=sent_end+1;
			currentmem=MIMAPARAM(currentmem+1);
		}
		printf("M:INFO: Parser done\n");
	}
	printf("M:INFO: Cleaning up\n");
	
	
	freeglobals();
	free(buffer);
	fclose(input);
	fclose(output);
	return 0;
}
