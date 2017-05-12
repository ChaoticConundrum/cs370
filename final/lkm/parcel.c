#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/string.h>
#include <linux/uuid.h>

#include "parcel.h"

void parcel_parse_super(struct treefs_super *sb, const char *data){
    sb->magic =     be32_to_cpu(*(__be32 *)(data));
    sb->version =   data[7];
    sb->flags =     be32_to_cpu(*(__be32 *)(data + 8));
    sb->treehead =  be64_to_cpu(*(__be64 *)(data + 12));
    sb->freehead =  be64_to_cpu(*(__be64 *)(data + 20));
    sb->freetail =  be64_to_cpu(*(__be64 *)(data + 28));
    sb->tail =      be64_to_cpu(*(__be64 *)(data + 36));
    memcpy(sb->rootid, data + 44, 16);
    sb->crc =       be32_to_cpu(*(__be32 *)(data + 60));
}

void parcel_parse_treenode(struct treefs_tree_node *tn, const char *data){
    tn->magic =     be32_to_cpu(*(__be32 *)(data));
    memcpy(tn->uid, data + 4, 16);
    tn->lnode =     be64_to_cpu(*(__be64 *)(data + 20));
    tn->rnode =     be64_to_cpu(*(__be64 *)(data + 28));
    tn->type =      data[36];
    tn->extra =     data[37];
    tn->crc =       be32_to_cpu(*(__be32 *)(data + 38));
    memcpy(tn->payload, data + 42, 16);

    if(tn->type >= BLOBOBJ){
        tn->data.offset =   be64_to_cpu(*(__be64 *)(tn->payload));
        tn->data.size=      be64_to_cpu(*(__be64 *)(tn->payload + 8));
    }
}
