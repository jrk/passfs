/*
This module handles the parameter analysis for the file system.
Note specifically the enum and the fuse opt struct below which determine the available parameters.
*/

#include "fsname.h"        /* this defines the name and version of the filesystem */
/* standard clib stuff */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>        /* unix stuff */

#include <fuse.h>          /* FUSE stuff */

#include "userModeFS.h"    /*interfaces relating to the main file system module */
#include "stats.h"         /*interfaces relating to stats module */
#include "debug.h"         /*interfaces relating to the debug option */
/* This module borrowed from Radek Podgorny unionfs-fuse  with customisations by JC*/
int use_readir_method2;
int doexit;
char* root;
/* this struct demonstrates the use of a structure to store automatically parsed option data (it doesn't actually have a useful function in this code)*/
struct passFSData{unsigned long intval;char *stringval;}optData;
/* an enumeration to generate the values for keys in the options structure */
enum {
	KEY_STATS,        /*the stats file option -ostats */
	KEY_HELP,         /*the fs help flag -h */
	KEY_FUSE_HELP,    /*the fuse help flag -H */
	KEY_VERSION,      /*the fs and fuse version flag -V */
	KEY_DEBUG,        /*the debug flag -d */
	KEY_MONITOR,      /*the monitor flag -m */
	KEY_MONITOR_FILE, /*the monitor file value -m= */
	KEY_DIR_METHOD2,  /*the read dir method flag -D */
	KEY_DEMO_INT,     /*the demo integer value -i=%lu */
	KEY_DEMO_STRING,  /*the demo string value -s=%s */
	KEY_DEMO_SPACE    /*the demo flag followed by value -n */
};


/* this is the options structure */
static struct fuse_opt userModeFS_opts[] = {
	FUSE_OPT_KEY("--help", KEY_HELP),
	FUSE_OPT_KEY("--version", KEY_VERSION),
	FUSE_OPT_KEY("-h", KEY_HELP),
	FUSE_OPT_KEY("-H", KEY_FUSE_HELP),
	FUSE_OPT_KEY("-V", KEY_VERSION),
	FUSE_OPT_KEY("stats", KEY_STATS),
	FUSE_OPT_KEY("-d", KEY_DEBUG),
	FUSE_OPT_KEY("-m",KEY_MONITOR),
	FUSE_OPT_KEY("-m=",KEY_MONITOR_FILE),
	FUSE_OPT_KEY("-D",KEY_DIR_METHOD2),
/* the next entries are for demonstration purposes only: they have no useful function*/
	/*-x value form*/
	FUSE_OPT_KEY("-n ",KEY_DEMO_SPACE),
/* note the next two examples do not need additional code to implement them in the 
   option call back routine */
	/*-x=%s form*/
	{ "-s=%s", (unsigned long)offsetof(struct passFSData,stringval),KEY_DEMO_STRING },
	/*-x=%lu form*/
	{ "-i=%lu", offsetof(struct passFSData,intval), KEY_DEMO_INT },
	FUSE_OPT_END
};

/*service routines */
static char *make_absolute(const char *name) {/*
make the given name an absolute path (if it is not already) by prefixing the 
current working directory to it. Store the new path (prefixed or not) on the heap 
and return a pointer to it, will return NULL if unsuccessful.
*/
	char cwd[PATHLEN_MAX+1];
	size_t cwdlen = 0;
	size_t nameLen = strlen(name);
	char *newName=NULL;

	if (*name != '/') {/*get current working directory (if name not absolute) */
		if (!getcwd(cwd, PATHLEN_MAX)) {
			perror("Unable to get current working directory");
			return NULL;
		} else {
			cwdlen = strlen(cwd);
			cwd[cwdlen++]='/';                 /* put / at end of cwd */
		}
	}
	if((cwdlen+nameLen+2)>PATHLEN_MAX)printf("Path greater than maximum name size (%i)\n",PATHLEN_MAX);
	else {
		newName = (char *)malloc(cwdlen + nameLen + 2);
		if(newName){                          /*make sure malloc was ok*/
			if (cwdlen) strcpy (newName, cwd);  /*prefix by cwd/ */
			strcpy(&newName[cwdlen],name);      /*copy rest of name */
		}
		else printf("Insufficient memory when allocating space for path\n");
	}
	return newName;
}
/* end service routines */

