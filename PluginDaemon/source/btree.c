/*=======================================================
btree.c

A binary tree implmentation
=======================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include "btree.h"
#include "misc.h"

void msgAssert(char condition, const char *msg) {

  if (!condition) {
    SYSLOG(LOG_CRIT, "%s", msg);
    exit(1);
  }
}


/*=======================================================
Private Code.
=======================================================*/
static BTreeNode_t *_btree_find(BTreeNode_t **tn, const BTreeNode_comparator nodeCompare, const BTreeNode_t *key) {

  int comparison = 0;
  BTreeNode_t *result = NULL;

  /*hit end of tree, nothing found*/
  if (!*tn)
    return NULL;

  comparison = nodeCompare(*tn, key);
  if (comparison > 0)
    result = _btree_find(&(*tn)->left, nodeCompare, key);
  else if (comparison < 0)
    result = _btree_find(&(*tn)->right, nodeCompare, key);
  else
    result = (*tn);

  return result;
}


static void _btree_insert(BTreeNode_t **tn, const BTreeNode_comparator nodeCompare, BTreeNode_t *newNode) {

  int comparison = 0;

  if (!*tn) {
    *tn = newNode;
    return;
  }

  comparison = nodeCompare(*tn, newNode);
  if (comparison > 0)
    _btree_insert(&(*tn)->left, nodeCompare, newNode);
  else if (comparison < 0)
    _btree_insert(&(*tn)->right, nodeCompare, newNode);

  return;
}

static void _btree_print(BTreeNode_t *start, int (*print)(const char *, ...), const BTree_PrintData printData) {

  if (!start) return;

  _btree_print(start->left, print, printData);

  print("\t%d\t", (size_t) start->data);
  printData(start->data, print);
  print("\n");

  _btree_print(start->right, print, printData);
}


static int _btree_forEach(BTreeNode_t *start, BTreeNode_Operator op, void *data) {

  if (!start) return 0;

  int lstatus = _btree_forEach(start->left, op, data);
  if (lstatus) return lstatus;

  int opstatus = op(BTreeNode_getData(start), data);
  if (opstatus) return opstatus;

  int rstatus = _btree_forEach(start->right, op, data);
  if (rstatus) return rstatus;

  return 0;
}


static void _btree_destroy(BTreeNode_t *start, const BTreeNode_rmNodeData rmData) {

  BTreeNode_t *left, *right;
  if (!start) return;
  /*need to copy these since we are deleting the node*/
  left = start->left;
  right = start->right;

  _btree_destroy(left, rmData);

  BTreeNode_destroy(start, rmData);

  _btree_destroy(right, rmData);
}

static BTreeNode_t *_btree_remove(BTreeNode_t **tree, const BTreeNode_rmNodeData rmData,
                                  const BTreeNode_comparator nodeCompare, const BTreeNode_t *key) {

  if (!*tree) return *tree;

  int comparison = 0;

  comparison = nodeCompare(*tree, key);
  if (comparison > 0)
    (*tree)->left = _btree_remove(&(*tree)->left, rmData, nodeCompare, key);
  else if (comparison < 0)
    (*tree)->right = _btree_remove(&(*tree)->right, rmData, nodeCompare, key);
  else {

    if ((*tree)->left == NULL) {
      BTreeNode_t *temp = (*tree)->right;
      BTreeNode_destroy(*tree, rmData);
      return temp;
    }
    else if ((*tree)->right == NULL) {

      BTreeNode_t *temp = (*tree)->left;
      BTreeNode_destroy(*tree, rmData);
      return temp;
    }
    //otherwise, node has two children
    BTreeNode_t *successor = (*tree)->right;
    while (successor->left != NULL) {
      successor = successor->left;
    }

    (*tree)->data = successor->data;
    (*tree)->right = _btree_remove(&(*tree)->right, rmData, nodeCompare, key);
  }

  return *tree;
}

/*=======================================================
Public code - BTreeNode_t
=======================================================*/
BTreeNode_t *BTreeNode_create(void *data) {

  BTreeNode_t *node = NULL;
  msgAssert(data != NULL, "Error: creating node with NULL data.");

  node = calloc(1, sizeof(BTreeNode_t));
  msgAssert(node != NULL, "Error allocating tree node.");

  node->data = data;
  return node;
}

void BTreeNode_destroy(BTreeNode_t *node, const BTreeNode_rmNodeData rmData) {

  if (!node) return;

  rmData(node->data);
  node->data = NULL;
  node->left = NULL;
  node->right = NULL;
  free(node);
}


void *BTreeNode_getData(const BTreeNode_t *node) {

  return node->data;
}


/*=======================================================
Public code - BTree_t
=======================================================*/
BTree_t *BTree_create(const BTreeNode_comparator fn, const BTreeNode_rmNodeData fn2) {

  BTree_t *tree = calloc(1, sizeof(BTree_t));
  msgAssert(tree != NULL, "Error allocating tree.");

  tree->head = NULL;
  tree->nodeCompare = fn;
  tree->rmNodeData = fn2;
  return tree;
}

void BTree_destroy(BTree_t *tree) {

  if (!tree) return;

  _btree_destroy(tree->head, tree->rmNodeData);
  tree->size = 0;
  tree->nodeCompare = NULL;
  tree->rmNodeData = NULL;

  free(tree);
}


void BTree_add(BTree_t *tree, BTreeNode_t *node) {

  if (tree && node) {
    _btree_insert(&tree->head, tree->nodeCompare, node);
    tree->size++;
  }
}


BTreeNode_t *BTree_find(BTree_t *tree, const BTreeNode_t *key) {

  if (tree && key)
    return _btree_find(&tree->head, tree->nodeCompare, key);

  return NULL;
}

void BTree_print(const BTree_t *tree, int (*print)(const char *, ...), const BTree_PrintData printData) {
  /*if no print functions are specified, no point in visiting
    each node to not print anything*/
  if (tree && print && printData)
    _btree_print(tree->head, print, printData);
}

int BTree_forEach(const BTree_t *tree, BTreeNode_Operator operation, void *data) {

  if (tree && operation) {
    return _btree_forEach(tree->head, operation, data);
  }

  return -1;
}


int BTree_getSize(const BTree_t *tree) {

  return tree->size;
}

void BTree_rmNode(BTree_t *tree, const BTreeNode_t *key) {

  tree->head = _btree_remove(&tree->head, tree->rmNodeData, tree->nodeCompare, key);
}