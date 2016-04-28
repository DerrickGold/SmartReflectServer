//
// Created by Derrick on 2016-03-18.
//
#include <stdio.h>
#include <syslog.h>
#include "plugin.h"
#include "btree.h"
#include "misc.h"

static BTree_t *allPlugins = NULL;
/*=======================================================================================
Binary Tree accessors
=======================================================================================*/

//How to compare one plugin with another in a binary tree
int PluginNode_comparator(const BTreeNode_t *n1, const BTreeNode_t *n2) {

  return strcmp(
          Plugin_GetName(BTreeNode_getData(n1)),
          Plugin_GetName(BTreeNode_getData(n2)));
}

//How to cleanup the data associated with a node in a binary tree
void PluginNode_destroy(void *data) {

  if (!data) {
    SYSLOG(LOG_ERR, "PluginNode_Destroy: deleting null data!");
    return;
  }

  Plugin_Free((Plugin_t *) data, 1);
}


//Instantiates a binary tree node containing the specified plugin
BTreeNode_t *PluginNode_create(Plugin_t *plugin) {

  if (!plugin) {
    SYSLOG(LOG_ERR, "PluginNode_Create: creating node with empty plugin!");
    return NULL;
  }

  return BTreeNode_create(plugin);
}

//Creates a binary tree node that is used for searching the binary tree
BTreeNode_t *PluginNode_searchKey(const char *searchTerm) {

  Plugin_t *temp = Plugin_Create();
  if (!temp) return NULL;
  if (Plugin_SetName(temp, searchTerm)) return NULL;
  return PluginNode_create(temp);
}


//Initialize a binary tree for storing plugins
int PluginList_Init(void) {

  allPlugins = BTree_create(&PluginNode_comparator, &PluginNode_destroy);
  if (!allPlugins) {
    SYSLOG(LOG_ERR, "PluginList_Init: error initializing binary tree for plugins");
    return -1;
  }
  return 0;
}

//Add plugin to internalized binary tree
void PluginList_Add(Plugin_t *plugin) {

  if (!allPlugins) {
    SYSLOG(LOG_ERR, "PluginList_Add: plugin list hasn't been initialized yet");
    return;
  }
  BTree_add(allPlugins, PluginNode_create(plugin));
}

//Retrieve a specific plugin based on plugin name
Plugin_t *PluginList_Find(const char *pluginName) {

  BTreeNode_t *result = NULL;
  BTreeNode_t *keyNode = PluginNode_searchKey(pluginName);
  if (!keyNode) {
    SYSLOG(LOG_ERR, "PluginList_Find: error creating search key node.");
    return NULL;
  }

  result = BTree_find(allPlugins, keyNode);
  BTreeNode_destroy(keyNode, &PluginNode_destroy);

  if (!result) return NULL;
  return (Plugin_t *) BTreeNode_getData(result);
}

void PluginList_Delete(const char *pluginName) {

  BTreeNode_t *keyNode = PluginNode_searchKey(pluginName);
  if (!keyNode) {
    SYSLOG(LOG_ERR, "PluginList_Delete: error creating search key node.");
    return;
  }
  BTree_rmNode(allPlugins, keyNode);

  //done with search key, destroy it
  BTreeNode_destroy(keyNode, &PluginNode_destroy);
}

//Cleanup plugin list
void PluginList_Free(void) {

  BTree_destroy(allPlugins);
}

/*
  Apply an action to each plugin in the binary tree.

  operation is a function pointer in which:
    -the first argument contains the current Plugin_t struct to operate on
    -the second argument is any data passed into 'PluginList_ForEach' second
      data argument


*/
int PluginList_ForEach(int (*operation)(void *, void *), void *data) {

  return BTree_forEach(allPlugins, (BTreeNode_Operator) operation, data);
}

int PluginList_GetCount(void) {

  return BTree_getSize(allPlugins);
}