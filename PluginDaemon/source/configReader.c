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


static char *skipLeadingWhiteSpace(char *inputLine) {

  while (*inputLine == ' ' || *inputLine == '\t')
    inputLine++;

  return inputLine;
}

static char *trimTrailingWhiteSpace(char *inputLine) {

  char *end = inputLine + strlen(inputLine);

  while (*end == '\0' || *end == '\r' || *end == '\n' || *end == '\t')
    *end-- = '\0';

  return inputLine;
}

static char *getProperty(char *inputLine, char *output) {

  while (*inputLine != PLUGIN_CONF_ASSIGN && *inputLine != '\0') {
    //skip any spaces or tabs within the property name
    if (*inputLine == ' ' || *inputLine == '\t')
      inputLine++;
      //and copy the rest
    else
      *output++ = *inputLine++;
  }
  *output = '\0';
  return inputLine;
}

static char *getValue(char *inputLine, char *output) {

  //skip any spaces immediately after the = sign
  while (*inputLine == ' ' || *inputLine == '\t')
    inputLine++;

  while (*inputLine != '\n' && *inputLine != '\0') {
    //ignore optional quotes
    if (*inputLine == '\"')
      inputLine++;

    *output++ = *inputLine++;
  }
  *output = '\0';

  return inputLine;
}

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
    if (!line)
      break;


    //strip trailing white space
    line = trimTrailingWhiteSpace(line);
    //line is empty, skip it
    if (strlen(line) < 1)
      continue;

    //strip leading white space
    line = skipLeadingWhiteSpace(line);
    memmove(lineBuf, line, strlen(line));
    line = lineBuf;

    //skip line if commented out
    if (*line == PLUGIN_CONF_COMMENT)
      continue;

    //otherwise grab the setting
    line = getProperty(line, property);

    //make sure we aren't at the end of the line
    if (*line == '\0')
      continue;

    //skip equal sign
    line++;

    //grab value now
    line = getValue(line, value);

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
    line = trimTrailingWhiteSpace(line);

    //line is empty, skip it
    if (strlen(lineBuf) < 1) {
      //preserve whitespace in the output file
      fprintf(saveStream, "\n");
      continue;
    }

    //strip leading white space
    line = skipLeadingWhiteSpace(line);
    memmove(lineBuf, line, strlen(line));
    line = lineBuf;

    //skip line if commented out
    if (*line == PLUGIN_CONF_COMMENT) {
      //preserve comments in the output stream
      fprintf(saveStream, "%s\n", line);
      continue;
    }

    //otherwise grab the setting
    line = getProperty(line, property);

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
    line = getValue(line, value);
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
