#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>

#ifndef _WIN32
	#include <termios.h>
	#include <unistd.h>

	//this emulates conio.h's getch().
	//via http://cboard.cprogramming.com/faq-board/27714-faq-there-getch-conio-equivalent-linux-unix.html
	
	int _getch(void){
		struct termios canon,raw;
		int ch;
		tcgetattr(STDIN_FILENO,&canon);
		raw=canon;
		raw.c_lflag&=~(ICANON|ECHO);
		tcsetattr(STDIN_FILENO,TCSANOW,&raw);
		ch=getchar();
		tcsetattr(STDIN_FILENO,TCSANOW,&canon);
		return ch;
	}

	#define _GETCH_ _getch
#else
	#define _GETCH_ getch
#endif

//change this to expand to (a) to see list debug messages.
//Will also pretty much make the output unreadable
#define LISTDEBUG(a)

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

FJS 2012-2014
*/

#define OPCODE(a) (((a)&0xF00000)>>20)
#define EXTOPCODE(a) (((a)&0xF0000)>>16)
#define PARAMETER(a) ((a)&0xFFFFF)
#define MIMAWORD(a) ((a)&0xFFFFFF)

#define ULONG unsigned

typedef struct _MEMCELL {
	struct _MEMCELL* next;
	struct _MEMCELL* prev;
	char* name;
	ULONG mempos;
	ULONG value;
} MEMCELL;

struct /*GLOBAL_OPTS*/ {
	bool verbose;
	bool dumpverbose;
	ULONG maxsteps;
	bool breakpoints;
} OPTIONS;

struct /*MEM*/ {
	MEMCELL bottom;
	MEMCELL* top;
} MEMORY;

struct /*MIMA*/{
	MEMCELL* ir;
	ULONG iar;
	ULONG akku;
	ULONG steps;
	bool running;
} MIMA;

FILE *input=NULL,*output=NULL;

//Linked List Prototypes
void setcell(ULONG, ULONG); //set cell to value
MEMCELL* getcell(ULONG); //get cell
void freemem(); //free list

/*
Used some kind of special "always-sorted weirdly
pseudo-optimized" linked list because most programs
don't need the entire memory to be allocated at any
point in execution. Also I liked the challenge of
implementing it.
*/

void printstate(char*,bool,MEMCELL*); //pretty-print cell name, address and value

void printstate(char* cmd,bool deref,MEMCELL* modified){
	ULONG param=PARAMETER(MIMA.ir->value);
	MEMCELL* cell;
	
	if(deref){
		cell=getcell(param);
	}
	
	printf("[%5d] [@0x%06X] ",MIMA.steps,MIMA.ir->mempos);
	
	if(MIMA.ir->name&&OPTIONS.verbose){
		printf("[%s] ",MIMA.ir->name);
	}
	else{
		printf("\t");
	}
	
	printf(cmd);
	if(output){
		//steps & label
		fprintf(output," %8d  %10s ",MIMA.steps,(OPTIONS.verbose&&MIMA.ir->name)?MIMA.ir->name:"");
		//ir & op
		fprintf(output," 0x%06X  %6s ",MIMA.ir->value,cmd);
		
		if(deref&&(!OPTIONS.verbose||(OPTIONS.verbose&&cell->name==NULL))||(!deref&&modified==NULL)){
			fprintf(output,"   0x%06X ",param);
		}
		else{
			fprintf(output," %10s ",(deref&&OPTIONS.verbose&&cell->name!=NULL)?cell->name:"");
		}
		
		fprintf(output," 0x%06X  0x%06X ",MIMA.akku,MIMA.iar);
		
		if(modified&&deref){
			if(modified->name&&OPTIONS.verbose){
				fprintf(output," %s",modified->name);
			}
			else{
				fprintf(output," 0x%06X",modified->mempos);
			}
			fprintf(output," = 0x%06X",modified->value);
		}
		fputs("\r\n",output);
	}
	
	if(!deref){
		if(modified!=NULL){
			printf("\n");
		}
		else{
			printf("0x%06X\n",param);
		}
	}
	else if(OPTIONS.verbose&&cell->name){
		printf("%s (0x%06X)\n",cell->name,cell->value);
	}
	else{
		printf("0x%06X(@0x%06X)\n",cell->mempos,cell->value);
	}
}

