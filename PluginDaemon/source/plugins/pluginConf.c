//
// Created by Derrick on 2016-03-18.
//

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <limits.h>
#include "plugin.h"
#include "configReader.h"
#include "misc.h"

#define PLUGIN_CONF_FILE "/" PLUGIN_CONF_FILENAME
#define PLUGIN_CONF_OUT "/" PLUGIN_CONF_OUTFILE

#define PLUGIN_CONF_HASH_SIZE 13

//Used for tagging original read settings in hash table keys
#define PLUGIN_CONF_ORIGTAG "[r]"
#define PLUGIN_CONF_TAG_DELIM ':'
#define PLUGIN_CONF_OPT_TRUE "true"

#define PLUGIN_CONF_MAX_PATHLIST 128
static char *pathList[128];

typedef enum {
    TYPE_NOTPATH = 0,
    TYPE_FILEPATH,
    TYPE_SCRIPTPATH,
} ConfigPropertyType_e;


static char *configGetString(Plugin_t *plugin, char *setting) {

  HashData_t *entry = HashTable_find(plugin->config.table, setting);
  if (!entry)
    return NULL;

  return entry->value;
}

//For settings that might have multiple definitions, need to do partial key search
//and return list of results
static char **configGetAllStrings(Plugin_t *plugin, char *partialKey, int *count) {

  HashTable_t *table = plugin->config.table;
  if (!table || !table->entries)
    return NULL;


  SYSLOG(LOG_INFO, "CONFIG PARTIAL MATCHING: %s", partialKey);

  int curCount = 0;
  //loop through all symbol table entries
  size_t i = 0;
  for (i = 0; i < table->size; i++) {

    //skip empty entries
    if (!table->entries[i])
      continue;

    //skip entries where key may not be set (malformed entries)
    HashData_t *entry = table->entries[i];
    if (!entry->key)
      continue;

    SYSLOG(LOG_INFO, "Comparing %s to %s", entry->key, partialKey);
    if (!strncmp(entry->key, partialKey, strlen(partialKey))) {
      //entry has been found
      SYSLOG(LOG_INFO, "Partial match made: %s", entry->value);
      pathList[curCount] = entry->value;
      curCount++;
    }
  }

  //set the entry in the list after the last item as null
  pathList[curCount] = NULL;

  char **newPathList = calloc(1, sizeof(char *) * curCount);
  if (!newPathList) {
    SYSLOG(LOG_ERR, "Config Path Listing failed to allocate.");
    return NULL;
  }

  for (i = 0; i < curCount; i++)
    newPathList[i] = pathList[i];


  //return count
  if (count)
    *count = curCount;

  //and list
  return newPathList;
}

static char *makeAbsPath(Plugin_t *plugin, char *path) {

  SYSLOG(LOG_INFO, "makeAbsPath: original path: %s", path);
  if (!plugin || !path) {
    SYSLOG(LOG_ERR, "makeAbsPath: No path or plugin provided");
    return NULL;
  }

  //first check if path is already absolute
  int isRelPath = (*path != '/');


  //otherwise, make path absolute
  char *dir = Plugin_GetDirectory(plugin);

  size_t pathLen = strlen(path) + 1;

  //if relative path, add the root dir and divider
  if (isRelPath)
    pathLen += strlen(dir) + strlen("/");

  char *absPath = calloc(pathLen + 1, sizeof(char));
  if (!absPath) {
    SYSLOG(LOG_ERR, "makeAbsPath: Error allocating memory for path %s", path);
    return NULL;
  }

  //copy path over
  if (isRelPath)
    snprintf(absPath, pathLen, "%s/%s", dir, path);
  else
    strncpy(absPath, path, pathLen);

  SYSLOG(LOG_INFO, "makeAbsPath: Created path %s", absPath);
  return absPath;
}


static ConfigPropertyType_e getKnownSettingType(char *property) {

  ConfigPropertyType_e type = TYPE_NOTPATH;
  //element it coresponds to.
  if (!strncmp(property, PLUGIN_CONF_TAG_HTML, strlen(property))) {
    type = TYPE_FILEPATH;
  }
  else if (!strncmp(property, PLUGIN_CONF_TAG_SCRIPT, strlen(property))) {
    type = TYPE_SCRIPTPATH;
  }
    //webgui property is a file path that needs to be escaped
  else if (!strncmp(property, PLUGIN_CONF_WEBGUI, strlen(property))) {
    type = TYPE_FILEPATH;
  }
    //these two settings can have multiple values assigne dto them
  else if (!strncmp(property, PLUGIN_CONF_TAG_JS, strlen(PLUGIN_CONF_TAG_JS))) {
    type = TYPE_FILEPATH;
  }
  else if (!strncmp(property, PLUGIN_CONF_TAG_CSS, strlen(PLUGIN_CONF_TAG_CSS))) {
    type = TYPE_FILEPATH;
  }

  return type;
}

