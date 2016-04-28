//
// Created by David on 1/14/16.
//

#ifndef MAGIC_MIRROR_CONFIGREADER_H
#define MAGIC_MIRROR_CONFIGREADER_H

extern int ConfigReader_readConfig(char *filePath, int (*apply)(void *, char *, char *), void *data);

extern int ConfigReader_writeConfig(char *outputFile, char *origFile, char *setting, char *newVal);

extern char *ConfigReader_escapePath(char *str);

#endif //MAGIC_MIRROR_CONFIGREADER_H
