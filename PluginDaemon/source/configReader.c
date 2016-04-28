//
// Created by David on 1/14/16.
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>

#include "configReader.h"
#include "misc.h"


#define PLUGIN_CONF_PROPERTY_LEN 256
#define PLUGIN_CONF_COMMENT '#'
#define PLUGIN_CONF_ASSIGN '='

/*
Read plugin config file

Config file follows the layout of:
  propertyName=valueToAssign

with lines having '#' as the first character being ignored as comments.

 Inputs: Filepath, function pointer for applyConfig, pointer to data for the config
 pass in void *data into apply if apply exists
 */

int ConfigReader_readConfig(char *filePath, int (*apply)(void *, char *, char *), void *data) {

  if (!data) {
    SYSLOG(LOG_ERR, "Config Reader: No data intiailized...");
    return -1;
  }
  if (!filePath) {
    SYSLOG(LOG_ERR, "Config Reader: No filepath initalized...");
    return -1;
  }

  //open config file for reading
  FILE *confStream = fopen(filePath, "r");
  if (!confStream) {
    SYSLOG(LOG_ERR, "Config Reader: Config file is missing %s", filePath);
    return -1;
  }

  size_t lineCount = 0;
  char lineBuf[PLUGIN_CONF_PROPERTY_LEN + PATH_MAX];
  char property[PLUGIN_CONF_PROPERTY_LEN], value[PATH_MAX];

  while (!feof(confStream)) {
    lineCount++;

    //read next line in the config file
    memset(lineBuf, 0, sizeof(lineBuf));
    char *line = fgets(lineBuf, sizeof(lineBuf), confStream);
    if (!line) break;


    //strip trailing white space
    while (*line != '\0') line++;
    line--;
    while (*line == ' ' || *line == '\r' || *line == '\n') *line-- = '\0';

    //line is empty, skip it
    if (strlen(lineBuf) < 1) continue;

    //reset line pointer to beginning
    line = lineBuf;

    //strip leading white space
    while (*line == ' ' || *line == '\t') line++;
    strncpy(lineBuf, line, sizeof(lineBuf));
    line = lineBuf;

    //skip line if commented out
    if (*line == PLUGIN_CONF_COMMENT) continue;

    //otherwise grab the setting
    char *dest = property;
    while (*line != PLUGIN_CONF_ASSIGN && *line != '\0') {
      if (*line == ' ' || *line == '\t') line++;
      else *dest++ = *line++;
    }
    *dest = '\0';

    //make sure we aren't at the end of the line
    if (*line == '\0') continue;
    //skip equal sign
    line++;

    //grab value now
    char quote = 0;
    dest = value;
    while (*line != '\n' && *line != '\0') {
      if (*line == '\"') {
        if (!quote) quote = 1;
        else quote = 0;
        line++;
      }
      if ((*line == ' ' || *line == '\t') && !quote) {
        line++;
      }
      else *dest++ = *line++;
    }
    *dest = '\0';

    //set plugin property
    if (apply) apply(data, property, value);
  }

  //close config file
  fclose(confStream);
  return 0;
}