static int pluginStoreSettingWithType(Plugin_t *plugin, char *property, char *value, ConfigPropertyType_e fileType) {

  //store original config value into config hash for plugin
  size_t origKeyLen = strlen(property) + strlen(PLUGIN_CONF_ORIGTAG) + 1;
  char origKey[origKeyLen];
  snprintf(origKey, origKeyLen, "%s%s", PLUGIN_CONF_ORIGTAG, property);

  HashData_t *newSetting = HashData_create(origKey, value);
  if (!newSetting) {
    SYSLOG(LOG_INFO, "Plugin_Conf_Apply: Failed adding config option to hash: %s:%s", origKey, value);
    return -1;
  }

  HashTable_add(plugin->config.table, newSetting);


  HashData_t *appliedSetting = NULL;
  //store the applied setting without the original tag into the hash
  switch (fileType) {
    case TYPE_NOTPATH:
    default:
      //no need to escape any paths
      appliedSetting = HashData_create(property, value);
      if (appliedSetting)
        HashTable_add(plugin->config.table, appliedSetting);
      break;

    case TYPE_FILEPATH: {
      //make all file paths absolute
      char *absPath = makeAbsPath(plugin, value);
      if (absPath) {
        appliedSetting = HashData_create(property, absPath);
        if (appliedSetting)
          HashTable_add(plugin->config.table, appliedSetting);

        free(absPath);
      }
    }
      break;

    case TYPE_SCRIPTPATH: {
      //script paths will be made absolute
      char *absPath = makeAbsPath(plugin, value);
      if (absPath) {
        appliedSetting = HashData_create(property, absPath);
        if (appliedSetting)
          HashTable_add(plugin->config.table, appliedSetting);


        //then an escaped version of the path will be stored
        char *escaped = ConfigReader_escapePath(absPath);
        free(absPath);

        if (escaped) {
          appliedSetting = HashData_create(PLUGIN_CONF_TAG_SCRIPT_ESCAPED, escaped);
          free(escaped);

          if (appliedSetting)
            HashTable_add(plugin->config.table, appliedSetting);
        }
      }
    }
      break;
  }

  return 0;
}


/*
  Applies a value to the plugin struct based on property name from the
  plugin.conf file.
*/
int Plugin_confApply(void *data, char *property, char *value) {

  if (!data) return -1;

  Plugin_t *plugin = (Plugin_t *) data;

  if (!property) return 0;


  ConfigPropertyType_e fileType = getKnownSettingType(property);
  SYSLOG(LOG_INFO, "Plugin_Conf_Apply: Initial: attr: %s, value: %s", property, value);




  //Based on which property is being set, change pathPtr to the struct
  //element it coresponds to.
  if (!strncmp(property, PLUGIN_CONF_TAG_SCRIPT_TIME, strlen(property))) {
    if (value == NULL || strlen(value) == 0) return 0;

    plugin->config.periodLen = atoi(value);

    //negative values will only run the script once
    if (plugin->config.periodLen < 0) {
      plugin->config.periodLen = -plugin->config.periodLen; //make it positive
      plugin->flags |= PLUGIN_FLAG_SCRIPT_ONESHOT;
    }
  }
  else if (!strncmp(property, PLUGIN_CONF_TAG_SCRIPT_PROCESS, strlen(property))) {
    if (value == NULL || strlen(value) == 0) return 0;

    if (!strncmp(value, PLUGIN_CONF_TAG_SCRIPT_PROCESS_2, strlen(value))) {
      plugin->flags |= PLUGIN_FLAG_OUTPUT_CLEAR;
      plugin->flags &= ~PLUGIN_FLAG_OUTPUT_APPEND;
    }
    else {
      plugin->flags |= PLUGIN_FLAG_OUTPUT_APPEND;
      plugin->flags &= ~PLUGIN_FLAG_OUTPUT_CLEAR;
    }

  }
  else if (!strncmp(property, PLUGIN_CONF_TAG_SCRIPT_BACKGROUND, strlen(property))) {
    plugin->flags |= PLUGIN_FLAG_SCRIPT_BACKGROUND;

    //if no time has yet been defined for the background script, add a time
    //to get it into the scheduler so it can be executed
    if (!strncmp(value, PLUGIN_CONF_OPT_TRUE, strlen(value)) && !PluginConf_GetScriptPeriod(plugin))
      plugin->config.periodLen = 1;
  }
  else if (!strncmp(property, PLUGIN_CONF_START_ON_LOAD, strlen(property))) {
    //check if plugin is supposed to start once it is loaded.
    if (value != NULL && !strncmp(value, PLUGIN_CONF_OPT_TRUE, strlen(value)))
      PLUGIN_SET_ENABLED(plugin);
  }


  if (value == NULL || strlen(value) <= 0)
    return 0;


  pluginStoreSettingWithType(plugin, property, value, fileType);

  return 0;
}


