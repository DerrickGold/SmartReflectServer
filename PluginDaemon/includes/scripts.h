//
// Created by Derrick on 2016-01-22.
//

#ifndef MAGICMIRROR_SCRIPTS_H
#define MAGICMIRROR_SCRIPTS_H

extern char *Script_ExecGetSTDIO(char *scriptPath);

extern void Script_KillBG(pid_t pid);

extern pid_t Script_ExecInBg(char *scriptPath, char *comFilePath, int portNumber);


#endif //MAGICMIRROR_SCRIPTS_H
