/*
This code uses code written by Radek Podgorny for his unionfs-fuse as a template It retains his stats,debug and opts modules as useful service routines and follows his startup structure.

The initial aim is to produce a more useful template than the xmpl code that comes with the FUSE package. The template just acts as a pass through fileing system. you just give it 
a directory and it treats that as the root of a fileing system. Not intrinsically very useful. However the code demonstrates the use of some of the parameter
analysis interfaces and can be used as a simple monitor.

John Cobb

Written by Radek Podgorny

This is offered under a BSD-style license. This means you can use the code for whatever you desire in any way you may want but you MUST NOT forget to give me appropriate credits when spreading your work which is based on mine. Something like "original implementation by Radek Podgorny" should be fine.
*/
#include "fsname.h"
#ifdef linux
	/* For pread()/pwrite() */
	#define _XOPEN_SOURCE 500
#endif

#include <fuse.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/statvfs.h>

#ifdef HAVE_SETXATTR
	#include <sys/xattr.h>
#endif

#ifdef F_FULLFSYNC
/* this is a Mac OS X system which does not implement fdatasync as such */
#define fdatasync(f) fcntl(f, F_FULLFSYNC)
#endif

#include "userModeFS.h"

#include "stats.h"
#include "debug.h"
int monitor=0;
FILE *monitor_file=NULL;


