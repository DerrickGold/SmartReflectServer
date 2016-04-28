#ifndef _BTREE_H_
#define _BTREE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

typedef struct _BTreeNode {
    void *data;

    struct _BTreeNode *left;
    struct _BTreeNode *right;
} BTreeNode_t;

typedef void (*BTreeNode_rmNodeData)(void *);

typedef int (*BTreeNode_comparator)(const BTreeNode_t *, const BTreeNode_t *);

/*for storing all labels and their values*/
typedef struct {
    BTreeNode_t *head;
    int size;

    /*BTreeNode_getKey getNodeKey;*/
    BTreeNode_rmNodeData rmNodeData;
    BTreeNode_comparator nodeCompare;
} BTree_t;

typedef void (*BTree_PrintData)(void *, int (*print)(const char *, ...));

typedef int (*BTreeNode_Operator)(void *, void *);


extern void BTree_print(const BTree_t *tree, int (*print)(const char *, ...), const BTree_PrintData printData);

extern void BTree_add(BTree_t *tree, BTreeNode_t *node);

extern BTreeNode_t *BTree_find(BTree_t *tree, const BTreeNode_t *key);

extern BTree_t *BTree_create(const BTreeNode_comparator fn, const BTreeNode_rmNodeData fn2);

extern void BTree_destroy(BTree_t *tree);

extern BTreeNode_t *BTreeNode_create(void *data);

extern void *BTreeNode_getData(const BTreeNode_t *node);

extern void BTreeNode_destroy(BTreeNode_t *node, const BTreeNode_rmNodeData rmData);

extern int BTree_forEach(const BTree_t *tree, BTreeNode_Operator operation, void *data);

extern int BTree_getSize(const BTree_t *tree);

extern void BTree_rmNode(BTree_t *tree, const BTreeNode_t *key);


#ifdef __cplusplus
}
#endif

#endif
