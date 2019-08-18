#define FUSE_USE_VERSION 27
#define _GNU_SOURCE

#include <libgen.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fuse.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include "fsx600.h"
#include "blkdev.h"

/*
 * disk access - the global variable 'disk' points to a blkdev
 * structure which has been initialized to access the image file.
 *
 * NOTE - blkdev access is in terms of 1024-byte blocks
 */
extern struct blkdev *disk;

/* by defining bitmaps as 'fd_set' pointers, you can use existing
 * macros to handle them.
 *   FD_ISSET(##, inode_map);
 *   FD_CLR(##, block_map);
 *   FD_SET(##, block_map);
 */
fd_set *inode_map;              /* = malloc(sb.inode_map_size * FS_BLOCK_SIZE); */
fd_set *block_map;
struct fs_super super_blk;
struct fs_inode *inodes;

/* init - this is called once by the FUSE framework at startup. Ignore
 * the 'conn' argument.
 * recommended actions:
 *   - read superblock
 *   - allocate memory, read bitmaps and inodes
 */
void* fs_init(struct fuse_conn_info *conn)
{

    if (disk->ops->read(disk, 0, 1, &super_blk) < 0)
        exit(1);
    inode_map = (fd_set*) malloc(super_blk.inode_map_sz * FS_BLOCK_SIZE);
    block_map = (fd_set*) malloc(super_blk.block_map_sz * FS_BLOCK_SIZE);
    inodes = (struct fs_inode*) malloc(super_blk.inode_region_sz * FS_BLOCK_SIZE);

    // read inode_map
    if (disk->ops->read(disk, 1, super_blk.inode_map_sz, inode_map) < 0) {
        exit(1);
    }

    // read block_map
    if (disk->ops->read(disk, 1 + super_blk.inode_map_sz, super_blk.block_map_sz, block_map) < 0) {
        exit(1);
    }

    // read inodes
    if (disk->ops->read(disk, 1 + super_blk.inode_map_sz + super_blk.block_map_sz, super_blk.inode_region_sz, inodes) < 0) {
        exit(1);
    }

    return NULL;
}

static void updateDisk()
{
    if (disk->ops->write(disk, 1, super_blk.inode_map_sz, inode_map) < 0) {
        exit(1);
    }

    if (disk->ops->write(disk, 1 + super_blk.inode_map_sz, super_blk.block_map_sz, block_map) < 0) {
        exit(1);
    }

    if (disk->ops->write(disk, 1 + super_blk.inode_map_sz + super_blk.block_map_sz, super_blk.inode_region_sz, inodes) < 0) {
        exit(1);
    }
}


const char *delim = "/";

/* Return inode number for specified file or
 * directory.
 *
 * Errors
 *   -ENOENT  - a component of the path is not present.
 *   -ENOTDIR - an intermediate component of path not a directory
 *
 * @param path the file path
 * @return inode of path node or error
 */

static int translate(const char *path)
{
    int inum = 1; // root inode
    int k, j;
    int length = 0;
    const char *token[64];
    char *p = NULL;

    if (strcmp(path, "/") == 0) { //is _path is "/" return root inode num (1)
        return inum;
    }

    char *dpath = strdup(path); //duplicate _path
    p = strtok(dpath, delim);

    while (p) {
        token[length++] = p;
        p = strtok(0, delim);
    }
    struct fs_dirent *de =  (struct fs_dirent*) malloc(FS_BLOCK_SIZE);

    for(j = 0; j < length; j++) {
        bzero(de, FS_BLOCK_SIZE);
        if (!S_ISDIR(inodes[inum].mode)) return -ENOTDIR;
        if (disk->ops->read(disk, inodes[inum].direct[0], 1, de) < 0) {
            exit(1);
        }
        for (k = 0; k <= 31; k++) {
            if (de[k].valid && (strcmp(de[k].name, token[j]) == 0)) {
                inum = de[k].inode;
                break;
            }
            if (k == 31) {
                inum = -ENOENT;
                break;
            }
        }
    }
    free(de);
    return inum;
}

