#ifndef __FILES_H__
#define __FILES_H__

#include "dfs.h"
#include "files_shared.h"

//#define FILE_MAX_OPEN_FILES 15

void FileModuleInit(); //: initialize the file module; must be called before any other file-level functions are used.
int FileOpen(char *filename, char *mode); //: open the given filename with one of three possible modes: "r", "w", "a". If opening the file in "w" mode, and the file already exists, the inode should first be deleted and then reopened. Return FILE_FAIL on failure, and the handle of a file descriptor on success. Remember to use locks whenever you allocate a new file descriptor. If opening the file in "a" mode, position the write pointer at EOF and create the file if it does not exist. You can use dstrncmp function (misc.c) to compare strings.
int FileClose(int handle); //: close the given file descriptor handle. Return FILE_FAIL on failure, and FILE_SUCCESS on success.
int FileRead(int handle, void *mem, int num_bytes); //: read num_bytes from the open file descriptor identified by handle. Return FILE_FAIL on failure or if the end-of-file flag is already set, and the number of bytes read on success. If end of file is reached, the end-of-file flag in the file descriptor should be set.
int FileWrite(int handle, void *mem, int num_bytes); //: write num_bytes to the open file descriptor identified by handle. If the file is opened with mode="r", then return failure. Return FILE_FAIL on failure, and the number of bytes written on success.
int FileSeek(int handle, int num_bytes, int from_where); //: seek num_bytes within the file descriptor identified by handle, from the location specified by from_where. There are three possible values for from_where: FILE_SEEK_CUR (seek relative to the current position), FILE_SEEK_SET (seek relative to the beginning of the file), and FILE_SEEK_END (seek relative to the end of the file). Any seek operation will clear the eof flag.
int FileDelete(char *filename);  //: delete the file specified by filename. Return FILE_FAIL on failure, and FILE_SUCCESS on success.
int FileRename(char *oldname, char *newname); //: rename a file; return fail if newname already exists.

#endif
