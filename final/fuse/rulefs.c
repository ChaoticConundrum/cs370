/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

/** @file
 *
 * This file system mirrors the existing file system hierarchy of the
 * system, starting at the root file system. This is implemented by
 * just "passing through" all requests to the corresponding user-space
 * libc functions. Its performance is terrible.
 *
 * Compile with
 *
 *     gcc -Wall passthrough.c `pkg-config fuse3 --cflags --libs` -o passthrough
 *
 * ## Source code ##
 * \include passthrough.c
 */


#define FUSE_USE_VERSION 30

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <limits.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

struct rulefs_data {
    const char *rootparam;
};

#define RFS_DATA ((struct rulefs_data *)fuse_get_context()->private_data)

static void *rfs_init(struct fuse_conn_info *conn,
                      struct fuse_config *cfg)
{
    (void) conn;
    cfg->use_ino = 1;

    /* Pick up changes from lower filesystem right away. This is
       also necessary for better hardlink support. When the kernel
       calls the unlink() handler, it does not know the inode of
       the to-be-removed entry and can therefore not invalidate
       the cache of the associated inode - resulting in an
       incorrect st_nlink value being reported for any remaining
       hardlinks to this inode. */
    cfg->entry_timeout = 0;
    cfg->attr_timeout = 0;
    cfg->negative_timeout = 0;

    // Need to return fuse_get_context()->private_data from here,
    // so it will be filled in future fuse_get_context() calls?
    return RFS_DATA;
}

static int rfs_getattr(const char *path, struct stat *stbuf,
                       struct fuse_file_info *fi)
{
    (void) fi;
    int res;

    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

//    printf("%s: %s\n", __FUNCTION__, mpath);

    res = lstat(mpath, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int rfs_access(const char *path, int mask)
{
    int res;

    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

//    printf("%s: %s\n", __FUNCTION__, mpath);

    res = access(mpath, mask);
    if (res == -1)
        return -errno;

    return 0;
}

static int rfs_readlink(const char *path, char *buf, size_t size)
{
    int res;

    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    printf("%s: %s\n", __FUNCTION__, mpath);

    res = readlink(mpath, buf, size - 1);
    if (res == -1)
        return -errno;

    buf[res] = '\0';
    return 0;
}


static int rfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi,
                       enum fuse_readdir_flags flags)
{
    DIR *dp;
    struct dirent *de;

    (void) offset;
    (void) fi;
    (void) flags;

    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    printf("%s: %s\n", __FUNCTION__, mpath);

    dp = opendir(mpath);
    if (dp == NULL)
        return -errno;

    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        if(strcmp(de->d_name, ".Rulefile") == 0){
            printf("name: %s\n", de->d_name);
        }

        if (filler(buf, de->d_name, &st, 0, 0))
            break;
    }

    closedir(dp);
    return 0;
}

static int rfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int res;

    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    printf("%s: %s\n", __FUNCTION__, mpath);

    /* On Linux this could just be 'mknod(path, mode, rdev)' but this
       is more portable */
    if (S_ISREG(mode)) {
        res = open(mpath, O_CREAT | O_EXCL | O_WRONLY, mode);
        if (res >= 0)
            res = close(res);
    } else if (S_ISFIFO(mode))
        res = mkfifo(mpath, mode);
    else
        res = mknod(mpath, mode, rdev);
    if (res == -1)
        return -errno;

    return 0;
}

