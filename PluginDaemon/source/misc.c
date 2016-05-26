//
// Created by Derrick on 2016-02-28.
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <syslog.h>
#include "misc.h"


/*
  Applies an operation to each file or folder found in a directory specified
  by the first path argument.

  the forEach function pointer is supplied:
    -the current file path of the directory being scanned
    -the dirent struct of the current file or folder

*/
int DirectoryAction(char *path, int (*forEach)(char *, struct dirent *, void *d), void *data) {

  struct dirent *dirInfo = NULL;
  DIR *directory = opendir(path);

  if (!directory || !forEach) {
    SYSLOG(LOG_ERR, "DirectoryAction: error opening: %s", path);
    return -1;
  }

  char filepath[PATH_MAX];
  while ((dirInfo = readdir(directory)) != NULL) {
    //skip . and .. directories
    int len = strlen(dirInfo->d_name);
    if (!strncmp(dirInfo->d_name, ".", len) || !strncmp(dirInfo->d_name, "..", len))
      continue;

    //determin if a slash needs to be added to the path or not.
    char *trailingSlash = strrchr(path, '/');
    char *pathEnd = (char *) path + strlen(path);

    if (!trailingSlash || (size_t) pathEnd - (size_t) trailingSlash > 1)
      snprintf(filepath, PATH_MAX, "%s/%s", path, dirInfo->d_name);
    else
      snprintf(filepath, PATH_MAX, "%s%s", path, dirInfo->d_name);

    //return failure if one of the forEach functions failed.
    if (forEach(filepath, dirInfo, data)) return -1;
  }

  closedir(directory);

  return 0;
}

