#ifndef HUNK_H
#define HUNK_H

typedef struct hunk_s
{
	int sentinal;
	int size;
	char name[64];
} hunk_t;

typedef struct memblock_s
{
	int size;
	int tag;
	int id;
	struct memblock_s* next;
	struct memblock_s* prev;
	int pad;
} memblock_t;

typedef struct memzone_s
{
	int size;
	memblock_t blocklist;
	memblock_t* rover;
} memzone_t;

#if !defined( CACHE_USER )
#define CACHE_USER
typedef struct cache_user_s
{
	void* data;
} cache_user_t;
#endif

typedef struct cache_system_s
{
	int size;
	cache_user_t* user;
	char name[64];
	struct cache_system_s* prev;
	struct cache_system_s* next;
	struct cache_system_s* lru_prev;
	struct cache_system_s* lru_next;
} cache_system_t;

#endif // HUNK_H