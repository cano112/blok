/*
  Blok File System
  Copyright (C) 2019 Kamil Noster <kamil.noster@gmail.com>

  The code is derived from:
  Big Brother File System
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>
  Which is distributed under the GNU GPLv3.

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.
  A copy of that code is included in the file fuse.h
*/

#include "../include/params.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stdarg.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

void log_msg(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(BLOK_DATA->logfile, format, ap);
}

//  All the paths I see are relative to the root of the mounted filesystem.  In order to get to the underlying
//  filesystem, I need to have the mountpoint. I'll save it away early on in main(), and then whenever I need a path
//  for something I'll call this to construct it.
static void blok_fullpath(char fpath[PATH_MAX], const char *path)
{
    strcpy(fpath, BLOK_DATA->rootdir);
    strncat(fpath, path, PATH_MAX); // ridiculously long paths will break here
}

static int wrap_return_code(int real_code) {
    if(real_code < 0) {
        return -errno;
    }
    return real_code;
}

int blok_getattr(const char *path, struct stat *statbuf)
{
    char fpath[PATH_MAX];
    blok_fullpath(fpath, path);
    return wrap_return_code(lstat(fpath, statbuf));
}

// Note the system readlink() will truncate and lose the terminating null. So, the size passed to to the system
// readlink() must be one less than the size passed to blok_readlink() blok_readlink() code by Bernardo F Costa (thanks!)
int blok_readlink(const char *path, char *link, size_t size)
{
    char fpath[PATH_MAX];
    blok_fullpath(fpath, path);

    int retstat = wrap_return_code(readlink(fpath, link, size - 1));
    if (retstat >= 0) {
	    link[retstat] = '\0';
	    retstat = 0;
    }
    return retstat;
}

int blok_mknod(const char *path, mode_t mode, dev_t dev)
{
    char fpath[PATH_MAX];
    blok_fullpath(fpath, path);
    
    // On Linux this could just be 'mknod(path, mode, dev)' but this tries to be be more portable by honoring
    // the quote in the Linux mknod man page stating the only portable use of mknod() is to make a fifo, but saying
    // it should never actually be used for that.
    if (S_ISREG(mode)) {
	    int retstat = wrap_return_code(open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode));
	    if (retstat >= 0) {
            return wrap_return_code(close(retstat));
        }
	    return retstat;
    } else {
        if (S_ISFIFO(mode)) {
            return wrap_return_code(mkfifo(fpath, mode));
        }
        else {
            return wrap_return_code(mknod(fpath, mode, dev));
        }
    }
}

int blok_mkdir(const char *path, mode_t mode)
{
    char fpath[PATH_MAX];
    blok_fullpath(fpath, path);
    return wrap_return_code(mkdir(fpath, mode));
}

int blok_unlink(const char *path)
{
    char fpath[PATH_MAX];
    blok_fullpath(fpath, path);
    return wrap_return_code(unlink(fpath));
}

int blok_rmdir(const char *path)
{
    char fpath[PATH_MAX];
    blok_fullpath(fpath, path);
    return wrap_return_code(rmdir(fpath));
}

// The parameters here are a little bit confusing, but do correspond to the symlink() system call.  The 'path' is where
// the link points, while the 'link' is the link itself.  So we need to leave the path unaltered, but insert the link
// into the mounted directory.
int blok_symlink(const char *path, const char *link)
{
    char flink[PATH_MAX];
    blok_fullpath(flink, link);
    return wrap_return_code(symlink(path, flink));
}

// both path and newpath are fs-relative
int blok_rename(const char *path, const char *newpath)
{
    char fpath[PATH_MAX];
    char fnewpath[PATH_MAX];
    blok_fullpath(fpath, path);
    blok_fullpath(fnewpath, newpath);
    return wrap_return_code(rename(fpath, fnewpath));
}

int blok_link(const char *path, const char *newpath)
{
    char fpath[PATH_MAX];
    char fnewpath[PATH_MAX];
    blok_fullpath(fpath, path);
    blok_fullpath(fnewpath, newpath);
    return wrap_return_code(link(fpath, fnewpath));
}