MEMCELL* getcell(ULONG pos){
	LISTDEBUG(printf(" GET 0x%X ",pos));
	if(pos>MEMORY.top->mempos){
		LISTDEBUG(printf("INS TOP %d@0x0\n",pos));
		//create at top
		MEMCELL* new=(MEMCELL*)malloc(sizeof(MEMCELL));
		new->mempos=pos;
		new->name=NULL;
		new->value=0;
		
		//mount
		new->next=NULL;
		new->prev=MEMORY.top;
		MEMORY.top->next=new;
		MEMORY.top=new;
		return new;
	}
	
	bool forward=true;
	MEMCELL* it=&MEMORY.bottom;
	
	ULONG delta=MEMORY.top->mempos-pos;
	if(delta<(MEMORY.top->mempos/2)){
		//start from top
		forward=false;
		it=MEMORY.top;
		LISTDEBUG(printf("TRAV top | "));
	}
	else{
		LISTDEBUG(printf("TRAV bot | "));
	}
	
	while(it!=NULL){
		
		if(it->mempos==pos){
			LISTDEBUG(printf("FOUND %d@0x%X\n",it->mempos,it->value));
			return it;
		}
		
		if(forward&&it->next!=NULL&&it->next->mempos>pos||!forward&&it->prev!=NULL&&it->prev->mempos<pos){
			//insert
			MEMCELL* new=(MEMCELL*)malloc(sizeof(MEMCELL));
			new->name=NULL;
			new->mempos=pos;
			new->value=0;
		
			//mount
			new->next=forward?(it->next):(it);
			new->prev=forward?(it):(it->prev);
			if(forward){
				it->next->prev=new;
				it->next=new;
			}
			else{
				it->prev->next=new;
				it->prev=new;
			}
			LISTDEBUG(printf("INS %d@0x0 OK\n",pos));
			return it;
		}
		
		it=forward?(it->next):(it->prev);
	}
	LISTDEBUG(printf("FAIL\n"));
	return NULL;
}

void setcell(ULONG pos, ULONG value){
	//just realized, this is a one-liner. ohwell, overengineering ftw. //09062012 1434
	getcell(pos)->value=value;
}

void freemem(){
	//FIXME the output in here could be done nicer
	MEMCELL* current=MEMORY.bottom.next;
	if(OPTIONS.dumpverbose||MEMORY.bottom.name){
		printf(" END: Cell 0x00000 %s%s%swas at 0x%X\n",(MEMORY.bottom.name)?"(":"",(MEMORY.bottom.name)?MEMORY.bottom.name:"",(MEMORY.bottom.name)?") ":"",MEMORY.bottom.value);
	}
	
	while(current!=NULL){
		MEMCELL* temp=current;
		
		if(OPTIONS.dumpverbose||temp->name){
			printf(" END: Cell 0x%05X %s%s%swas at 0x%X\n",temp->mempos,(temp->name)?"(":"",(temp->name)?temp->name:"",(temp->name)?") ":"",temp->value);
		}
		
		if(current->name){
			free(current->name);
		}
		
		current=current->next;		
		free((void*)temp);//FIXME wat
	}
	
	if(MEMORY.bottom.name){
		free(MEMORY.bottom.name);
	}

	return;
}

