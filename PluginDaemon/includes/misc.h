//
// Created by Derrick on 2016-02-28.
//

#ifndef MAGICMIRROR_MISC_H
#define MAGICMIRROR_MISC_H


#include <dirent.h>

#define SYSLOG(logtype, fmt, ...) syslog((logtype), fmt, ##__VA_ARGS__)
//#define SYSLOG(logtype, fmt, ...) do {} while (0)

extern int DirectoryAction(char *path, int (*forEach)(char *, struct dirent *, void *), void *data);

#endif //MAGICMIRROR_MISC_H
