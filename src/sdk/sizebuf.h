#ifndef SIZEBUF_H
#define SIZEBUF_H

typedef enum overflow_s
{
	FSB_NOOVERFLOW,
	FSB_ALLOWOVERFLOW,
	FSB_OVERFLOWED
} overflow_t;

typedef struct sizebuf_s
{
	char*			debugname;
	int				overflow;
	unsigned char*	data;

	int				maxsize;
	int				cursize;
} sizebuf_t;

#endif // SIZEBUF_H