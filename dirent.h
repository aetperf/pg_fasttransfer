/*
 * Fake dirent.h for Windows PostgreSQL extensions
 * This header provides Windows-compatible definitions for directory
 * operations that PostgreSQL expects from dirent.h
 */

#ifndef _DIRENT_H_
#define _DIRENT_H_

#ifdef _WIN32

#include <windows.h>
#include <io.h>
#include <direct.h>

/* Directory entry structure */
struct dirent {
    long d_ino;              /* inode number */
    off_t d_off;             /* offset to next dirent */
    unsigned short d_reclen; /* length of this record */
    unsigned char d_type;    /* file type */
    char d_name[256];        /* filename (null-terminated) */
};

/* Directory stream type */
typedef struct {
    HANDLE hFind;
    WIN32_FIND_DATA findData;
    struct dirent entry;
    int first;
} DIR;

/* File type constants for d_type */
#define DT_UNKNOWN  0
#define DT_FIFO     1
#define DT_CHR      2
#define DT_DIR      4
#define DT_BLK      6
#define DT_REG      8
#define DT_LNK      10
#define DT_SOCK     12
#define DT_WHT      14

/* Function declarations */
#ifdef __cplusplus
extern "C" {
#endif

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);
void rewinddir(DIR *dirp);
long telldir(DIR *dirp);
void seekdir(DIR *dirp, long pos);

#ifdef __cplusplus
}
#endif

/* 
 * Note: Actual implementations would typically be in a .c file,
 * but for this minimal extension we only need the declarations
 * since PostgreSQL may not actually use these functions in our code path.
 * If needed, implementations can be added or linked from a Windows dirent library.
 */

#endif /* _WIN32 */

#endif /* _DIRENT_H_ */