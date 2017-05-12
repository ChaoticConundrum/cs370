#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/string.h>
#include <linux/uuid.h>
#include <linux/crypto.h>

#include "treefs.h"

MODULE_DESCRIPTION("TreeFS Filesystem Driver");
MODULE_AUTHOR("Charlie Waters");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

// //////////////////////////////////////////////////////////////////////////

static ssize_t treefs_read(struct file *file, char *buf, size_t size, loff_t *loff){
    pr_debug(TFS_LOG "file read\n");

    return 0;
}

static int treefs_iterate(struct file *file, struct dir_context *ctx){
    pr_debug(TFS_LOG "dir iterate\n");

    return 0;
}

static struct file_operations treefs_file_ops = {
    .llseek = generic_file_llseek,
    .read = treefs_read,
    .read_iter = generic_file_read_iter,
    .mmap = generic_file_mmap,
    .splice_read = generic_file_splice_read
};

static struct file_operations treefs_dir_ops = {
    .llseek = generic_file_llseek,
    .read = generic_read_dir,
    .iterate = treefs_iterate,
};

// //////////////////////////////////////////////////////////////////////////

static struct dentry *treefs_lookup(struct inode *dir, struct dentry *dentry, unsigned flags){
    pr_debug(TFS_LOG "inode lookup\n");

    struct inode *inode;

    return NULL;
}

static struct inode_operations treefs_dir_inode_ops = {
    .lookup = treefs_lookup,
};

static struct inode *treefs_inode_get(struct super_block *sb, u8 *id){
    struct treefs_super *trsb = TREEFS_SB(sb);
    struct buffer_head *bh;
    struct inode *inode;
    struct treefs_inode *ti;

    inode = sb->s_op->alloc_inode(sb);


    if(S_ISREG(inode->i_mode)){
        inode->i_fop = &treefs_file_ops;
    } else {
        inode->i_op = &treefs_dir_inode_ops;
        inode->i_fop = &treefs_dir_ops;
    }

    ti = TREEFS_IN(inode);
    memcpy(ti->tn.uid, id, 16);

    return inode;
}

// //////////////////////////////////////////////////////////////////////////

static struct inode *treefs_inode_alloc(struct super_block *sb){
    struct treefs_inode *inode = (struct treefs_inode *)kzalloc(sizeof(struct treefs_inode), GFP_NOFS);
    pr_debug(TFS_LOG "inode alloc\n");

    if(!inode)
        return NULL;
    return &inode->in;
}

static void treefs_inode_free(struct inode *in){
    kfree(in);

    pr_debug(TFS_LOG "inode free\n");
}

// //////////////////////////////////////////////////////////////////////////

// Sync and destroy superblock
static void treefs_put_super(struct super_block *sb){
    struct treefs_super *trsb = TREEFS_SB(sb);

    // free super
    if(trsb)
        kfree(trsb);
    sb->s_fs_info = NULL;

    pr_debug(TFS_LOG "super block destroyed\n");
}

// Superblock operations function pointers
static struct super_operations const treefs_super_ops = {
    .alloc_inode = treefs_inode_alloc,
    .destroy_inode = treefs_inode_free,
    .put_super = treefs_put_super,
};

// Init superblock fields
static void treefs_init_sb(struct treefs_super *trsb, char *data){
    parcel_parse_super(trsb, data);
    if(trsb->magic != TREEFS_MAGIC)
        pr_err(TFS_LOG "bad super magic\n");
    trsb->block_size = TREEFS_BLOCK_SIZE;
}

// Read superblock from disk
static struct treefs_super *treefs_read_super(struct super_block *sb){
    struct treefs_super *trsb = (struct treefs_super *)kzalloc(sizeof(struct treefs_super), GFP_NOFS);
    if(!trsb){
        pr_err(TFS_LOG "cannot alloc super\n");
        return NULL;
    }

    struct buffer_head *bh = sb_bread(sb, 0);
    if(!bh){
        pr_err(TFS_LOG "failed reading block 0");
        kfree(trsb);
        return NULL;
    }

    treefs_init_sb(trsb, bh->b_data);

    return trsb;
}

// Init superblock
static int treefs_fill_super(struct super_block *sb, void *data, int silent){
    // read superblock
    struct treefs_super *trsb = treefs_read_super(sb);
    if(!trsb){
        return -EINVAL;
    }

    // fill sb
    sb->s_magic = trsb->magic;
    sb->s_fs_info = trsb;           // fs private data
    sb->s_op = &treefs_super_ops;

    if(sb_set_blocksize(sb, trsb->block_size) == 0){
        pr_err(TFS_LOG "device does not support block size");
        return -EINVAL;
    }

    // alloc root inode
    struct inode *root = treefs_inode_get(sb, trsb->rootid);
    if(IS_ERR(root)){
        pr_err(TFS_LOG "inode get failed\n");
        return PTR_ERR(root);
    }

    sb->s_root = d_make_root(root);
    if(!sb->s_root){
        pr_err(TFS_LOG "make root failed\n");
        return -ENOMEM;
    }

    return 0;
}

// //////////////////////////////////////////////////////////////////////////

// Mount handler
static struct dentry *treefs_mount(struct file_system_type *type,
                                     int flags,
                                     char const *dev,
                                     void *data)
{
    struct dentry *const entry = mount_bdev(type, flags, dev, data, treefs_fill_super);

    if(IS_ERR(entry))
        pr_err(TFS_LOG "Mount Failed\n");
    else
        pr_debug(TFS_LOG "Mounted\n");

    return entry;
}

// File system type registration structure
static struct file_system_type parcelfs_type = {
    .owner = THIS_MODULE,
    .name = "treefs",
    .mount = treefs_mount,
    .kill_sb = kill_block_super,
    .fs_flags = FS_REQUIRES_DEV,
};

// Init function
static int __init treefs_init(void){
    int ret = register_filesystem(&parcelfs_type);
    if(ret != 0){
        pr_err(TFS_LOG "Error Loading\n");
        return ret;
    }
    pr_debug(TFS_LOG "Loaded\n");
    return 0;
}

// Exit function
static void __exit treefs_exit(void){
    pr_debug(TFS_LOG "Unloaded\n");
}

// Register functions
module_init(treefs_init);
module_exit(treefs_exit);