int Plugin_initConfig(Plugin_t *plugin) {

  //allocate plugin configuration hash
  plugin->config.table = HashTable_init(PLUGIN_CONF_HASH_SIZE);
  if (!plugin->config.table) {
    SYSLOG(LOG_ERR, "Plugin Create: Error allocating plugin configuration hash.");
    return -1;
  }

  return 0;
}

int Plugin_loadConfig(Plugin_t *plugin) {

  if (!plugin) return -1;

  //build plugin file path
  size_t remainingSpace = PATH_MAX;

  //set base path
  char pluginFile[remainingSpace];
  strncpy(pluginFile, Plugin_GetDirectory(plugin), remainingSpace);

  //add file name
  remainingSpace -= strlen(Plugin_GetDirectory(plugin));
  if (remainingSpace <= 0) {
    SYSLOG(LOG_ERR, "Plugin Conf: basepath exceeds path buffer...");
    return -1;
  }
  strncat(pluginFile, PLUGIN_CONF_FILE, remainingSpace);

  //initialize config hash before reading in config file
  if (Plugin_initConfig(plugin))
    return -1;

  int status = ConfigReader_readConfig(pluginFile, Plugin_confApply, plugin);

  //print plugin hash table
  printf("========================\n");
  printf("%s\n", Plugin_GetName(plugin));
  printf("=========================\n");
  HashTable_print(stdout, plugin->config.table);

  return status;
}


int PluginConf_setValue(Plugin_t *plugin, char *property, char *value) {

  if (!plugin) return -1;

  //get old plugin config file path
  //set base path
  size_t remainingSpace = PATH_MAX;
  char pluginFile[remainingSpace];
  strncpy(pluginFile, Plugin_GetDirectory(plugin), remainingSpace);

  //add file name
  remainingSpace -= strlen(Plugin_GetDirectory(plugin));
  if (remainingSpace <= 0) {
    SYSLOG(LOG_ERR, "Plugin Conf: basepath exceeds path buffer...");
    return -1;
  }
  strncat(pluginFile, PLUGIN_CONF_FILE, remainingSpace);

  //get output filepath name
  remainingSpace = PATH_MAX;
  char outfile[remainingSpace];
  strncpy(outfile, Plugin_GetDirectory(plugin), remainingSpace);

  //add file name
  remainingSpace -= strlen(Plugin_GetDirectory(plugin));
  if (remainingSpace <= 0) {
    SYSLOG(LOG_ERR, "Plugin Conf: basepath exceeds path buffer...");
    return -1;
  }
  strncat(outfile, PLUGIN_CONF_OUT, remainingSpace);

  ConfigReader_writeConfig(outfile, pluginFile, property, value);
  return 0;
}


char **PluginConf_GetCSS(Plugin_t *plugin, int *count) {

  return configGetAllStrings(plugin, PLUGIN_CONF_TAG_CSS, count);
}

char **PluginConf_GetJS(Plugin_t *plugin, int *count) {

  return configGetAllStrings(plugin, PLUGIN_CONF_TAG_JS, count);
}

char *PluginConf_GetHTML(Plugin_t *plugin) {

  return configGetString(plugin, PLUGIN_CONF_TAG_HTML);
}


char *PluginConf_GetScript(Plugin_t *plugin) {

  return configGetString(plugin, PLUGIN_CONF_TAG_SCRIPT);
}

char *Plugin_GetDirectory(Plugin_t *plugin) {

  return plugin->basePath;
}

char *PluginConf_GetEscapeScript(Plugin_t *plugin) {

  char *escaped = configGetString(plugin, PLUGIN_CONF_TAG_SCRIPT_ESCAPED);
  if (!escaped)
    return PluginConf_GetScript(plugin);

  return escaped;
}

char *Plugin_GetWebProtocol(Plugin_t *plugin) {

  return Plugin_GetName(plugin);
}

char *Plugin_GetDaemonProtocol(Plugin_t *plugin) {

  return plugin->uuidShort;
}

int PluginConf_GetScriptPeriod(Plugin_t *plugin) {

  return plugin->config.periodLen;
}


char *PluginConf_GetJSMain(Plugin_t *plugin) {

  return configGetString(plugin, PLUGIN_CONF_TAG_JS_CLASS);
}

/*
 * Look up the value of a property as it would be displayed in
 * the config file.
 */
char **PluginConf_GetConfigValue(Plugin_t *plugin, char *property, int *count) {

  size_t origKeyLen = strlen(property) + strlen(PLUGIN_CONF_ORIGTAG) + 1;
  char origKey[origKeyLen];
  snprintf(origKey, origKeyLen, "%s%s", PLUGIN_CONF_ORIGTAG, property);

  return configGetAllStrings(plugin, origKey, count);
}
