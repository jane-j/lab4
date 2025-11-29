#ifndef __FILES_H__
#define __FILES_H__

#include "dfs.h"
#include "files_shared.h"

//#define FILE_MAX_OPEN_FILES 15

void FileModuleInit();
int FileNameExists(char * filename);
int FileOpen(char *filename, char *mode);
int FileClose(int handle);
int FileRead(int handle, void *mem, int num_bytes);
int FileWrite(int handle, void *mem, int num_bytes);
int FileSeek(int handle, int num_bytes, int from_where);
int FileDelete(char *filename);
int FileRename(char *oldname, char *newname);

#endif
