#ifndef DEBUG_H
#define DEBUG_H



	extern FILE* dbgfile;
	//#define DBG(x) fprintf(dbgfile, "%s\n", x);
	#define DBG(x) if(dbgfile){printf("%s\n", x);}


int debug_init(int debug);


#endif
