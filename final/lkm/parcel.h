
static unsigned long TREEFS_MAGIC       = 0x5452eef5;
static unsigned long TREEFS_TREE_MAGIC  = 0x54524545;
static unsigned long TREEFS_FREE_MAGIC  = 0x66726565;

enum treefs_object_types {
    NULLOBJ = 0,
    BOOLOBJ,        //!< Boolean object. 1-bit.
    UINTOBJ,        //!< Unsigned integer object. 64-bit.
    SINTOBJ,        //!< Signed integer object. 64-bit.
    FLOATOBJ,       //!< Floating point number object. Double precision.
    ZUIDOBJ,        //!< UUID object.
    BLOBOBJ,        //!< Binary blob object.
    STRINGOBJ,      //!< String object.
    LISTOBJ,        //!< List object. Ordered list of UUIDs.
    FILEOBJ,        //!< File object. Includes embedded filename and file content.
};

struct treefs_super {
    u32 magic;
    u8 version;
    u32 flags;
    u64 treehead;
    u64 freehead;
    u64 freetail;
    u64 tail;
    u8 rootid[16];
    u32 crc;

    unsigned block_size;
};

struct treefs_tree_node {
    u32 magic;
    u8 uid[16];
    u64 lnode;
    u64 rnode;
    u8 type;
    u8 extra;
    u32 crc;
    u8 payload[16];

    struct {
        u64 offset;
        u64 size;
    } data;
};

struct treefs_free_node {
    u32 magic;
    u64 next;
    u64 size;
    u32 crc;
};

void parcel_parse_super(struct treefs_super *sb, const char *data);