static int rfs_mkdir(const char *path, mode_t mode)
{
    int res;

    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    printf("%s: %s\n", __FUNCTION__, mpath);

    res = mkdir(mpath, mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int rfs_unlink(const char *path)
{
    int res;

    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    printf("%s: %s\n", __FUNCTION__, mpath);

    res = unlink(mpath);
    if (res == -1)
        return -errno;

    return 0;
}

static int rfs_rmdir(const char *path)
{
    int res;

    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    printf("%s: %s\n", __FUNCTION__, mpath);

    res = rmdir(mpath);
    if (res == -1)
        return -errno;

    return 0;
}

static int rfs_symlink(const char *from, const char *to)
{
    int res;

    char fpath[PATH_MAX];
    strcpy(fpath, RFS_DATA->rootparam);
    strcat(fpath, from);

    char tpath[PATH_MAX];
    strcpy(tpath, RFS_DATA->rootparam);
    strcat(tpath, to);

    printf("%s: %s, %s\n", __FUNCTION__, fpath, tpath);

    res = symlink(fpath, tpath);
    if (res == -1)
        return -errno;

    return 0;
}

static int rfs_rename(const char *from, const char *to, unsigned int flags)
{
    int res;

    char fpath[PATH_MAX];
    strcpy(fpath, RFS_DATA->rootparam);
    strcat(fpath, from);

    char tpath[PATH_MAX];
    strcpy(tpath, RFS_DATA->rootparam);
    strcat(tpath, to);

    printf("%s: %s, %s\n", __FUNCTION__, fpath, tpath);

    if (flags)
        return -EINVAL;

    res = rename(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

static int rfs_link(const char *from, const char *to)
{
    int res;

    char fpath[PATH_MAX];
    strcpy(fpath, RFS_DATA->rootparam);
    strcat(fpath, from);

    char tpath[PATH_MAX];
    strcpy(tpath, RFS_DATA->rootparam);
    strcat(tpath, to);

    printf("%s: %s, %s\n", __FUNCTION__, fpath, tpath);

    res = link(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

static int rfs_chmod(const char *path, mode_t mode,
                     struct fuse_file_info *fi)
{
    (void) fi;
    int res;

    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    res = chmod(mpath, mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int rfs_chown(const char *path, uid_t uid, gid_t gid,
                     struct fuse_file_info *fi)
{
    (void) fi;
    int res;

    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    res = lchown(mpath, uid, gid);
    if (res == -1)
        return -errno;

    return 0;
}

static int rfs_truncate(const char *path, off_t size,
                        struct fuse_file_info *fi)
{
    int res;

    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    printf("%s: %s\n", __FUNCTION__, mpath);

    if (fi != NULL)
        res = ftruncate(fi->fh, size);
    else
        res = truncate(mpath, size);
    if (res == -1)
        return -errno;

    return 0;
}

#ifdef HAVE_UTIMENSAT
static int xmp_utimens(const char *path, const struct timespec ts[2],
struct fuse_file_info *fi)
{
    (void) fi;
    int res;

    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    /* don't use utime/utimes since they follow symlinks */
    res = utimensat(0, mpath, ts, AT_SYMLINK_NOFOLLOW);
    if (res == -1)
        return -errno;

    return 0;
}
#endif

static int rfs_create(const char *path, mode_t mode,
                      struct fuse_file_info *fi)
{
    int res;

    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    printf("%s: %s\n", __FUNCTION__, mpath);

    res = open(mpath, fi->flags, mode);
    if (res == -1)
        return -errno;

    fi->fh = res;
    return 0;
}

static int rfs_open(const char *path, struct fuse_file_info *fi)
{
    int res;

    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    printf("%s: %s\n", __FUNCTION__, mpath);

    res = open(mpath, fi->flags);
    if (res == -1)
        return -errno;

    fi->fh = res;
    return 0;
}

static int rfs_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    int fd;
    int res;

    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    printf("%s: %s\n", __FUNCTION__, mpath);

    if(fi == NULL)
        fd = open(mpath, O_RDONLY);
    else
        fd = fi->fh;

    if (fd == -1)
        return -errno;

    res = pread(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    if(fi == NULL)
        close(fd);
    return res;
}

static int rfs_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
    int fd;
    int res;

    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    printf("%s: %s\n", __FUNCTION__, mpath);

    (void) fi;
    if(fi == NULL)
        fd = open(mpath, O_WRONLY);
    else
        fd = fi->fh;

    if (fd == -1)
        return -errno;

    res = pwrite(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    if(fi == NULL)
        close(fd);
    return res;
}

static int rfs_statfs(const char *path, struct statvfs *stbuf)
{
    int res;

    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    printf("%s: %s\n", __FUNCTION__, mpath);

    res = statvfs(mpath, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int rfs_release(const char *path, struct fuse_file_info *fi)
{
    (void) path;
    close(fi->fh);
    return 0;
}

static int rfs_fsync(const char *path, int isdatasync,
                     struct fuse_file_info *fi)
{
    /* Just a stub.	 This method is optional and can safely be left
           unimplemented */

    (void) path;
    (void) isdatasync;
    (void) fi;
    return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int xmp_fallocate(const char *path, int mode,
                         off_t offset, off_t length, struct fuse_file_info *fi)
{
    int fd;
    int res;

    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    (void) fi;

    if (mode)
        return -EOPNOTSUPP;

    if(fi == NULL)
        fd = open(mpath, O_WRONLY);
    else
        fd = fi->fh;

    if (fd == -1)
        return -errno;

    res = -posix_fallocate(fd, offset, length);

    if(fi == NULL)
        close(fd);
    return res;
}
#endif

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int xmp_setxattr(const char *path, const char *name, const char *value,
                        size_t size, int flags)
{
    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    int res = lsetxattr(mpath, name, value, size, flags);
    if (res == -1)
        return -errno;
    return 0;
}

static int xmp_getxattr(const char *path, const char *name, char *value,
                        size_t size)
{
    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    int res = lgetxattr(mpath, name, value, size);
    if (res == -1)
        return -errno;
    return res;
}

static int xmp_listxattr(const char *path, char *list, size_t size)
{
    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    int res = llistxattr(mpath, list, size);
    if (res == -1)
        return -errno;
    return res;
}

static int xmp_removexattr(const char *path, const char *name)
{
    char mpath[PATH_MAX];
    strcpy(mpath, RFS_DATA->rootparam);
    strcat(mpath, path);

    int res = lremovexattr(mpath, name);
    if (res == -1)
        return -errno;
    return 0;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations rfs_oper = {
    .init           = rfs_init,
    .getattr        = rfs_getattr,
    .access         = rfs_access,
    .readlink       = rfs_readlink,
    .readdir        = rfs_readdir,
    .mknod          = rfs_mknod,
    .mkdir          = rfs_mkdir,
    .symlink        = rfs_symlink,
    .unlink         = rfs_unlink,
    .rmdir          = rfs_rmdir,
    .rename         = rfs_rename,
    .link           = rfs_link,
    .chmod          = rfs_chmod,
    .chown          = rfs_chown,
    .truncate       = rfs_truncate,

#ifdef HAVE_UTIMENSAT
    .utimens        = xmp_utimens,
#endif

    .open           = rfs_open,
    .create         = rfs_create,
    .read           = rfs_read,
    .write          = rfs_write,
    .statfs         = rfs_statfs,
    .release        = rfs_release,
    .fsync          = rfs_fsync,

#ifdef HAVE_POSIX_FALLOCATE
    .fallocate      = xmp_fallocate,
#endif

#ifdef HAVE_SETXATTR
    .setxattr       = xmp_setxattr,
    .getxattr       = xmp_getxattr,
    .listxattr      = xmp_listxattr,
    .removexattr    = xmp_removexattr,
#endif
};

static int opt_proc(void *data, const char *arg, int key, struct fuse_args *outarg){
    struct rulefs_data *rfs_data = (struct rulefs_data *)data;
    if(rfs_data == NULL)
        return 1;
    if(key == FUSE_OPT_KEY_NONOPT && rfs_data->rootparam == NULL){
        printf("arg: %s\n", arg);
        rfs_data->rootparam = strdup(arg);
        return 0;
    }
    return 1;
}

int main(int argc, char **argv){
    struct rulefs_data *rfs_data;
    rfs_data = malloc(sizeof(struct rulefs_data));
    rfs_data->rootparam = NULL;

    umask(0);

    if(argc > 2){
        rfs_data->rootparam = realpath(argv[argc-2], NULL);

        printf("root: %s\n", rfs_data->rootparam);
        printf("mount: %s\n", argv[argc-1]);

        argv[argc-2] = argv[argc-1];
        argv[argc-1] = NULL;
        argc--;

//        struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
//        fuse_opt_parse(&args, rfs_data, NULL, opt_proc);

        return fuse_main(argc, argv, &rfs_oper, rfs_data);
    } else {

    }
}

