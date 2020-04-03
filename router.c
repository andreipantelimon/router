//Copyright (C) 2020 Andrei Pantelimon 325 CA

#include "skel.h"
#include "route_table.h"
#include "utils.h"
#include "arp.h"
#include "queue.h"
#include "net/if_arp.h"
#include <stdio.h>
#include <stdlib.h>

#define IP_OFF sizeof(struct ether_header)
#define ICMP_OFF (sizeof (struct ether_header) + sizeof(struct iphdr))

//Functia pentru gasirea entry-ului din tabela de ARP
struct arp_entry *get_arp_entry(__u32 ip, struct arp_entry *arp_table, int arp_table_size) {
	if (arp_table == NULL) {
		return NULL;
	}
    for (int i = 0; i < arp_table_size; i++) {
    	if (arp_table[i].ip == ip) {
    		return &arp_table[i];
    	}
    }
    return NULL;
}

//Functia ce creeaza un pachet de tip ICMP Reply si il trimite
void send_ICMP_reply(int type, struct iphdr *ip_hdr, int interface, struct icmphdr *icmp_hdr, struct ether_header *eth_hdr) {
	packet reply;

	init_packet(&reply);

	struct ether_header *reth_hdr = (struct ether_header *)reply.payload;
	struct iphdr *rip_hdr = (struct iphdr *)(reply.payload + IP_OFF);
	struct icmphdr *ricmp_hdr = (struct icmphdr *)(reply.payload + ICMP_OFF);

	reth_hdr->ether_type = htons(ETHERTYPE_IP);
	memcpy(reth_hdr->ether_dhost, eth_hdr->ether_shost, sizeof(u_char));
	memcpy(reth_hdr->ether_shost, eth_hdr->ether_dhost, sizeof(u_char));

	reply.len = sizeof(struct ether_header) + sizeof(struct iphdr)
		+ sizeof(struct icmphdr);

	rip_hdr->version = 4;
	rip_hdr->ihl = 5;
	rip_hdr->tot_len = htons(sizeof(struct iphdr) + sizeof(struct icmphdr));
	rip_hdr->id = ip_hdr->id;
	rip_hdr->ttl = 63;
	rip_hdr->protocol = IPPROTO_ICMP;
	rip_hdr->daddr = ip_hdr->saddr;
	rip_hdr->saddr = ip_hdr->daddr;
	rip_hdr->check = 0;
	rip_hdr->check = checksum(rip_hdr, sizeof(struct iphdr));

	ricmp_hdr->type = type;
	ricmp_hdr->code = 0;
	ricmp_hdr->un.echo.id = icmp_hdr->un.echo.id;
	ricmp_hdr->un.echo.sequence = icmp_hdr->un.echo.sequence;
	ricmp_hdr->checksum = 0;
	ricmp_hdr->checksum = checksum(ricmp_hdr, sizeof(struct icmphdr));

	send_packet(interface, &reply);
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, 0);

	packet m;
	int rc;

	init();

	struct route_table_entry *rtable;
	int rtable_size;

	struct arp_entry *dynamic_arp_table;
	int dynamic_arp_table_size = 0;

	FILE *route_table_file;
	route_table_file = fopen("rtable.txt", "r");

	if (route_table_file == NULL) {
		perror("Error: Can't open file rtable.txt\n");
		fclose(route_table_file);
		return -1;
	}

	rtable_size = count_lines(route_table_file);
	rtable = malloc(rtable_size * sizeof(struct route_table_entry));
	route_table_read(rtable, rtable_size, route_table_file);

	dynamic_arp_table = malloc(10 * sizeof(struct arp_entry));

	//Sortez tabela de rutare
	qsort(rtable, rtable_size, sizeof(struct route_table_entry), rtable_comparator);

	//coada pentru pachete
	queue q;
	q = queue_create();

	while (1) {
		rc = get_packet(&m);
		DIE(rc < 0, "get_message");
		/* Students will write code here */

		struct ether_header *eth_hdr = (struct ether_header *)m.payload;

		//Verificam daca e pachet de tip IP

		if (ntohs(eth_hdr->ether_type) == ETHERTYPE_IP) {
			struct iphdr *ip_hdr = (struct iphdr *)(m.payload + IP_OFF);
			struct icmphdr *icmp_hdr = (struct icmphdr *)(m.payload + ICMP_OFF);

			//Verificam TTL si trimitem mesaj ICMP pentru Time Exceeded daca e nevoie
			if (ip_hdr->ttl <= 1) {
				send_ICMP_reply(ICMP_TIME_EXCEEDED, ip_hdr, m.interface, icmp_hdr, eth_hdr);
				continue;
			}

			//Verificam checksum
			if (checksum(ip_hdr, sizeof(struct iphdr))) {
				continue;
			}

			//Ruta din tabela de rutare
			struct route_table_entry *matching_route = binary_search_fo(rtable, rtable_size, ip_hdr->daddr);

			//Verificam daca ruta este existenta, daca nu trimitem DEST_UNREACH
			if (matching_route == NULL) {
				send_ICMP_reply(ICMP_PORT_UNREACH, ip_hdr, m.interface, icmp_hdr, eth_hdr);
				continue;
			}

			//Daca e pachet de tip ICMP_ECHO trimitem reply
			if (icmp_hdr->type == ICMP_ECHO && check_for_router(ip_hdr->daddr)) {
				uint32_t aux = ip_hdr->daddr;
				ip_hdr->daddr = ip_hdr->saddr;
				ip_hdr->saddr = aux;

				ip_hdr->tot_len = htons(sizeof(struct iphdr) + sizeof(struct icmphdr));
				ip_hdr->protocol = IPPROTO_ICMP;

				ip_hdr->check = 0;
				ip_hdr->check = checksum(ip_hdr, sizeof(struct iphdr));

				ip_hdr->ttl--;

				//Algoritmul incremental din RFC 1624, detaliat in README
				ip_hdr->check++;

				icmp_hdr->type = ICMP_ECHOREPLY;
				icmp_hdr->code = 0;
				icmp_hdr->checksum = 0;
				icmp_hdr->checksum = checksum(icmp_hdr, sizeof(struct icmphdr));

				send_packet(m.interface, &m);

				continue;
			}

			ip_hdr->check = 0;
			ip_hdr->check = checksum(ip_hdr, sizeof(struct iphdr));

			ip_hdr->ttl--;

			//Algoritmul incremental din RFC 1624, detaliat in README
			ip_hdr->check++;

			//Ruta pentru ARP
			struct arp_entry *matching_arp = get_arp_entry(ip_hdr->daddr, dynamic_arp_table, dynamic_arp_table_size);

			uint8_t *mac = malloc(6 * sizeof(uint8_t));
			get_interface_mac(matching_route->interface, mac);
			memcpy(eth_hdr->ether_shost, mac, 6);

			m.interface = matching_route->interface;

			//Daca nu exista intrare in tabela ARP, trimitem request
			if (matching_arp == NULL) {

				//Punem pachetul pe coada
				packet *new_packet = malloc(sizeof(packet));
				memcpy(new_packet, &m, sizeof(packet));
				queue_enq(q, new_packet);

				uint8_t *dst_mac = malloc(6 * sizeof(uint8_t));
				memset (dst_mac, 0xff, 6 * sizeof(uint8_t));

				uint8_t *src_mac = malloc(6 * sizeof(uint8_t));
				get_interface_mac(matching_route->interface, src_mac);

				uint32_t ip_s = get_interface_ipaddr(matching_route->interface);
				uint32_t ip_t = matching_route->next_hop;

				//Creez un nou pachet si il trimit
				packet request;

				init_packet(&request);

				request.len = sizeof(struct ether_header) + sizeof(arp_header);
				request.interface = matching_route->interface;

				struct ether_header *reth_hdr = (struct ether_header *)request.payload;
				arp_header *rarp_hdr = (arp_header *)(request.payload + sizeof(struct ether_header));

				memset(reth_hdr->ether_dhost, 0xff, 6 * sizeof(uint8_t));
				get_interface_mac(matching_route->interface, reth_hdr->ether_shost);
				reth_hdr->ether_type = htons(ETHERTYPE_ARP);

				rarp_hdr->htype = htons (ARPHRD_ETHER);
				rarp_hdr->ptype = htons (ETH_P_IP);
				rarp_hdr->hlen = 6;
				rarp_hdr->plen = 4;
				rarp_hdr->opcode = htons (ARPOP_REQUEST);
				memcpy(&rarp_hdr->sender_ip, &ip_s, 4 * sizeof(uint8_t));
				memcpy(&rarp_hdr->target_ip, &ip_t, 4 * sizeof(uint8_t));

				memcpy (rarp_hdr->sender_mac, src_mac, 6 * sizeof(uint8_t));
				memset (rarp_hdr->target_mac, 0, 6 * sizeof(uint8_t));

				send_packet(matching_route->interface, &request);

				continue;
			}

			//Daca exista ruta, trimitem pachetul
			memcpy(eth_hdr->ether_dhost, matching_arp->mac, 6);

			send_packet(matching_route->interface, &m);

			continue;
		}

		//Verificam daca e pachet de tip ARP
		if (ntohs(eth_hdr->ether_type) == ETHERTYPE_ARP) {
			arp_header *arp_hdr = (arp_header *)(m.payload + sizeof(struct ether_header));

			uint32_t ipv4;
			memcpy(&ipv4, arp_hdr->target_ip, 4);

			//Pachet ARP Request destinat noua, trimitem Reply
			if (ntohs(arp_hdr->opcode) == 1 && check_for_router(ipv4)) {

				uint8_t *src_mac = malloc(6 * sizeof(uint8_t));
				get_interface_mac(m.interface, src_mac);

				//Creem pachetul si il trimitem
				packet arp_reply;

				init_packet(&arp_reply);

				struct ether_header *reth_hdr = (struct ether_header *)arp_reply.payload;
				arp_header *rarp_hdr = (arp_header *)(arp_reply.payload + sizeof(struct ether_header));

				reth_hdr->ether_type = htons(ETHERTYPE_ARP);
				memcpy(reth_hdr->ether_dhost, eth_hdr->ether_shost, 6);
				get_interface_mac(m.interface, reth_hdr->ether_shost);

				rarp_hdr->htype = htons (ARPHRD_ETHER);
				rarp_hdr->ptype = htons (ETH_P_IP);
				rarp_hdr->hlen = 6;
				rarp_hdr->plen = 4;
				rarp_hdr->opcode = htons (ARPOP_REPLY);

				memcpy(rarp_hdr->sender_ip, arp_hdr->target_ip, 4);
				memcpy(rarp_hdr->target_ip, arp_hdr->sender_ip, 4);
				
				memcpy (rarp_hdr->target_mac, reth_hdr->ether_dhost, 6);
				memcpy (rarp_hdr->sender_mac, reth_hdr->ether_shost, 6);

				arp_reply.len = sizeof(struct ether_header) + sizeof(arp_header);
				arp_reply.interface = m.interface;

				send_packet(m.interface, &arp_reply);
				continue;

			}

			//Pachet de tip ARP, updatam tabela si trimitem pachetele
			if (ntohs(arp_hdr->opcode) == 2) {
				dynamic_arp_table_size++;
				if (dynamic_arp_table_size % 10 == 0) {
					if (!realloc(dynamic_arp_table, (dynamic_arp_table_size + 10) * sizeof(struct arp_entry))) {
						perror("Error: Can't reallocate memory.\n");
						return -1;
					}
				}
				memcpy(&dynamic_arp_table[dynamic_arp_table_size - 1].ip, arp_hdr->sender_ip, 4);
				memcpy(dynamic_arp_table[dynamic_arp_table_size - 1].mac, arp_hdr->sender_mac, 6);

				//Trimitem pachetele din coada
				while(!queue_empty(q)) {
					packet *s;
					s = (packet*) queue_deq(q);

					struct ether_header *seth_hdr = (struct ether_header *)s->payload;

					memcpy(seth_hdr->ether_dhost, arp_hdr->target_mac, 6);

					send_packet(s->interface, s);

					continue;
				}
			}
		}
	}

	free(rtable);
	free(dynamic_arp_table);
	fclose(route_table_file);
}