static int translate_1(const char *path, char *leaf) //done
{
	int inum = 1; // root inode
	int k, j;
	int length = 0;
	const char *token[64];
	char *p = NULL;

	if (strcmp(path, "/") == 0) { //is _path is "/", return root inode num (1)
		return inum;
	}

	char *pathdu = strdup(path); //duplicate string
	p = strtok(pathdu, delim); // .split with "/"

	while (p) {
		token[length++] = p;
		p = strtok(0, delim);
	}

	struct fs_dirent *de =  (struct fs_dirent*) malloc(FS_BLOCK_SIZE);

	for(j = 0; j < length - 1; j++) {
		bzero(de, FS_BLOCK_SIZE); //Sets the first num bytes of the block of memory pointed by ptr to the specified value
		if (!S_ISDIR(inodes[inum].mode)) return -ENOTDIR; // check if is dir, otherwise return error
		if (disk->ops->read(disk, inodes[inum].direct[0], 1, de) < 0) { // read dir content into de
			exit(1);
		}
		for (k = 0; k <= 31; k++) {
			if (de[k].valid && (strcmp(de[k].name, token[j]) == 0)) {
				inum = de[k].inode; //find next level directory break k loop
				break;
			}
			if (k == 31) {
				inum = -ENOENT; // not find directory after looping k, no such element, return error
				break;
			}
		}
	}
	char *last = strrchr(path, '/') + 1;
	strcpy(leaf, last);
	free(de); //free de memory
	return inum;
}
/* Note on path translation errors:
 * In addition to the method-specific errors listed below, almost
 * every method can return one of the following errors if it fails to
 * locate a file or directory corresponding to a specified path.
 *
 * ENOENT - a component of the path is not present.
 * ENOTDIR - an intermediate component of the path (e.g. 'b' in
 *           /a/b/c) is not a directory
 */

/* note on splitting the 'path' variable:
 * the value passed in by the FUSE framework is declared as 'const',
 * which means you can't modify it. The standard mechanisms for
 * splitting strings in C (strtok, strsep) modify the string in place,
 * so you have to copy the string and then free the copy when you're
 * done. One way of doing this:
 *
 *    char *_path = strdup(path);
 *    int inum = translate(_path);
 *    free(_path);
 */

/* getattr - get file or directory attributes. For a description of
 *  the fields in 'struct stat', see 'man lstat'.
 *
 * Note - fields not provided in fsx600 are:
 *    st_nlink - always set to 1
 *    st_atime, st_ctime - set to same value as st_mtime
 *
 * errors - path translation, ENOENT
 */