int blok_chmod(const char *path, mode_t mode)
{
    char fpath[PATH_MAX];
    blok_fullpath(fpath, path);
    return wrap_return_code(chmod(fpath, mode));
}

int blok_chown(const char *path, uid_t uid, gid_t gid)
{
    char fpath[PATH_MAX];
    blok_fullpath(fpath, path);
    return wrap_return_code(chown(fpath, uid, gid));
}

int blok_truncate(const char *path, off_t newsize)
{
    char fpath[PATH_MAX];
    blok_fullpath(fpath, path);
    return wrap_return_code(truncate(fpath, newsize));
}

int blok_utime(const char *path, struct utimbuf *ubuf)
{
    char fpath[PATH_MAX];
    blok_fullpath(fpath, path);
    return wrap_return_code(utime(fpath, ubuf));
}

int blok_open(const char *path, struct fuse_file_info *fi)
{
    char fpath[PATH_MAX];
    blok_fullpath(fpath, path);

    // if the open call succeeds, my retstat is the file descriptor, else it's -errno.  I'm making sure that in that
    // case the saved file descriptor is exactly -1.
    int fd = wrap_return_code(open(fpath, fi->flags));
    fi->fh = fd;
    if(fd < 0) {
        return -errno;
    }
    return 0;
}

int blok_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    log_msg("{filename: \"%s\", offset: %ld, size: %zu}\n", path, offset, size);
    return wrap_return_code(pread(fi->fh, buf, size, offset));
}

int blok_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    return wrap_return_code(pwrite(fi->fh, buf, size, offset));
}

int blok_statfs(const char *path, struct statvfs *statv)
{
    char fpath[PATH_MAX];
    blok_fullpath(fpath, path);
    return wrap_return_code(statvfs(fpath, statv));
}

int blok_flush(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

int blok_release(const char *path, struct fuse_file_info *fi)
{
    return wrap_return_code(close(fi->fh));
}

int blok_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    // some unix-like systems (notably freebsd) don't have a datasync call
#ifdef HAVE_FDATASYNC
    if (datasync)
	    return wrap_return_code(fdatasync(fi->fh));
    else
#endif	
	return wrap_return_code(fsync(fi->fh));
}

#ifdef HAVE_SYS_XATTR_H
/** Note that my implementations of the various xattr functions use
    the 'l-' versions of the functions (eg blok_setxattr() calls
    lsetxattr() not setxattr(), etc).  This is because it appears any
    symbolic links are resolved before the actual call takes place, so
    I only need to use the system-provided calls that don't follow
    them */

int blok_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    char fpath[PATH_MAX];
    blok_fullpath(fpath, path);
    return wrap_return_code(lsetxattr(fpath, name, value, size, flags));
}

int blok_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    blok_fullpath(fpath, path);
    return wrap_return_code(lgetxattr(fpath, name, value, size));
}

int blok_listxattr(const char *path, char *list, size_t size)
{
    char fpath[PATH_MAX];
    blok_fullpath(fpath, path);
    return wrap_return_code(llistxattr(fpath, list, size));
}

int blok_removexattr(const char *path, const char *name)
{
    char fpath[PATH_MAX];
    blok_fullpath(fpath, path);
    return wrap_return_code(lremovexattr(fpath, name));
}
#endif

int blok_opendir(const char *path, struct fuse_file_info *fi)
{
    char fpath[PATH_MAX];
    blok_fullpath(fpath, path);
    DIR *dp = opendir(fpath);
    fi->fh = (intptr_t) dp;
    if (dp == NULL)
	    return -errno;
    return 0;
}

int blok_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    DIR *dp = (DIR *) (uintptr_t) fi->fh;

    struct dirent *de = readdir(dp);
    if (de == 0) {
	    return -errno;
    }

    // This will copy the entire directory into the buffer.  The loop exits when either the system readdir() returns
    // NULL, or filler() returns something non-zero.  The first case just means I've read the whole directory;
    // the second means the buffer is full.
    do {
        if (filler(buf, de->d_name, NULL, 0) != 0) {
            return -ENOMEM;
        }
    } while ((de = readdir(dp)) != NULL);
    return retstat;
}

