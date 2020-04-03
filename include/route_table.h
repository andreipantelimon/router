#ifndef ROUTE_TABLE_H
#define ROUTE_TABLE_H

#include <stdio.h>
#include <string.h>
#include "utils.h"

struct route_table_entry {
	uint32_t prefix;
	uint32_t next_hop;
	uint32_t mask;
	int interface;
} __attribute__((packed));

int count_lines(FILE *file);

void route_table_read(struct route_table_entry *route_table, int num_lines, FILE *route_table_file);

#endif