static void mprintf(const char *format,...) {

	va_list vp;
	
	if(monitor&2){va_start(vp,format);vfprintf(stdout,format,vp);va_end(vp);}
	if(monitor_file){va_start(vp,format);vfprintf(monitor_file,format,vp);va_end(vp);}
}
int monitorInit(const char *file)
{

	if(file){
		monitor_file=fopen(file,"w");
		if(monitor_file)monitor|=1;
		else return 1;
	}
	else {
		monitor|=2;
	}
	return 0;
}
static int userModeFS_access(const char *path, int mask) {
	DBG("access\n");


	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", root, path);
	if(monitor)mprintf("access %s,mask=%x",path,mask);
	int res = access(p, mask);
	if (res == -1) {
		if(monitor)mprintf(" res=%x\n",errno);
		return -errno;
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

static int userModeFS_chmod(const char *path, mode_t mode) {
	DBG("chmod\n");


	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", root, path);
	if(monitor)mprintf("chmod %s,mode=%x",path,mode);
	int res = chmod(p, mode);
	if (res == -1) {
		if(monitor)mprintf(" res=%x\n",errno);
		return -errno;
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

static int userModeFS_chown(const char *path, uid_t uid, gid_t gid) {
	DBG("chown\n");

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", root, path);
	if(monitor)mprintf("chown %s,uid=%x,gid=%x",path,uid,gid);
	int res = lchown(p, uid, gid);
	if (res == -1) {
			if(monitor)mprintf(" res=%x\n",errno);
			return -errno;
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

/* flush may be called multiple times for an open file, this must not really close the file. This is important if used on a network filesystem like NFS which flush the data/metadata on close() */
static int userModeFS_flush(const char *path, struct fuse_file_info *fi) {
	DBG("flush\n");

	if (stats_enabled && strcmp(path, STATS_FILENAME) == 0) return 0;

	int fd = dup(fi->fh);
	if(monitor)mprintf("flush %s",path);
	if (fd == -1) {
		// What to do now?
		if (fsync(fi->fh) == -1) {
			if(monitor)mprintf(" res=%x\n",EIO);
			return -EIO;
		}
		if(monitor)mprintf(" res=FAIL\n");
		return 0;
	}

	if (close(fd) == -1){
		if(monitor)mprintf(" res=%x\n",errno);
		return -errno;
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

/* Just a stub. This method is optional and can safely be left unimplemented */
static int userModeFS_fsync(const char *path, int isdatasync, struct fuse_file_info *fi) {
	DBG("fsync\n");

	if (stats_enabled && strcmp(path, STATS_FILENAME) == 0) return 0;

	int res;
	if(monitor)mprintf("fsync %s, isdata",path,isdatasync);
	if (isdatasync) {
		res = fdatasync(fi->fh);
	} else {
		res = fsync(fi->fh);
	}

	if (res == -1) {
		if(monitor)mprintf(" res=%x\n",errno);
		return -errno;
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

static int userModeFS_getattr(const char *path, struct stat *stbuf) {
	DBG("getattr\n");

	if (stats_enabled && strcmp(path, STATS_FILENAME) == 0) {
		memset(stbuf, 0, sizeof(stbuf));
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = STATS_SIZE;
		return 0;
	}

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", root, path);
	if(monitor)mprintf("getattr %s",path);
	int res = lstat(p, stbuf);
	if (res == -1) {
		res=errno;
		if(monitor)mprintf(" res=%x\n",res);
		return -res;
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}


static int userModeFS_link(const char *from, const char *to) {
	DBG("link\n");


	char t[PATHLEN_MAX],p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", root, from);
	snprintf(t, PATHLEN_MAX, "%s%s", root, to);
	if(monitor)mprintf("link from:%s, to:%s",from,to);
	int res = link(p, t);
	if (res == -1) {
		res=errno;
		if(monitor)mprintf(" res=%x\n",res);
		return -res;
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

static int userModeFS_mkdir(const char *path, mode_t mode) {
	DBG("mkdir\n");


	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", root, path);

	if(monitor)mprintf("make dir: %s,mode=%x",path,mode);
	int res = mkdir(p, mode);
	if (res == -1) {
		res=errno;
		if(monitor)mprintf(" res=%x\n",res);
		return -res;
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

static int userModeFS_mknod(const char *path, mode_t mode, dev_t rdev) {
	DBG("mknod\n");

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", root, path);
	if(monitor)mprintf("make node: %s, mode=%x, dev=%x",path,mode,rdev);
    #ifdef __APPLE__
    #warning "Substituting creat for mknod - limited functionality"
    int res = creat(p, mode);
    #else
	int res = mknod(p, mode, rdev);
    #endif
	if (res == -1) {
		res=errno;
		if(monitor)mprintf(" res=%x\n",res);
		return -res;
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

static int userModeFS_open(const char *path, struct fuse_file_info *fi) {
	DBG("open\n");

	if(monitor)mprintf("open: %s,flags=%x,",path,fi->flags);
	if (stats_enabled && strcmp(path, STATS_FILENAME) == 0) {
		if ((fi->flags & 3) == O_RDONLY) {
			fi->direct_io = 1;
		}
		else {
			if(monitor)mprintf(" res=%x\n",EACCES);
			return -EACCES;
		}
	}
	else {
		char p[PATHLEN_MAX];
		snprintf(p, PATHLEN_MAX, "%s%s", root, path);

		int fd = open(p, fi->flags);
		if (fd == -1) {
			int res=errno;
			if(monitor)mprintf(" res=%x\n",res);
			return -res;
		}
		else {
			fi->direct_io = 1;
			fi->fh = (unsigned long)fd;
		}
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

static int userModeFS_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	DBG("read\n");

	if (stats_enabled && strcmp(path, STATS_FILENAME) == 0) {
		char out[STATS_SIZE] = "";
		stats_sprint(out);

		int s = size;
		if (offset < strlen(out)) {
			if (s > strlen(out)-offset) s = strlen(out)-offset;
			memcpy(buf, out+offset, s);
		} else {
			s = 0;
		}
		return s;
	}

	int res = pread(fi->fh, buf, size, offset);
	if (res == -1) return -errno;

	if (stats_enabled) stats_add_read(size);

	return res;
}

static int userModeFS_readdirMethod1(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {


	DBG("readdir\n");

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", root, path);
	if(monitor)mprintf("readdir1 %s",path);
	DIR *dp = opendir(p);
	if (dp){
		struct dirent *de;
		while ((de = readdir(dp)) != NULL) {
			if (filler(buf, de->d_name, NULL, 0)) break;
		}

		closedir(dp);
	}

	if (stats_enabled && strcmp(path, "/") == 0) {
		filler(buf, "stats", NULL, 0);
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}
static int userModeFS_readdirMethod2(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	DBG("readdir\n");

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", root, path);
	if(monitor)mprintf("readdir2 %s",path);
	DIR *dp = opendir(p);
	if (dp){
		struct dirent *de;
		if(offset)seekdir(dp,offset);
		while ((de = readdir(dp)) != NULL) {
			if (filler(buf, de->d_name, NULL, telldir(dp))) break;
		}
		closedir(dp);
	}
	if (stats_enabled && strcmp(path, "/") == 0) {
		filler(buf, "stats", NULL, 0);
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}
static int userModeFS_readlink(const char *path, char *buf, size_t size) {
	DBG("readlink\n");

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", root, path);
	if(monitor)mprintf("readlink: %s",path);
	int res = readlink(p, buf, size - 1);
	if (res == -1) {
		res=errno;
		if(monitor)mprintf(" res=%x\n",res);
		return -res;
	}

	buf[res] = '\0';
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

static int userModeFS_release(const char *path, struct fuse_file_info *fi) {
	DBG("release\n");

	if (stats_enabled && strcmp(path, STATS_FILENAME) == 0) return 0;
	if(monitor)mprintf("release(close): %s",path);
	int res = close(fi->fh);
	if (res == -1) {
		res=errno;
		if(monitor)mprintf(" res=%x\n",res);
		return -res;
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

static int userModeFS_rename(const char *from, const char *to) {
	DBG("rename\n");

	char f[PATHLEN_MAX];
	snprintf(f, PATHLEN_MAX, "%s%s", root, from);

	char t[PATHLEN_MAX];
	snprintf(t, PATHLEN_MAX, "%s%s", root, to);
	if(monitor)mprintf("rename from:%s, to:%s",from,to);
	int res = rename(f, t);
	if (res == -1) {
		res=errno;
		if(monitor)mprintf(" res=%x\n",res);
		return -res;
	}

	// The path should no longer exist
	
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

static int userModeFS_rmdir(const char *path) {
	DBG("rmdir\n");

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", root, path);
	if(monitor)mprintf("rmdir: %s",path);
	int res = rmdir(p);
	if (res == -1) {
		res=errno;
		if(monitor)mprintf(" res=%x\n",res);
		return -res;
	}

	// The path should no longer exist
	
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

static int userModeFS_statfs(const char *path, struct statvfs *stbuf) {
	(void)path;

	DBG("statfs\n");
	if(monitor)mprintf("statfs: %s",path);
	int res = statvfs(root, stbuf);
	if (res == -1) {
		res=errno;
		if(monitor)mprintf(" res=%x\n",res);
		return -res;
	}


	stbuf->f_fsid = 0;
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

static int userModeFS_symlink(const char *from, const char *to) {
	DBG("symlink\n");


	char t[PATHLEN_MAX];
	snprintf(t, PATHLEN_MAX, "%s%s", root, to);
	if(monitor)mprintf("symlink from:%s, to:%s",from,to);
	int res = symlink(from, t);
	if (res == -1) {
		res=errno;
		if(monitor)mprintf(" res=%x\n",res);
		return -res;
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

static int userModeFS_truncate(const char *path, off_t size) {
	DBG("truncate\n");

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", root, path);
	if(monitor)mprintf("truncate: %s",path);
	int res = truncate(p, size);
	if (res == -1) {
		res=errno;
		if(monitor)mprintf(" res=%x\n",res);
		return -res;
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

static int userModeFS_unlink(const char *path) {
	DBG("unlink\n");

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", root, path);
	if(monitor)mprintf("unlink: %s",path);
	int res = unlink(p);
	if (res == -1) {
		res=errno;
		if(monitor)mprintf(" res=%x\n",res);
		return -res;
	}

	// The path should no longer exist

	if(monitor)mprintf(" res=OK\n");
	return 0;
}

static int userModeFS_utime(const char *path, struct utimbuf *buf) {
	DBG("utime\n");

	if (stats_enabled && strcmp(path, STATS_FILENAME) == 0) return 0;

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", root, path);
	if(monitor)mprintf("utime: %s",path);
	int res = utime(p, buf);
	if (res == -1) {
		res=errno;
		if(monitor)mprintf(" res=%x\n",res);
		return -res;
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

static int userModeFS_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	(void)path;

	DBG("write\n");

	int res = pwrite(fi->fh, buf, size, offset);
	if (res == -1) return -errno;

	if (stats_enabled) stats_add_written(size);

	return res;
}

#ifdef HAVE_SETXATTR
static int userModeFS_getxattr(const char *path, const char *name, char *value, size_t size) {
	DBG("getxattr\n");

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", root, path);
	if(monitor)mprintf("getxattr: %s",path);
	int res = lgetxattr(p, name, value, size);
	if (res == -1) {
		res=errno;
		if(monitor)mprintf(" res=%x\n",res);
		return -res;
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

static int userModeFS_listxattr(const char *path, char *list, size_t size) {
	DBG("listxattr\n");

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", root, path);
	if(monitor)mprintf("listxattr: %s",path);
	int res = llistxattr(p, list, size);
	if (res == -1) {
		res=errno;
		if(monitor)mprintf(" res=%x\n",res);
		return -res;
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

static int userModeFS_removexattr(const char *path, const char *name) {
	DBG("removexattr\n");

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", root, path);
	if(monitor)mprintf("removexattr: %s,name=%s",path,name);
	int res = lremovexattr(p, name);
	if (res == -1) {
		res=errno;
		if(monitor)mprintf(" res=%x\n",res);
		return -res;
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}

static int userModeFS_setxattr(const char *path, const char *name, const char *value, size_t size, int flags) {
	DBG("sexattr\n");

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", root, path);
	if(monitor)mprintf("setxattr: %s,name=%s,value=%s",path,name,value);
	int res = lsetxattr(p, name, value, size, flags);
	if (res == -1) {
		res=errno;
		if(monitor)mprintf(" res=%x\n",res);
		return -res;
	}
	if(monitor)mprintf(" res=OK\n");
	return 0;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations userModeFS_oper = {
	.access	= userModeFS_access,
	.chmod	= userModeFS_chmod,
	.chown	= userModeFS_chown,
	.flush	= userModeFS_flush,
	.fsync	= userModeFS_fsync,
	.getattr	= userModeFS_getattr,
	.link	= userModeFS_link,
	.mkdir	= userModeFS_mkdir,
	.mknod	= userModeFS_mknod,
	.open	= userModeFS_open,
	.read	= userModeFS_read,
	.readlink	= userModeFS_readlink,
	.readdir	= userModeFS_readdirMethod1,
	.release	= userModeFS_release,
	.rename	= userModeFS_rename,
	.rmdir	= userModeFS_rmdir,
	.statfs	= userModeFS_statfs,
	.symlink	= userModeFS_symlink,
	.truncate	= userModeFS_truncate,
	.unlink	= userModeFS_unlink,
	.utime	= userModeFS_utime,
	.write	= userModeFS_write,
#ifdef HAVE_SETXATTR
	.getxattr	= userModeFS_getxattr,
	.listxattr	= userModeFS_listxattr,
	.removexattr	= userModeFS_removexattr,
	.setxattr	= userModeFS_setxattr,
#endif
};
int userFSMain(struct fuse_args *args,int use_readir_method2){
	if(use_readir_method2)userModeFS_oper.readdir	= userModeFS_readdirMethod2;
	umask(0);
	return(fuse_main(args->argc, args->argv, &userModeFS_oper/*, NULL*/));
}

