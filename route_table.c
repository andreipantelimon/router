#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h> /* the L2 protocols */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <unistd.h>
/* According to POSIX.1-2001, POSIX.1-2008 */
#include <sys/select.h>
/* ethheader */
#include <net/ethernet.h>
/* ether_header */
#include <arpa/inet.h>
/* icmphdr */
#include <netinet/ip_icmp.h>
/* arphdr */
#include <net/if_arp.h>
#include <asm/byteorder.h>

#include "route_table.h"
#include "utils.h"
#include "skel.h"

int count_lines(FILE *file) {
	char c;
	int count = 0;
	for (c = getc(file); c != EOF; c = getc(file)) {
		if (c == '\n') {
			count++;
		}
	}
	return count;
}

void route_table_read(struct route_table_entry *route_table, int num_lines, FILE *route_table_file) {
	rewind(route_table_file);

	char line_string[100];

	for (int i = 0; i < num_lines; i++) {
		if (fgets(line_string, 100, route_table_file) != NULL) {
			char *token = strtok(line_string, " ");
			route_table[i].prefix = inet_addr(token);

			token = strtok(NULL, " ");
			route_table[i].next_hop = inet_addr(token);

			token = strtok(NULL, " ");
			route_table[i].mask = inet_addr(token);

			token = strtok(NULL, " ");
			route_table[i].interface = atoi(token);

			line_string[0] = '\0';
		}
	}
}
