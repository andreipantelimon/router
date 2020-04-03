#ifndef UTILS_H
#define UTILS_H

#include <skel.h>

struct arp_entry {
	__u32 ip;
	uint8_t mac[6];
};

uint16_t checksum(void *vdata, size_t length);

int rtable_comparator(const void *a, const void *b);

void init_packet(packet *pkt);

struct route_table_entry *binary_search_fo(struct route_table_entry *rtable, int size, uint32_t dest_ip);

#endif