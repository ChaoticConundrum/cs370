#include "parcel.h"

#define TFS_LOG "TreeFS: "

static unsigned TREEFS_BLOCK_SIZE = 4096;

#define TREEFS_SB(sb)((struct treefs_super *)sb->s_fs_info)
#define TREEFS_IN(in)((struct treefs_inode *)in)

// //////////////////////////////////////////////////////////////////////////

struct treefs_inode {
    struct inode in;
    struct treefs_tree_node tn;
};

// //////////////////////////////////////////////////////////////////////////

