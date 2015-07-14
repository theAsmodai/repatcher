#ifndef NETCHAN_H
#define NETCHAN_H

#include "const.h"
#include "netadr.h"
#include "sizebuf.h"

// 0 == regular, 1 == file stream
#define MAX_STREAMS			2    

// flow control bytes per second limits
#define MAX_RATE			20000				
#define MIN_RATE			1000

// default data rate
#define DEFAULT_RATE		(9999.0f)

#define MAX_FLOWS			2

#define FLOW_OUTGOING		0
#define FLOW_INCOMING		1
#define MAX_LATENT			32

// size of fragmentation buffer internal buffers
#define FRAGMENT_SIZE 		1400

#define FRAG_NORMAL_STREAM	0
#define FRAG_FILE_STREAM	1

#define NET_MAX_PAYLOAD		3992

typedef enum
{
	NS_CLIENT,
	NS_SERVER
} netsrc_t;

// message data
typedef struct flowstats_s
{
	int			size;		// size of message sent/received
	double		time;		// time that message was sent/received
} flowstats_t;

typedef struct flow_s
{
	flowstats_t	stats[MAX_LATENT];	// data for last MAX_LATENT messages
	int			current;		// current message position
	double		nextcompute; 	// time when we should recompute k/sec data
	float		kbytespersec;	// average data
	float		avgkbytespersec;
} flow_t;

// generic fragment structure
typedef struct fragbuf_s
{
	struct fragbuf_s*	next;		// next buffer in chain
	int					bufferid;		// id of this buffer
	sizebuf_t			frag_message;	// message buffer where raw data is stored
	byte				frag_message_buf[FRAGMENT_SIZE];	// the actual data sits here
	qboolean			isfile;		// is this a file buffer?
	qboolean			isbuffer;		// is this file buffer from memory ( custom decal, etc. ).
	char				filename[64];	// name of the file to save out on remote host
	int					foffset;		// offset in file from which to read data  
	int					size;		// size of data to read at that offset
} fragbuf_t;

// Waiting list of fragbuf chains
typedef struct fragbufwaiting_s
{
	struct fragbufwaiting_s*	next;	// next chain in waiting list
	int							fragbufcount;	// number of buffers in this chain
	fragbuf_t*					fragbufs;	// the actual buffers
} fragbufwaiting_t;

typedef struct netchan_s
{
	netsrc_t			socket;
	netadr_t			remote_address;

	unsigned int		client_index;
	float				last_received; // for timeouts
	float				connected_time;

	double				rate; // bandwidth choke. bytes per second
	double				cleartime; // if realtime > cleartime, free to send next packet

	// Sequencing variables
	unsigned int		incoming_sequence;
	unsigned int		incoming_acknowledged;
	unsigned int		incoming_reliable_acknowledged;
	unsigned int		incoming_reliable_sequence;

	unsigned int		outgoing_sequence;
	unsigned int		outgoing_reliable_sequence;
	unsigned int		outgoing_last_reliable_sequence;

	void*				owner;
	int					( *pfnGetFragmentSize )( struct client_s *);

	// staging and holding areas
	sizebuf_t			netchan_message;
	unsigned char		netchan_message_buf[NET_MAX_PAYLOAD];

	// reliable message buffer.
	// we keep adding to it until reliable is acknowledged.  Then we clear it
	int					reliable_length;
	unsigned char		reliable_buf[NET_MAX_PAYLOAD];
	
	// Waiting list of buffered fragments to go onto queue.
	// Multiple outgoing buffers can be queued in succession
	fragbufwaiting_t*	waitlist[MAX_STREAMS];
	
	int					reliable_fragment[MAX_STREAMS];	// is reliable waiting buf a fragment?          
	int					reliable_fragid[MAX_STREAMS];		// buffer id for each waiting fragment

	fragbuf_t*			fragbufs[MAX_STREAMS];	// the current fragment being set
	int					fragbufcount[MAX_STREAMS];	// the total number of fragments in this stream

	short				frag_startpos[MAX_STREAMS];	// position in outgoing buffer where frag data starts
	short				frag_length[MAX_STREAMS];

	fragbuf_t*			incomingbufs[MAX_STREAMS];	// incoming fragments are stored here
	qboolean			incomingready[MAX_STREAMS];	// set to true when incoming data is ready

	// Only referenced by the FRAG_FILE_STREAM component
	char				filename[260];
	char*				temp_buffer;
	int					temp_buffer_size;

	// incoming and outgoing flow metrics
	flow_t				flow[MAX_FLOWS];
} netchan_t;

#endif // NETCHAN_H