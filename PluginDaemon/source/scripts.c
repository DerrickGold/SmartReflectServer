//
// Created by Derrick on 2016-01-22.
//

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <signal.h>

#include "scripts.h"
#include "misc.h"

/*
 * Execute a script and return its output
 */
char *Script_ExecGetSTDIO(char *scriptPath) {
  //execute the script file here, and do communications to front end
  FILE *proc = popen(scriptPath, "r");
  if (!proc) {
    SYSLOG(LOG_ERR, "Script_ExecGetSTDIO: Error opening process file...");
    return NULL;
  }

  int streamSize = 256, bytesRead = 0;

  //initialize buffer
  char *stdoutStream = calloc(streamSize, sizeof(char));
  if (!stdoutStream) {
    SYSLOG(LOG_ERR, "Script_ExecGetSTDIO: Error initializing initial stdout buffer...");
    pclose(proc);
    return NULL;
  }

  char lineBuf[256];
  while (fgets(lineBuf, sizeof(lineBuf), proc) != NULL) {
    bytesRead += strlen(lineBuf) + 1;
    if (bytesRead >= streamSize) {
      streamSize <<= 1; //exponentially grow buffer, does less allocations over greater time

      char *tempbuf = realloc(stdoutStream, streamSize * sizeof(char));
      if (!tempbuf) {
        SYSLOG(LOG_ERR, "Script_ExecGetSTDIO: Error reallocing stdout buffer...");
        //free old stdout buffer
        free(stdoutStream);
        //close file handle
        pclose(proc);
        return NULL;
      }

      //successfully realloc'd
      stdoutStream = tempbuf;
    }

    //this should be safe as stdoutStream will be resized greater than linebuf contents
    strcat(stdoutStream, lineBuf);
  }
  //close standard output of the file
  pclose(proc);

  return stdoutStream;
}

void Script_KillBG(pid_t pid) {

  SYSLOG(LOG_INFO, "Script_KillBG: Killing process: %d", pid);
  kill(-pid, SIGTERM);
}

/*
 * Execute a script and run it in the background
 */
pid_t Script_ExecInBg(char *scriptPath, char *comFilePath, int portNumber) {

  //in parent, exit
  //pid_t parentGroup = getpgrid(0);

  pid_t pid = fork();
  if (pid) {
    if (pid < 0)
      SYSLOG(LOG_ERR, "Script_ExecInBG: Failed to fork background script.");
    return pid;
  }
  setpgid(0, 0);
  char command[PATH_MAX];
  snprintf(command, PATH_MAX, "%s %s %d", scriptPath, comFilePath, portNumber);
  SYSLOG(LOG_INFO, "Script_ExecInBg: Executing: %s", command);
  system(command);
  //char *args[] = {scriptPath, comFilePath, (char*)NULL};
  //execv(scriptPath, args);
  _exit(0);

  return 0;
}
