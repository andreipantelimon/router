#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <netinet/ip_icmp.h>
#include <netinet/in.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <ifaddrs.h>

#include "utils.h"
#include "skel.h"
#include "route_table.h"

uint16_t checksum(void *vdata, size_t length) {
	// Cast the data pointer to one that can be indexed.
	char* data=(char*)vdata;

	// Initialise the accumulator.
	uint64_t acc=0xffff;

	// Handle any partial block at the start of the data.
	unsigned int offset=((uintptr_t)data)&3;
	if (offset) {
		size_t count=4-offset;
		if (count>length) count=length;
		uint32_t word=0;
		memcpy(offset+(char*)&word,data,count);
		acc+=ntohl(word);
		data+=count;
		length-=count;
	}

	// Handle any complete 32-bit blocks.
	char* data_end=data+(length&~3);
	while (data!=data_end) {
		uint32_t word;
		memcpy(&word,data,4);
		acc+=ntohl(word);
		data+=4;
	}
	length&=3;

	// Handle any partial block at the end of the data.
	if (length) {
		uint32_t word=0;
		memcpy(&word,data,length);
		acc+=ntohl(word);
	}

	// Handle deferred carries.
	acc=(acc&0xffffffff)+(acc>>32);
	while (acc>>16) {
		acc=(acc&0xffff)+(acc>>16);
	}

	// If the data began at an odd byte address
	// then reverse the byte order to compensate.
	if (offset&1) {
		acc=((acc&0xff00)>>8)|((acc&0x00ff)<<8);
	}

	// Return the checksum in network byte order.
	return htons(~acc);
}

int rtable_comparator(const void *a, const void *b) {
	uint32_t a_prefix = ((struct route_table_entry *) a) -> prefix;
	uint32_t b_prefix = ((struct route_table_entry *) b) -> prefix;

	uint32_t a_mask = ((struct route_table_entry *) a) -> mask;
	uint32_t b_mask = ((struct route_table_entry *) b) -> mask;

	if (a_prefix != b_prefix) {
		return (int) a_prefix - b_prefix;
	}

	if (b_mask > a_mask) {
		return 1;
	} else {
		return -1;
	}
}

void init_packet(packet *pkt) {
	memset(pkt->payload, 0, sizeof(pkt->payload));
	pkt->len = 0;
}

struct route_table_entry *binary_search_fo(struct route_table_entry *rtable, int size, uint32_t dest_ip)
{
	int low = 0, high = size - 1;
	int result = -1;

	while (low <= high) {
		int mid = (low + high) / 2;

		if ((dest_ip & rtable[mid].mask) == rtable[mid].prefix) {
			result = mid;
			high = mid - 1;
		} else {
			if ((dest_ip & rtable[mid].mask) < rtable[mid].prefix) {
				high = mid - 1;
			} else {
				low = mid + 1;
			}	
		}
	}

	if (result == -1) {
		return NULL;
	} else {
		return &rtable[result];
	}
}
