#include <stdio.h>
#include "fsname.h"
#include "debug.h"

/* This module borrowed from Radek Podgorny unionfs-fuse */
FILE* dbgfile = NULL;

int debug_init(int debug) {
	if(debug){
		char *dbgpath = "./" userFSnameStr "_debug.log";
		printf("Debug mode, log will be written to %s\n", dbgpath);

		dbgfile = fopen(dbgpath, "w");
		if (!dbgfile) {
			printf("Failed to open %s for writing, exiting\n", dbgpath);
			return 2;
		}
	}
	else if(dbgfile){
		fclose(dbgfile);
		dbgfile=NULL;
	}

	return 0;
}
