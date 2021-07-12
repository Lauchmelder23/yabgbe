#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef unsigned char		BYTE;
typedef unsigned short		WORD;
typedef unsigned int		DWORD;
typedef unsigned long long	QWORD;

#define EXIT_MSG(msg, ...) { \
	fprintf(stderr, (msg), __VA_ARGS__); \
	fprintf(stderr, "\n"); \
	fprintf(stderr, "%s\n", strerror(errno)); \
	fprintf(stderr, "Exiting.\n"); \
}

inline BYTE undefined;