int main(int argc, char* argv[]){
	//init memory
	MEMORY.top=&MEMORY.bottom;
	MEMORY.bottom.next=NULL;
	MEMORY.bottom.prev=NULL;
	MEMORY.bottom.mempos=0;
	MEMORY.bottom.value=0;
	MEMORY.bottom.name=NULL;

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
	
	char* infile=NULL;
	char* outfile=NULL;
	bool interact=false;
	OPTIONS.verbose=false;
	OPTIONS.dumpverbose=false;
	OPTIONS.maxsteps=0;
	OPTIONS.breakpoints=false;
	ULONG entry=0;
	int a;
	
	//parse arguments
	for(a=1;a<argc;a++){
		if(*argv[a]=='-'){
			if(argv[a][1]=='i'||argv[a][1]=='I'){
				printf("Using interactive mode\n");
				interact=true;
			}
			else if(argv[a][1]=='d'||argv[a][1]=='D'){
				if(argv[a][2]=='v'||argv[a][2]=='V'){
					printf("Setting verbose dump flag\n");
					OPTIONS.dumpverbose=true;
				}
			}
			else if(argv[a][1]=='v'||argv[a][1]=='V'){
				printf("Increased verbosity\n");
				OPTIONS.verbose=true;
			}
			else if(argv[a][1]=='e'||argv[a][1]=='E'){
				if(argc>a+1){
					entry=strtoul(argv[++a],NULL,0);
					printf("Using entry point 0x%06X\n",entry);
				}
				else{
					printf("Entry point flag was specified, but had no argument.\n");
					return 1;
				}
			}
			else if(argv[a][1]=='l'||argv[a][1]=='L'){
				if(argc>a+1){
					OPTIONS.maxsteps=strtoul(argv[++a],NULL,0);
					printf("Doing %d steps at max\n",OPTIONS.maxsteps);
				}
				else{
					printf("Limit steps flag was specified, but had no argument.\n");
					return 1;
				}
			}
			else if(argv[a][1]=='b'||argv[a][1]=='B'){
				printf("Enabling breakpoint handling\n");
				OPTIONS.breakpoints=true;
			}
			else{
				printf("Unknown flag \"%s\"\n",argv[a]);
			}
		}
		else{
			if(!infile){
				printf("Using input %s\n",argv[a]);
				infile=argv[a];
			}
			else if(!outfile){
				printf("Using output %s\n",argv[a]);
				outfile=argv[a];
			}
			else{
				printf("Could not assign parameter \"%s\"\n",argv[a]);
			}
		}
	}
	
	//read input file
	ULONG filesize;
	char* currentPos;
	char* inputBuffer;
	
	input=fopen(infile,"rb");
	if(!input){
		printf("Could not open input file\n");
		exit(1);
	}

	if(outfile){
		output=fopen(outfile,"wb");
		if(!output){
			fclose(input);
			printf("Could not open output file\n");
			return 1;
		}
		fputs("Reading memory from ",output);
		fputs(infile,output);
		fputs("... ",output);
	}
	
	fseek(input,0,SEEK_END);
	filesize=ftell(input);
	rewind(input);
	
	inputBuffer=(char*)malloc(filesize+1);
	fread(inputBuffer,1,filesize,input);
	inputBuffer[filesize]=0;//terminate buffer
	currentPos=inputBuffer;
	
	while(currentPos<inputBuffer+filesize-2){
		ULONG address=PARAMETER(strtoul(currentPos,&currentPos,0));
		ULONG value=MIMAWORD(strtoul(currentPos,&currentPos,0));
		//skip leading
		for(;*currentPos!=';'&&*currentPos!='\n'&&*currentPos!=0;currentPos++){
		}
		
		setcell(address,value);
		
		if(*currentPos==';'){
			currentPos++;
			ULONG clen=0;
			for(;currentPos[clen]!=0&&currentPos[clen]!='\n';clen++){
			}
			//read comment
			//store
			MEMCELL* cur=getcell(address);
			cur->name=(char*)malloc(clen+1);
			strncpy(cur->name,currentPos,clen);
			cur->name[clen]=0;
			currentPos+=clen+1;
		}
	}
	
	if(outfile){
		fputs(" OK\r\n",output);
		fprintf(output,"[%8s][%10s][%8s][%6s][%10s][%8s][%8s][%-20s]\r\n","STEP","LABEL","IR   ","OP  ","PARAMETER","AKKU  ","IAR  "," MEM");
	}
	
	//simulation code goes here
	MIMA.iar=entry;
	MIMA.running=true;
	MIMA.steps=1;
	
	MEMCELL* temp;
	ULONG buf=0;

	if(interact){
		printf("Interactive mode commands:\n");
		printf("\tq\t\tstop execution\n");
		printf("\ta\t\tprint accumulator\n");
		printf("\ti\t\tprint iar\n");
		printf("\tm\t\tquery memory contents\n");
		printf("\tc\t\tcontinue execution\n");
		printf("\tn|<retn>\texecute next step\n");
	}
	
	while(MIMA.running){
		if(interact){
			bool next=false;
			bool quit=false;
			//interactive shell
			while(!next){
				printf("> ");
				switch(_GETCH_()){
					case 'm':
					case 'M':
						printf("Addr ?> 0x");
						unsigned addr=0;
						fflush(stdin);
						scanf("%x",&addr);
						MEMCELL* temp=getcell(addr);
						printf("0x%06X @ 0x%06X\n",temp->mempos,temp->value);
						break;
				
					case 'q':
					case 'Q':
						printf("\n");
						next=true;
						quit=true;
						break;
					
					case 'n':
					case 'N':
					case 13:
					case 10:
						//printf("\n");
						next=true;
						break;
					
					case 'a':
					case 'A':
						printf("Akku: 0x%06X\n",MIMA.akku);
						break;
						
					case 'i':
					case 'I':
						printf(" IAR: 0x%06X\n",MIMA.iar);
						break;

					case 'c':
					case 'C':
						printf("\n");
						interact=false;
						next=true;
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
		
		MIMA.ir=getcell(MIMA.iar++);
		switch(OPCODE(MIMA.ir->value)){
			case 0x0://LDC
				MIMA.akku=PARAMETER(MIMA.ir->value);
				printstate("LDC ",false,NULL);
				break;
			case 0x1://LDV
				temp=getcell(PARAMETER(MIMA.ir->value));
				MIMA.akku=temp->value;
				printstate("LDV ",true,NULL);
				break;
			case 0x2://STV
				setcell(PARAMETER(MIMA.ir->value),MIMA.akku);
				printstate("STV ",true,getcell(PARAMETER(MIMA.ir->value)));
				break;
			case 0x3://ADD
				temp=getcell(PARAMETER(MIMA.ir->value));
				MIMA.akku=MIMAWORD(MIMA.akku+temp->value);
				printstate("ADD ",true,NULL);
				break;
			case 0x4://AND
				temp=getcell(PARAMETER(MIMA.ir->value));
				MIMA.akku&=temp->value;
				printstate("AND ",true,NULL);
				break;
			case 0x5://OR
				temp=getcell(PARAMETER(MIMA.ir->value));
				MIMA.akku|=temp->value;
				printstate("OR ",true,NULL);
				break;
			case 0x6://XOR
				temp=getcell(PARAMETER(MIMA.ir->value));
				MIMA.akku^=temp->value;
				printstate("XOR ",true,NULL);
				break;
			case 0x7://EQL
				temp=getcell(PARAMETER(MIMA.ir->value));
				MIMA.akku=(MIMA.akku==temp->value)?MIMAWORD(-1):0;
				printstate("EQL ",true,NULL);
				break;
			case 0x8://JMP
				MIMA.iar=PARAMETER(MIMA.ir->value);
				printstate("JMP ",true,NULL);
				break;
			case 0x9://JMN
				if((MIMA.akku&0x800000)!=0){
					MIMA.iar=PARAMETER(MIMA.ir->value);
				}
				printstate("JMN ",true,NULL);
				break;
			case 0xA://LDIV
				temp=getcell(PARAMETER(MIMA.ir->value));
				MIMA.akku=getcell(temp->value)->value;
				printstate("LDIV ",true,NULL);
				break;
			case 0xB://STIV
				temp=getcell(PARAMETER(MIMA.ir->value));
				setcell(temp->value,MIMA.akku);
				printstate("STIV ",true,getcell(temp->value));
				break;
			case 0xC://JMS
				setcell(PARAMETER(MIMA.ir->value),MIMA.iar);
				MIMA.iar=PARAMETER(MIMA.ir->value)+1;
				printstate("JMS ",true,getcell(PARAMETER(MIMA.ir->value)));
				break;
			case 0xD://JIND
				temp=getcell(PARAMETER(MIMA.ir->value));
				MIMA.iar=temp->value;
				printstate("JIND ",true,NULL);
				break;
			case 0xF:
				switch(EXTOPCODE(MIMA.ir->value)){
					case 0x0://HALT
						printstate("HALT ",false,(void*)!NULL);//FIXME ugly
						if(!OPTIONS.breakpoints){
							MIMA.running=false;
						}
						else{
							printf("M: Breakpoint hit, going interactive. Press 'c' to resume.\n");
							interact=true;
						}
						break;
					case 0x1://NOT
						MIMA.akku=MIMAWORD(~MIMA.akku);
						printstate("NOT ",false,(void*)!NULL);
						break;
					case 0x2://RAR
						buf=MIMA.akku&1;
						MIMA.akku>>=1;
						buf<<=23;
						MIMA.akku|=buf;
						printstate("RAR ",false,(void*)!NULL);
						break;
					default://FAIL
						printf("OP %X, EXOP %X ",OPCODE(MIMA.ir->value),EXTOPCODE(MIMA.ir->value));
						printstate("FAIL ",false,(void*)!NULL);
						break;
				}
				break;
		}
		if(MIMA.steps==OPTIONS.maxsteps){//wont fire if 0
			printf("> Reached allowed step limit, aborting\n");
			break;
		}
		MIMA.steps++;
	}
	
	if(interact){
		printf("> Execution stopped\n");
		_GETCH_();
	}
	
	freemem();
	fclose(input);
	if(output){
		fclose(output);
	}
	free(inputBuffer);
	return 0;
}
