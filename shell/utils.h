#ifndef UTILS_H
#define UTILS_H

#include "defs.h"

int unmask(int status);

char* split_line(char* buf, char splitter);

int block_contains(char* buf, char c);

char* remove_begin_spaces(char* buf);

#endif // UTILS_H
