#ifndef __DISPLAY_H__
#define __DISPLAY_H__

extern int Display_LoadPlugin(Plugin_t *plugin);

extern int Display_UnloadPlugin(Plugin_t *plugin);

extern int Display_GetDisplaySize(void);

extern int Display_Generate(int portNum, const char *comFolder, const char *cssFolder, const char *jsLibsFolder,
                            const char *output);

extern int Display_IsDisplayConnected(void);

extern int Display_SendFrontendMsg(char *msg, size_t size);

extern void Display_ClearDisplayResponse(void);

extern char *Display_GetDisplayResponse(void);

extern void Display_Cleanup(void);

#endif