//insert a new value for a setting into an existing config file
//or append a new setting that does not exist to the file
int ConfigReader_writeConfig(char *outputFile, char *origFile, char *setting, char *newVal) {

  if (!origFile || !outputFile) {
    SYSLOG(LOG_ERR, "Config Reader: No filepath initalized...");
    return -1;
  }

  //open config file for reading
  FILE *confStream = fopen(origFile, "r");
  if (!confStream) {
    SYSLOG(LOG_ERR, "Config Reader: Config file is missing %s", origFile);
    return -1;
  }

  FILE *saveStream = fopen(outputFile, "w");
  if (!saveStream) {
    SYSLOG(LOG_ERR, "ConfigReader: Output config file missing %s", outputFile);
    fclose(confStream);
    return -1;
  }

  size_t lineCount = 0;
  char lineBuf[PLUGIN_CONF_PROPERTY_LEN + PATH_MAX];
  char property[PLUGIN_CONF_PROPERTY_LEN], value[PATH_MAX];

  int foundProperty = 0;

  //read input stream
  //write to output stream
  while (!feof(confStream)) {
    lineCount++;

    //read next line in the config file
    memset(lineBuf, 0, sizeof(lineBuf));
    char *line = fgets(lineBuf, sizeof(lineBuf), confStream);
    if (!line) break;


    //strip trailing white space
    while (*line != '\0') line++;
    line--;
    while (*line == ' ' || *line == '\r' || *line == '\n')
      *line-- = '\0';

    //line is empty, skip it
    if (strlen(lineBuf) < 1) {
      //preserve whitespace in the output file
      fprintf(saveStream, "\n");
      continue;
    }

    //reset line pointer to beginning
    line = lineBuf;

    //strip leading white space
    while (*line == ' ' || *line == '\t') line++;
    strncpy(lineBuf, line, sizeof(lineBuf) - 1);
    line = lineBuf;

    //skip line if commented out
    if (*line == PLUGIN_CONF_COMMENT) {
      //preserve comments in the output stream
      fprintf(saveStream, "%s\n", line);
      continue;
    }

    //otherwise grab the setting
    char *dest = property;
    while (*line != PLUGIN_CONF_ASSIGN && *line != '\0') {
      if (*line == ' ' || *line == '\t') line++;
      else *dest++ = *line++;
    }
    *dest = '\0';

    //write out the <property> = part


    //check if we have found the value we wish to change
    if (!foundProperty && !strncmp(property, setting, strlen(setting))) {
      foundProperty = 1;

      //remove blank settings in the config file
      if (strlen(newVal) < 1)
        continue;

      fprintf(saveStream, "%s=", property);
      fprintf(saveStream, "%s\n", newVal);
      continue;
    }

    //otherwise, write out the old value, since we are leaving it unchanged
    //make sure we aren't at the end of the line
    if (*line == '\0') continue;

    fprintf(saveStream, "%s=", property);

    //skip equal sign
    line++;

    //grab value now
    char quote = 0;
    dest = value;
    while (*line != '\n' && *line != '\0') {
      if (*line == '\"') {
        if (!quote) quote = 1;
        else quote = 0;
        line++;
      }
      if ((*line == ' ' || *line == '\t') && !quote) {
        line++;
      }
      else *dest++ = *line++;
    }
    *dest = '\0';

    fprintf(saveStream, "%s\n", value);

  }

  //if we haven't found the property yet in the config file, its likely a new setting
  //so we can append it to the end of the file
  if (!foundProperty && strlen(newVal) > 0)
    fprintf(saveStream, "%s=%s\n", setting, newVal);

  //close config file
  fclose(confStream);
  fclose(saveStream);


  //rename second file to original file
  if (!unlink(origFile))
    rename(outputFile, origFile);


  return 0;
}


char *ConfigReader_escapePath(char *str) {

  if (!str) {
    SYSLOG(LOG_ERR, "ConfigReader_escapePath: null string passed in");
    return NULL;
  }

  SYSLOG(LOG_INFO, "ConfigReader_escapePath: path to escape: %s", str);
  int size = strlen(str);
  int spaces = 0;

  //count number of spaces
  char *start = str;
  while (start != NULL) {
    start = strchr(start, ' ');
    if (start != NULL) {
      spaces++;
      start++;
    }
  }
  size += spaces;
  SYSLOG(LOG_INFO, "ConfigReader_escapePath: number of spaces to escape: %d", spaces);
  char *newStr = calloc(size + 1, sizeof(char));
  if (!newStr) {
    SYSLOG(LOG_ERR, "ConfigReader_escapePath: error allocating space for escape path");
    return NULL;
  }

  char *dest = newStr;
  while (*str != '\0' && *str != '\n') {
    if (*str == ' ') {
      *dest++ = '\\';
    }
    *dest++ = *str++;
  }
  *dest = '\0';

  SYSLOG(LOG_INFO, "ConfigReader_escapePath: new escaped string: %s", newStr);
  return newStr;
}
