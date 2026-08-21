#ifndef __UTILITIES_H__
#define __UTILITIES_H__

int findcwd(char** buf);

int read_file(char* dir, char** file);

int build_tree(char** root);

char* buildArgs(char* option, char* var);

#endif // __UTILITIES_H__
