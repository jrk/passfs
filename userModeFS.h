#ifndef USERMODEFS_H
#define USERMODEFS_H


#define PATHLEN_MAX 1024

char *root;
int monitorInit(const char *file);
int userFSMain(struct fuse_args *args,int use_readir_method2);
#endif
