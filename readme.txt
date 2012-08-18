The purpose of this code is to be an example of the use of the FUSE system.
FUSE allows filesystems to be written and run in user space as opposed to the Kernel.

As such it implements a 'pass through' file system that simply takes a directory
and treats it as the root of a filing system that it then makes available on a given mount mount. All file operations are simply executed unchanged. 

It has no purpose except to demonstrate the features of FUSE and possibly to be a template.

You compile it by typeing ./build.sh

Some notes on structure

header files
------ -----
fsname.h       contains the name and version of the file system (in this case passfs)
userModeFS.h   is the main header file
other .h files are headers relating to the corresponding .c modules

c source files
passfs.c      contains the call back procedures that actually implement the file system.
opts.c        contains the main procedure and the call back procedure that handles
              options specific to the passfs file system. It defines the option templates.
debug.c       initialises the debug output, debug.h define the debug macros.
status.c      implements the stats system.