int blok_releasedir(const char *path, struct fuse_file_info *fi)
{
    closedir((DIR *) (uintptr_t) fi->fh);
    return 0;
}

int blok_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    return 0;
}

// Undocumented but extraordinarily useful fact:  the fuse_context is set up before this function is called, and
// fuse_get_context()->private_data returns the user_data passed to fuse_main().  Really seems like either it should
// be a third parameter coming in here, or else the fact should be documented (and this might as well return void, as
// it did in older versions of FUSE).
void *blok_init(struct fuse_conn_info *conn)
{
    return BLOK_DATA;
}

void blok_destroy(void *userdata)
{

}

int blok_access(const char *path, int mask)
{
    char fpath[PATH_MAX];
    blok_fullpath(fpath, path);
    
    int retstat = access(fpath, mask);
    if (retstat < 0) {
        return -errno;
    }
    return retstat;
}

int blok_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
    int retstat = ftruncate(fi->fh, offset);
    if (retstat < 0) {
        return -errno;
    }
    return retstat;
}

int blok_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
    // On FreeBSD, trying to do anything with the mountpoint ends up
    // opening it, and then using the FD for an fgetattr.  So in the
    // special case of a path of "/", I need to do a getattr on the
    // underlying root directory instead of doing the fgetattr().
    if (!strcmp(path, "/")) {
        return blok_getattr(path, statbuf);
    }

    
    int retstat = fstat(fi->fh, statbuf);
    if (retstat < 0) {
        return -errno;
    }
    return retstat;
}

struct fuse_operations blok_oper = {
  .getattr = blok_getattr,
  .readlink = blok_readlink,
  .getdir = NULL, // deprecated
  .mknod = blok_mknod,
  .mkdir = blok_mkdir,
  .unlink = blok_unlink,
  .rmdir = blok_rmdir,
  .symlink = blok_symlink,
  .rename = blok_rename,
  .link = blok_link,
  .chmod = blok_chmod,
  .chown = blok_chown,
  .truncate = blok_truncate,
  .utime = blok_utime,
  .open = blok_open,
  .read = blok_read,
  .write = blok_write,
  .statfs = blok_statfs,
  .flush = blok_flush,
  .release = blok_release,
  .fsync = blok_fsync,
#ifdef HAVE_SYS_XATTR_H
  .setxattr = blok_setxattr,
  .getxattr = blok_getxattr,
  .listxattr = blok_listxattr,
  .removexattr = blok_removexattr,
#endif
  .opendir = blok_opendir,
  .readdir = blok_readdir,
  .releasedir = blok_releasedir,
  .fsyncdir = blok_fsyncdir,
  .init = blok_init,
  .destroy = blok_destroy,
  .access = blok_access,
  .ftruncate = blok_ftruncate,
  .fgetattr = blok_fgetattr
};

void blok_usage()
{
    fprintf(stderr, "usage:  blok [FUSE and mount options] rootDir mountPoint\n");
    abort();
}

FILE *log_open()
{
    FILE *logfile;

    // very first thing, open up the logfile and mark that we got in
    // here.  If we can't open the logfile, we're dead.
    logfile = fopen("blok.log", "w");
    if (logfile == NULL) {
        perror("logfile");
        exit(EXIT_FAILURE);
    }

    // set logfile to line buffering
    setvbuf(logfile, NULL, _IOLBF, 0);

    return logfile;
}

int main(int argc, char *argv[])
{
    fprintf(stderr, "Fuse library version %d.%d\n", FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
    
    // Perform some sanity checking on the command line:  make sure there are enough arguments, and that neither of
    // the last two start with a hyphen (this will break if you actually have a rootpoint or mountpoint whose name
    // starts with a hyphen, but so will a zillion other programs)
    if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-')) {
        blok_usage();
    }

    struct fs_state *blok_data = malloc(sizeof(struct fs_state));
    if (blok_data == NULL) {
	    perror("main calloc");
	    abort();
    }

    // Pull the rootdir out of the argument list and save it in my internal data
    blok_data->rootdir = realpath(argv[argc-2], NULL);
    blok_data->logfile = log_open();
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;

    int fuse_stat = fuse_main(argc, argv, &blok_oper, blok_data);

    return fuse_stat;
}