/* the parameter analysis call back procedure */
int userModeFS_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs) {/*
return -1 to indicate error
        0 to accept parameter
        1 to retain parameter and pase to FUSE
*/
	switch (key) {
		case FUSE_OPT_KEY_NONOPT:
			if (!root) {
				root = make_absolute(arg); /*make a copy prefixed by CWD if necessary*/
				return 0;
			}
			return 1;
		case KEY_STATS:
			stats_enabled = 1;
			return 0;
		case KEY_FUSE_HELP: /* -H get help and fuse help */
			fuse_opt_add_arg(outargs, "-ho"); /* add to the args that FUSE will see */
		case KEY_HELP:
			fprintf (stderr,
			userFSnameStr "-fuse version " userFSverStr "\n"
			"by John Cobb <j.c.cobb@qmul.ac.uk>\n"
			"with borrowings from Radek Podgorny\n"
			"using the FUSE Filesystems in user space support\n"
			"\n"
			"Usage: %s [options] root_path mountpoint\n"
			"The first argument is the directory to form the root of the filesystem\n"
			"\n"
			"general options:\n"
			"    -o opt,[opt...]        mount options\n"
			"    -h   --help            print this help\n"
			"    -H                     print additional help\n"
			"    -V   --version         print version\n"
			"\n"
			"options specific to " userFSnameStr " :\n"
			"    -m                     monitor to standard output"
			"    -m=file                monitor to file"
			"    -D                     implement use of offset in readdir interface"
			"    -o stats               show statistics in the file 'stats' under the mountpoint\n"
			"for other options use -H\n"
			"\n",
			outargs->argv[0]);
			doexit = 1;
			return 0; /*accept but do not retain argument */

		case KEY_VERSION: /* -V print filesystem version and cause FUSE to print its version */
			printf(userFSnameStr " version: " userFSverStr "\n");
			doexit = 1;
			return 1;      /* retain -V so that FUSE sees it */
		case KEY_DEBUG:  /* -d debug option */
			debug_init(1);
			return 1;      /* retain -d so that FUSE sees it */
		case KEY_MONITOR:
			monitorInit(NULL);
			printf("\nmonitor in forground");
			fuse_opt_add_arg(outargs, "-f"); /*force foreground operation*/
			return 0;
		case KEY_DIR_METHOD2:  /*-D use the offset style of working in readdir interface */
			use_readir_method2=1;
			return 0;
		case KEY_MONITOR_FILE:
			{
				const char *fp=&arg[3];
				while(fp[0]==' ')fp++;
				printf("\nmonitor to file:%s",fp);
				if(monitorInit(fp)){
					printf(" failed to open monitor file\n");
					return -1;
				}
			}
			return 0;
		case KEY_DEMO_SPACE:
			printf("demonstration parameter -n value=%s\n",arg);
			return 0;
		default:
			doexit=1; /*if we don't recognise it assume it is for fuse_main */
			return 1;
	}
}
/* The entry point for the file system */
int main(int argc, char *argv[]) {
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	int i;
	int res = debug_init(0); /* start with debug off */
	if (res != 0) return res;
	/*initialise values */
	stats_init();
	optData.intval=0;
	optData.stringval=NULL;
	doexit = 0;
	use_readir_method2=0;
	root=NULL;
	/*initiate parameter analysis */
	if(fuse_opt_parse(&args,(void *)&optData,userModeFS_opts,userModeFS_opt_proc)==-1) res=1;
	else {
		printf("\narguments to fuse_main: ");
		for(i=0;i<args.argc;i++)printf("%s ",args.argv[i]);
		printf("\n");
		printf("Demo parameters: value of  -i=%lu, value of -s=",optData.intval);
		if(optData.stringval)printf("%s\n",optData.stringval);else printf("NULL\n");
		if (!doexit) {
			if (!root) {
				printf("You need to specify at least a root directory and a mount point !\n"
				       "try -h for more information\n");
				res=1;
			}
		}
	}
	/*enter the filesystem  module */
	if(!res)res= userFSMain(&args,use_readir_method2);
	/*tidy up */
	fuse_opt_free_args(&args);
	if(root)free(root);
	return res;
}
