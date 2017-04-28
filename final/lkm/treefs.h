
#define TFS_LOG "TreeFS: "

#define TREEFS_TREENODE_MAGIC   "TREENODE"
#define TREEFS_OBJNODE_MAGIC    "NODE

static unsigned long TREEFS_MAGIC = 0x5452eef5;
static unsigned TREEFS_BLOCK_SIZE = 4096;

#define TREEFS_SB(sb)((struct treefs_super *)sb->s_fs_info)

// //////////////////////////////////////////////////////////////////////////

struct treefs_super {
    unsigned long magic;
    u8 version;
    unsigned block_size;
    u64 treehead;
    u64 freehead;
    u64 freetail;
    u64 tail;
};

enum treefs_object_types {
    NULLOBJ = 0,
    BOOLOBJ,        //!< Boolean object. 1-bit.
    UINTOBJ,        //!< Unsigned integer object. 64-bit.
    SINTOBJ,        //!< Signed integer object. 64-bit.
    FLOATOBJ,       //!< Floating point number object. Double precision.
    ZUIDOBJ,        //!< UUID object.
    BLOBOBJ,        //!< Binary blob object.
    STRINGOBJ,      //!< String object.
    FILEOBJ,        //!< File object. Includes embedded filename and file content.
    DIROBJ,         //!< Directory object.
};

struct treefs_tree_node {
    u8 uid[16];
    u64 lnode;
    u64 rnode;
    u8 type;
    u8 flags;
    u64 offset;
    u16 crc;
};

struct treefs_object_node {
    u8 uid[16];
};

// //////////////////////////////////////////////////////////////////////////