static void fill_stat(int inum, struct fs_inode _inode ,struct stat *sb) {

    memset(sb,0,sizeof(*sb));
    sb->st_dev = 0;                    /* ID of device containing file */
    sb->st_ino = inum;                /* inode number */
    sb->st_mode = _inode.mode;     /* protection */
    sb->st_nlink = 1;                  /* number of hard links */
    sb->st_uid = _inode.uid;       /* user ID of owner */
    sb->st_gid = _inode.gid;       /* group ID of owner */
    sb->st_rdev = 0;                   /* device ID (if special file) */
    sb->st_size = _inode.size;     /* total size, in bytes */
    sb->st_blksize = FS_BLOCK_SIZE;    /* blocksize for filesystem I/O */
    sb->st_blocks = (_inode.size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;  /* number of 512B blocks allocated */
    sb->st_atime = _inode.mtime;   /* time of last access */
    sb->st_mtime = _inode.mtime;   /* time of last modification */
    sb->st_ctime = _inode.ctime;   /* time of last status change */

}

static int fs_getattr(const char *path, struct stat *sb)
{
    int inum = translate(path);
    if (inum < 0) {
        return inum;
    }

    struct fs_inode _inode = inodes[inum];
    fill_stat(inum, _inode, sb);

    return 0;
}

/* readdir - get directory contents.
 *
 * for each entry in the directory, invoke the 'filler' function,
 * which is passed as a function pointer, as follows:
 *     filler(buf, <name>, <statbuf>, 0)
 * where <statbuf> is a struct stat, just like in getattr.
 *
 * Errors - path resolution, ENOTDIR, ENOENT
 */
static int fs_readdir(const char *path, void *ptr, fuse_fill_dir_t filler,
   off_t offset, struct fuse_file_info *fi)
{
    int i = 0;
    struct stat sb;

    int inum = translate(path);
    if (inum < 0) {
        return inum;
    }
    if (!S_ISDIR(inodes[inum].mode)) return -ENOTDIR;

    // struct fs_inode _inode = inodes[inum];
    struct fs_dirent *de =  (struct fs_dirent*) malloc(FS_BLOCK_SIZE);
    if (disk->ops->read(disk, inodes[inum].direct[0], 1, de) < 0) {
        exit(1);
    }
    // sb = (struct stat*) malloc(sizeof(struct stat));

    for (; i <=31; i++) {
        if (!de[i].valid) {
            continue;
        }
        fill_stat(inum, inodes[de[i].inode], &sb);
        filler(ptr, de[i].name, &sb, 0);
    }
    free(de);
    return 0;
}

/* see description of Part 2. In particular, you can save information
 * in fi->fh. If you allocate memory, free it in fs_releasedir.
 */
static int fs_opendir(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

static int fs_releasedir(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

static int getFreeInodeIndex()
{
    int i;
    for (i = 0; i < super_blk.inode_map_sz * FS_BLOCK_SIZE; i++) {
        if (!FD_ISSET(i, inode_map)) {
            return i;
        }
    }
    return -ENOSPC;
}

static int getFreeBlockIndex()
{
    int i;
    for (i = 0; i < super_blk.block_map_sz * FS_BLOCK_SIZE; i++) {
        if (!FD_ISSET(i, block_map)) {
            return i;
        }
    }
    return -ENOSPC;
}

static int addEntity(const char *path, mode_t mode, int isDir)
{
    int index;
    char base[1024];
    int inum = translate(path);
    int inumDir = translate_1(path, base);

    if (inumDir < 0) {
        return -ENOENT;
    } else if (!S_ISDIR(inodes[inumDir].mode)) {
        return -ENOTDIR;
    }

    struct fs_dirent *de =  (struct fs_dirent*) malloc(FS_BLOCK_SIZE);
    if (disk->ops->read(disk, inodes[inumDir].direct[0], 1, de) < 0) {
        exit(1);
    }

    for (index = 0; index <= 31; index++) {
        if (de[index].valid) {
            if (strcmp(de[index].name, base) == 0) {
                free(de);
                return -EEXIST;
            }
        }
    }

    for (index = 0; index <= 31; index++) {
        if (de[index].valid == 0) {
            break;
        }
        if (index == 31) {
            free(de);
            return -ENOSPC;
        }
    }

    int freeInodeIndex = getFreeInodeIndex();
    int freeBlockIndex = getFreeBlockIndex();

    if (freeInodeIndex < 0 || freeBlockIndex < 0) {
        return -ENOSPC;
    }

    FD_SET(freeInodeIndex, inode_map);

    struct fs_inode *_inode = &inodes[freeInodeIndex];
    struct fuse_context *ctx = fuse_get_context();

    _inode->uid = ctx->uid;
    _inode->gid = ctx->gid;
    _inode->mode =  mode;
    _inode->ctime = time(NULL);
    _inode->mtime = _inode->ctime;
    _inode->size = 0;

    if (isDir) {
        FD_SET(freeBlockIndex, block_map);
        _inode->direct[0] = freeBlockIndex;
    }

    de[index].valid = 1;
    de[index].isDir = isDir;
    de[index].inode = freeInodeIndex;
    strcpy(de[index].name, base);

    // write directory
    if (disk->ops->write(disk, inodes[inumDir].direct[0], 1, de) < 0) {
        exit(1);
    }

    updateDisk();

    free(de);

    return 0;
}

/* mknod - create a new file with permissions (mode & 01777)
 *
 * Errors - path resolution, EEXIST
 *          in particular, for mknod("/a/b/c") to succeed,
 *          "/a/b" must exist, and "/a/b/c" must not.
 *
 * If a file or directory of this name already exists, return -EEXIST.
 * If this would result in >32 entries in a directory, return -ENOSPC
 * if !S_ISREG(mode) return -EINVAL [i.e. 'mode' specifies a device special
 * file or other non-file object]
 */
static int fs_mknod(const char *path, mode_t mode, dev_t dev) // to look into !
{
    if (!S_ISREG(mode)) return -EINVAL;
    return addEntity(path, mode | S_IFREG , 0);
}

/* mkdir - create a directory with the given mode.
 * Errors - path resolution, EEXIST
 * Conditions for EEXIST are the same as for create.
 * If this would result in >32 entries in a directory, return -ENOSPC
 *
 * Note that you may want to combine the logic of fs_mknod and
 * fs_mkdir.
 */
static int fs_mkdir(const char *path, mode_t mode)
{
    return addEntity(path, mode | S_IFDIR , 1);
}

/**
 * Return a block to the free list
 *
 * @param  blkno the block number
 */
static void return_blk(int blkno) //done
{
	int *block;
	block = malloc(FS_BLOCK_SIZE); //allocate BLOCK_SIZE to int
	if (disk->ops->read(disk, blkno, 1, block) < 0) { //read current blkno block to block
		exit(1);
	}
	memset(block, 0, FS_BLOCK_SIZE); //return block to all 0 (format)
	if (disk->ops->write(disk, blkno, 1, block) < 0) { //write all 0 back to blkno block
		exit(1);
	}
	FD_CLR(blkno, block_map); //free block from block map
	free(block);
}

/**
 * Return a inode to the free list.
 *
 * @param  _inode the inode to remove
 */
static void return_inode(struct fs_inode _inode)
{
    int dir;
    int k;
    int i;
    int *indirect1;
    int *indirect2;
    int *double_indirect2;
    int *block;
    //int inum = _inode
    for(dir = 0; dir<6; dir++) {
        if (_inode.direct[dir]) {
        	return_blk(_inode.direct[dir]);
        	_inode.direct[dir] = 0;
        }
    }
    if (_inode.indir_1) {
        indirect1 = malloc(sizeof(int) * 256);
        if (disk->ops->read(disk, _inode.indir_1, 1, indirect1) < 0) {
            exit(1);
        }
        for (k = 0; k < 256; k++) {
            if (indirect1[k] && FD_ISSET(indirect1[k], block_map)) {
            	return_blk(indirect1[k]);
				indirect1[k] = 0;
            }
        }
        if (disk->ops->write(disk, _inode.indir_1, 1, indirect1) < 0) {
            exit(1);
        }

        FD_CLR(_inode.indir_1, block_map);
        _inode.indir_1 = 0;
        free(indirect1);
    }

    if (_inode.indir_2) {
        indirect2 = malloc(sizeof(int) * 256);
        if (disk->ops->read(disk, _inode.indir_2, 1, indirect2) < 0) {
            exit(1);
        }
        for (k = 0; k < 256; k++) {
            if (indirect2[k]) {
                double_indirect2 = malloc(sizeof(int) * 256);
                if (disk->ops->read(disk, indirect2[k], 1, double_indirect2) < 0) {
                    exit(1);
                }
                for (i = 0; i < 256; i++) {
                    if (double_indirect2[i] && FD_ISSET(double_indirect2[i], block_map)) {
                    	return_blk(double_indirect2[i]);
						double_indirect2[i] = 0;
                    }
                }
                if (disk->ops->write(disk, indirect2[k], 1, double_indirect2) < 0) {
                    exit(1);
                }
                FD_CLR(indirect2[k], block_map);
                indirect2[k] = 0;
                free(double_indirect2);
            }
        }
        if (disk->ops->write(disk, _inode.indir_2, 1, indirect2) < 0) {
            exit(1);
        }

        FD_CLR(_inode.indir_2, block_map);
        _inode.indir_2 = 0;
        free(indirect2);
    }
}

/**
 * truncate - truncate file to exactly 'len' bytes.
 *
 * Errors:
 *   ENOENT  - file does not exist
 *   ENOTDIR - component of path not a directory
 *   EINVAL  - length invalid (only supports 0)
 *   EISDIR	 - path is a directory (only files)
 *
 * @param path the file path
 * @param len the length
 * @return 0 if successful, or error value
 */
static int fs_truncate(const char *path, off_t len)
{
    if (len != 0) return -EINVAL;		/* invalid argument */
    int index;
    char base[1024];
    int inum = translate(path);
    int inumDir = translate_1(path, base);

    if (inum < 0) {
        return -ENOENT;
    } else if (S_ISDIR(inodes[inum].mode)) {
        return -EISDIR;
    }

    struct fs_dirent *de =  (struct fs_dirent*) malloc(FS_BLOCK_SIZE);
    if (disk->ops->read(disk, inodes[inumDir].direct[0], 1, de) < 0) {
        exit(1);
    }

    for (index = 0; index<=31; index++) {
        if(de[index].valid && strcmp(de[index].name, base) == 0) {

            // truncate
            inodes[de[index].inode].size = 0;
            inodes[de[index].inode].mtime = time(NULL);

            struct fs_inode _inode = inodes[de[index].inode];
            return_inode(_inode);
            break;
        }
    }

    inodes[inumDir].mtime = time(NULL);

    // write directory
    if (disk->ops->write(disk, inodes[inumDir].direct[0], 1, de) < 0) {
        exit(1);
    }

    updateDisk();

    free(de);
    return 0;
}

/**
 * unlink - delete a file.
 *
 * Errors
 *   -ENOENT   - file does not exist
 *   -ENOTDIR  - component of path not a directory
 *   -EISDIR   - cannot unlink a directory
 *
 * @param path path to file
 * @return 0 if successful, or error value
 */
static int fs_unlink(const char *path)
{
    int index;
    char base[1024];
    int inum = translate(path);
    int inumDir = translate_1(path, base);

    if (inum < 0) {
        return -ENOENT;
    } else if (S_ISDIR(inodes[inum].mode)) {
        return -EISDIR;
    }

    struct fs_dirent *de =  (struct fs_dirent*) malloc(FS_BLOCK_SIZE);
    if (disk->ops->read(disk, inodes[inumDir].direct[0], 1, de) < 0) {
        exit(1);
    }

    for (index = 0; index<=31; index++) {
        if(de[index].valid && strcmp(de[index].name, base) == 0) {

            // unlink
            de[index].valid = 0;
            FD_CLR(de[index].inode, inode_map);

            struct fs_inode _inode = inodes[de[index].inode];
            return_inode(_inode);
            break;
        }
    }

    inodes[inumDir].mtime = time(NULL);

    // write directory
    if (disk->ops->write(disk, inodes[inumDir].direct[0], 1, de) < 0) {
        exit(1);
    }

    updateDisk();

    free(de);
    return 0;
}

/**
 * Determines whether directory is empty.
 *
 * @param inum inode id of directory
 * @return 1 if empty 0 if has entries
 */
static int is_empty_dir(int inum)
{
    int index;
    struct fs_dirent *de =  (struct fs_dirent*) malloc(FS_BLOCK_SIZE);
    if (disk->ops->read(disk, inodes[inum].direct[0], 1, de) < 0) {
        exit(1);
    }

    for (index = 0; index <= 31; index++) {
        if (de[index].valid) {
        	free(de);
            return 0;
        }
    }
    free(de);
    return 1;
}

/**
 * rmdir - remove a directory.
 *
 * Errors
 *   -ENOENT   - file does not exist
 *   -ENOTDIR  - component of path not a directory
 *   -ENOTDIR  - path not a directory
 *   -ENOEMPTY - directory not empty
 *
 * @param path the path of the directory
 * @return 0 if successful, or error value
 */
static int fs_rmdir(const char *path)
{
    int index;
    char base[1024];

    int inum = translate(path);
    int inumDir = translate_1(path, base);

    if (inum < 0) {
        return -ENOENT;
    } else if (!S_ISDIR(inodes[inum].mode)) {
        return -ENOTDIR;
    } else if (!is_empty_dir(inum)) {
        return -ENOTEMPTY;
    }

    struct fs_dirent *de =  (struct fs_dirent*) malloc(FS_BLOCK_SIZE);
    if (disk->ops->read(disk, inodes[inumDir].direct[0], 1, de) < 0) {
        exit(1);
    }

    for (index = 0; index <= 31; index++) {
        if (de[index].valid && (strcmp(de[index].name, base) == 0)) {
            de[index].valid = 0;
            FD_CLR(de[index].inode, inode_map);
            break;
        }
    }

    inodes[inumDir].mtime = time(NULL);
    FD_CLR(inodes[inum].direct[0], block_map);

    // write directory
    if (disk->ops->write(disk, inodes[inumDir].direct[0], 1, de) < 0) {
        exit(1);
    }

    updateDisk();

    free(de);
    return 0;
    // return -EOPNOTSUPP;
}

/**
 * rename - rename a file or directory.
 *
 * Note that this is a simplified version of the UNIX rename
 * functionality - see 'man 2 rename' for full semantics. In
 * particular, the full version can move across directories, replace a
 * destination file, and replace an empty directory with a full one.
 *
 * Errors:
 *   -ENOENT   - source file or directory does not exist
 *   -ENOTDIR  - component of source or target path not a directory
 *   -EEXIST   - destination already exists
 *   -EINVAL   - source and destination not in the same directory
 *
 * @param src_path the source path
 * @param dst_path the destination path.
 * @return 0 if successful, or error value
 */
static int fs_rename(const char *src_path, const char *dst_path)
{
	int index;
    char base_dst[1024];
    char base_src[1024];

    int inumSrc = translate(src_path);
    int inumDst = translate(dst_path);
    int parentSrc = translate_1(src_path, base_src);
    int parentDst = translate_1(dst_path, base_dst);
    if (parentSrc != parentDst) {
		return -EINVAL;
	}

    if (inumSrc < 0) {
        return -ENOENT;
    }
    if (inumDst >= 0) {
        return -EEXIST;
    }

    struct fs_dirent *de =  (struct fs_dirent*) malloc(FS_BLOCK_SIZE);
    if (disk->ops->read(disk, inodes[parentSrc].direct[0], 1, de) < 0) {
        exit(1);
    }

    for (index = 0; index <= 31; index++) {
        if (de[index].valid) {
            if (strcmp(de[index].name, base_src) == 0) {
                strcpy(de[index].name, base_dst);
                break;
            }
        }
    }

    inodes[inumSrc].mtime = time(NULL);

    // write directory
    if (disk->ops->write(disk, inodes[parentSrc].direct[0], 1, de) < 0) {
        exit(1);
    }

    // write inodes
    if (disk->ops->write(disk, 1 + super_blk.inode_map_sz + super_blk.block_map_sz, super_blk.inode_region_sz, inodes) < 0) {
        exit(1);
    }

    free(de);

    return 0;
}

/**
 * chmod - change file permissions
 *
 * Errors:
 *   -ENOENT   - file does not exist
 *   -ENOTDIR  - component of path not a directory
 *
 * @param path the file or directory path
 * @param mode the mode_t mode value -- see man 'chmod'
 *   for description
 * @return 0 if successful, or error value
 */
static int fs_chmod(const char *path, mode_t mode)
{
    int inum = translate(path);

    if (inum < 0) {
        return inum;
    }

    inodes[inum].mode = S_ISDIR(mode) ? (mode | S_IFDIR) :  (mode | S_IFREG);

    if (disk->ops->write(disk, 1 + super_blk.inode_map_sz + super_blk.block_map_sz, super_blk.inode_region_sz, inodes) < 0) {
        exit(1);
    }

    return 0;
}

/**
 * utime - change access and modification times.
 *
 * Errors:
 *   -ENOENT   - file does not exist
 *   -ENOTDIR  - component of path not a directory
 *
 * @param path the file or directory path.
 * @param ut utimbuf - see man 'utime' for description.
 * @return 0 if successful, or error value
 */
int fs_utime(const char *path, struct utimbuf *ut)
{
    int inum = translate(path);
    if (inum < 0) {
        return inum;
    }
    inodes[inum].mtime = ut->modtime;
    if (disk->ops->write(disk, 1 + super_blk.inode_map_sz + super_blk.block_map_sz, super_blk.inode_region_sz, inodes) < 0) {
        exit(1);
    }

    return 0;
}


static int get_blk(int inum, int n, int allo)
{
    int result = 0;

    struct fs_inode _inode = inodes[inum];
    if (n < 6) {

        result = _inode.direct[n];
        if (allo && !result) {
            result = getFreeBlockIndex();
            if (result < 0) return -ENOSPC;
            _inode.direct[n] = result;
            FD_SET(result, block_map);
            disk->ops->write(disk, 1 + super_blk.inode_map_sz, super_blk.block_map_sz, block_map);
        }
        inodes[inum] = _inode;
        return result;
    }

    n -= 6;

    if (n < 256) {

        if (!allo) {
            int *indirect1 = malloc(sizeof(int) * 256);
            if (disk->ops->read(disk, _inode.indir_1, 1, indirect1) < 0) {
                exit(1);
            }
            result = indirect1[n];
            free(indirect1);
        } else {
            if (!_inode.indir_1) {
                int index = getFreeBlockIndex();
                if (result < 0) return -ENOSPC;
                _inode.indir_1 = index;
                FD_SET(index, block_map);
                disk->ops->write(disk, 1 + super_blk.inode_map_sz, super_blk.block_map_sz, block_map);

            }

            int *indirect1 = malloc(sizeof(int) * 256);
            if (disk->ops->read(disk, _inode.indir_1, 1, indirect1) < 0) {
                exit(1);
            }

            result = indirect1[n];
            if (!result) {
                result = getFreeBlockIndex();
                if (result < 0) return -ENOSPC;
                indirect1[n] = result;
                FD_SET(result, block_map);
                disk->ops->write(disk, 1 + super_blk.inode_map_sz, super_blk.block_map_sz, block_map);
            }

            if (disk->ops->write(disk, _inode.indir_1, 1, indirect1) < 0) {
                exit(1);
            }
            free(indirect1);
        }
        inodes[inum] = _inode;
        return result;
    }

    n -= 256;

    if (!allo) {
        int *indirect2 = malloc(sizeof(int) * 256);
        if (disk->ops->read(disk, _inode.indir_2, 1, indirect2) < 0) {
            exit(1);
        }
        int *double_indirect2 = malloc(sizeof(int) * 256);
        if (disk->ops->read(disk, indirect2[n/256], 1, double_indirect2) < 0) {
            exit(1);
        }

        result = double_indirect2[n%256];
        free(double_indirect2);
        free(indirect2);
        return result;

    } else {
        if (!_inode.indir_2) {
            int index = getFreeBlockIndex();
            if (result < 0) return -ENOSPC;
            _inode.indir_2 = index;
            FD_SET(index, block_map);
            disk->ops->write(disk, 1 + super_blk.inode_map_sz, super_blk.block_map_sz, block_map);
        }
        int *indirect2 = malloc(sizeof(int) * 256);
        if (disk->ops->read(disk, _inode.indir_2, 1, indirect2) < 0) {
            exit(1);
        }

        if (!indirect2[n/256]) {
            int index = getFreeBlockIndex();
            if (result < 0) return -ENOSPC;
            indirect2[n/256] = index;
            FD_SET(index, block_map);
            disk->ops->write(disk, 1 + super_blk.inode_map_sz, super_blk.block_map_sz, block_map);
        }

        if (disk->ops->write(disk, _inode.indir_2, 1, indirect2) < 0) {
            exit(1);
        }

        int *double_indirect2 = malloc(sizeof(int) * 256);
        if (disk->ops->read(disk, indirect2[n/256], 1, double_indirect2) < 0) {
            exit(1);
        }

        result = double_indirect2[n%256];
        if (!result) {
            result = getFreeBlockIndex();
            if (result < 0) return -ENOSPC;
            double_indirect2[n%256] = result;
            FD_SET(result, block_map);
            disk->ops->write(disk, 1 + super_blk.inode_map_sz, super_blk.block_map_sz, block_map);
        }

        if (disk->ops->write(disk, indirect2[n/256], 1, double_indirect2) < 0) {
            exit(1);
        }

        inodes[inum] = _inode;
        free(double_indirect2);
        free(indirect2);
        return result;
    }
}
/* read - read data from an open file.
 * should return exactly the number of bytes requested, except:
 *   - if offset >= file len, return 0
 *   - if offset+len > file len, return bytes from offset to EOF
 *   - on error, return <0
 * Errors - path resolution, ENOENT, EISDIR
 */
static int fs_read(const char *path, char *buf, size_t len, off_t offset,
  struct fuse_file_info *fi)
{
    int i;
    int inum;
    int bytesToRead = (int) len;
    int total = len;

    inum = translate(path);

    if (inum < 0) {
        return -ENOENT;
    } else if (S_ISDIR(inodes[inum].mode)) {
        return -EISDIR;
    }

    int file_len = inodes[inum].size;
    if ( offset >= file_len ) {
        if (file_len) printf("\n");
        return 0;
    }
    if (offset + len >= file_len) total = file_len - offset;
    int counter = 0;

    while (total > 0) {
        int blk_offset = offset/1024;
        int blk_number = get_blk(inum, blk_offset, 0);

        if (!blk_number) return -EINVAL;

        char *tempBuffer = (char*) malloc(FS_BLOCK_SIZE);
        bzero(tempBuffer, FS_BLOCK_SIZE);

        if (disk->ops->read(disk, blk_number, 1, tempBuffer)) {
            exit(1);
        }

        bytesToRead = ( (blk_offset + 1) * FS_BLOCK_SIZE ) - offset;
        i = offset % FS_BLOCK_SIZE;
        if (bytesToRead > total) {
            bytesToRead = total;
        }

        // strncpy( buf , tempBuffer + i, bytesToRead);
        memcpy(buf + counter, tempBuffer + i, bytesToRead);

        // buf += bytesToRead;
        counter += bytesToRead;
        offset += bytesToRead;
        total -= bytesToRead;
        free(tempBuffer);
    }

    return counter;
}

/* write - write data to a file
 * It should return exactly the number of bytes requested, except on
 * error.
 * Errors - path resolution, ENOENT, EISDIR
 *  return EINVAL if 'offset' is greater than current file length.
 *  (POSIX semantics support the creation of files with "holes" in them,
 *   but we don't)
 */
static int fs_write(const char *path, const char *buf, size_t len,
 off_t offset, struct fuse_file_info *fi)
{
    int inum;
    int blk;
    int i;
    int counter;

    int bytes_to_write = 0;
    int total = len;

    inum = translate(path);

    if (inum < 0) {
        return -ENOENT;
    } else if (S_ISDIR(inodes[inum].mode)) {
        return -EISDIR;
    }

    if ( offset > inodes[inum].size ) {
        return -EINVAL;
    }
    counter = 0;

    while (total) {

        int blk_offset = offset/FS_BLOCK_SIZE;
        blk = get_blk(inum, blk_offset, 1);

        if (blk == 0) return -EINVAL;
        if (blk < 0) return -ENOSPC;

        char *tempBuffer = malloc(FS_BLOCK_SIZE) ;
        if (disk->ops->read(disk, blk, 1, tempBuffer) < 0) {
            exit(1);
        }

        bytes_to_write = ( (blk_offset + 1) * FS_BLOCK_SIZE ) - offset;
        i = offset % FS_BLOCK_SIZE;
        if (bytes_to_write > total) {
            bytes_to_write = total;
        }
        // memset(TEMP + iOffset, buf[offset] , bytes_to_write);
        // strncpy(TEMP, buf + offset, bytes_to_write);
        // for (i = iOffset; i < iOffset + bytes_to_write; ++i)
        // {
        //     tempBuffer[i] = buf[bytesWritten++];
        // }
        memcpy(tempBuffer + i, buf + counter, bytes_to_write);

        if (disk->ops->write(disk, blk, 1, tempBuffer) < 0) {
            exit(1);
        }

        // bytesWritten+= bytes_to_write;
        offset += bytes_to_write;
        counter += bytes_to_write;
        total-= bytes_to_write;
        free(tempBuffer);
        updateDisk();
    }

    inodes[inum].size += counter;
    updateDisk();

    return counter;
    // return -EOPNOTSUPP;
}

static int fs_open(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

static int fs_release(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

//count free block number
static int blocksFree(struct fs_super sb)
{
    int result = 0;
    int i;
    for (i = 0; i < sb.block_map_sz * 8192; i++)
        if (FD_ISSET(i, block_map)) {
            result++;
        }
        return sb.num_blocks - result;
}


/* statfs - get file system statistics
 * see 'man 2 statfs' for description of 'struct statvfs'.
 * Errors - none.
 */
    static int fs_statfs(const char *path, struct statvfs *st)
    {
    /* needs to return the following fields (set others to zero):
     *   f_bsize = BLOCK_SIZE
     *   f_blocks = total image - metadata
     *   f_bfree = f_blocks - blocks used
     *   f_bavail = f_bfree
     *   f_namelen = <whatever your max namelength is>
     *
     * this should work fine, but you may want to add code to
     * calculate the correct values later.
     */
        struct fs_super sb;
        if (disk->ops->read(disk, 0, 1, &sb) < 0)
            exit(1);

        int numOfFreeBlocks = blocksFree(sb);
        st->f_bsize = FS_BLOCK_SIZE;
        st->f_blocks = sb.num_blocks;           /* probably want to */
        st->f_bfree = numOfFreeBlocks;            /* change these */
        st->f_bavail = numOfFreeBlocks;           /* values */
        st->f_namemax = 27;

        return 0;
    }

/* operations vector. Please don't rename it, as the skeleton code in
 * misc.c assumes it is named 'fs_ops'.
 */
    struct fuse_operations fs_ops = {
        .init = fs_init,
        .getattr = fs_getattr,
        .opendir = fs_opendir,
        .readdir = fs_readdir,
        .releasedir = fs_releasedir,
        .mknod = fs_mknod,
        .mkdir = fs_mkdir,
        .unlink = fs_unlink,
        .rmdir = fs_rmdir,
        .rename = fs_rename,
        .chmod = fs_chmod,
        .utime = fs_utime,
        .truncate = fs_truncate,
        .open = fs_open,
        .read = fs_read,
        .write = fs_write,
        .release = fs_release,
        .statfs = fs_statfs,
    };
