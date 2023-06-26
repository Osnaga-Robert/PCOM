#include "queue.h"
#include "lib.h"
#include "protocols.h"
#include <arpa/inet.h>
#include <string.h>

//valori globale pentru a face mai usor functiile
struct route_table_entry rtable[100000];
int rtable_size;
struct arp_entry arp_table[6];
int arp_table_size;

//functie pentru conversia din uint32_t la IPv4
char* get_ip(uint32_t number){
	struct in_addr sa;
	sa.s_addr = number;
	return inet_ntoa(sa);
}

//cea mai buna ruta(preluata din laborator)
struct route_table_entry *get_best_route(uint32_t ip_dest) {
	int sol = -1;
	for (int i = 0; i < rtable_size; i++) {
		if ((ip_dest & rtable[i].mask) == rtable[i].prefix) {
			if (sol == -1 || ntohl(rtable[i].mask) > ntohl(rtable[sol].mask)) {
				sol = i;
			}
		}
	}

	if (sol == -1) {
		printf("Nu s-a gasit adresa\n");
		return NULL;
	}
	return &rtable[sol];
}

struct arp_entry *get_mac_entry(uint32_t ip_dest) {
	for(int i = 0 ; i < arp_table_size ; i++){
		if(arp_table[i].ip == ip_dest){
			return &arp_table[i];
		}
			
	}
	return NULL;
}

void IP_get(int interface, size_t len, char* buf, queue *q){
	//cele trei structuri din care poate fi alcauit un IP , ether_header, iphdr si icmphdr
	struct ether_header *eth_hdr = (struct ether_header *) buf;
	struct iphdr *ip_hdr = (struct iphdr *)(buf + sizeof(struct ether_header));
	struct icmphdr *icmp_hdr = (struct icmphdr *)(buf + sizeof(struct ether_header) + sizeof(struct iphdr));

	uint32_t ip_dest = (ip_hdr->daddr);

	char* aux = get_interface_ip(interface);
	char* ip_dest_check = malloc(strlen(aux));

	strcpy(ip_dest_check, aux);

	aux = get_ip(ip_dest);

	//verificam daca mesajul este pentru router
	if(!strcmp(ip_dest_check,aux)){
		if (icmp_hdr->type == 8){ //este un echo request
			icmp_hdr->type = 0;
			icmp_hdr->code = 0;
			icmp_hdr->checksum  = 0;
			icmp_hdr->checksum =htons (checksum((uint16_t *)icmp_hdr, sizeof(struct icmphdr)));
		}
		//trimitem mesajul mai departe
		send_to_link(interface,buf,len);
		return;
	}
	uint16_t init_check = ip_hdr->check;

	ip_hdr->check = 0;
	ip_hdr->check = checksum( (uint16_t *)ip_hdr, sizeof(struct iphdr));

	//cazul in care checksum-ul este diferit de cel calculat, in care nu vom returna nimic
	if(ntohs(init_check) != ip_hdr->check){
		ip_hdr->check =  init_check;
		return;
	}

	//daca ttl este mai mic sau egal cu unu vom modifica icmp-ul pentru a trimite un time limit exceeded
	if(ip_hdr->ttl <= 1){
		uint8_t aux1[6];
		uint32_t aux2;

		//schimbam adresele destinatie si sursa, iar mac-urile le interschimbam
		for(int i = 0 ; i < 6 ; i++){
			aux1[i] = eth_hdr->ether_dhost[i];
			eth_hdr->ether_dhost[i] = eth_hdr->ether_shost[i];
			eth_hdr->ether_shost[i] = aux1[i];
		}

		aux2 = ip_hdr->saddr;

		ip_hdr->saddr = inet_addr(get_interface_ip(interface));
		ip_hdr->daddr = aux2;

		ip_hdr->protocol = 1;
		ip_hdr->ttl = 64;
		ip_hdr->tot_len = htons(sizeof(struct icmphdr) + sizeof(struct iphdr));

		ip_hdr->check = 0;
		ip_hdr->check = ntohs((checksum((uint16_t *)ip_hdr, sizeof(struct iphdr))));	
			
		icmp_hdr->type = 11;
		icmp_hdr->code = 0;
		icmp_hdr->checksum = 0;
		icmp_hdr->checksum = ntohs(checksum((uint16_t *)icmp_hdr, sizeof(struct icmphdr)));

		//trimitem buff-erul mai departe
		send_to_link(interface,buf,sizeof(struct icmphdr) + sizeof(struct iphdr) + sizeof(struct ether_header));
		return;
	}
	uint8_t old_ttl = ip_hdr->ttl;

	ip_hdr->ttl = ip_hdr->ttl - 1;

	//aflam cea mai buna ruta;

	struct route_table_entry *next = get_best_route(ip_dest);

	//calculam checksum-ul conform formulei

	uint16_t aux1 = ~(~(init_check) + ~((uint16_t)old_ttl) + (uint16_t)(ip_hdr->ttl)) - 1;

	//in cazul in care nu gasim in tabela de routare in element vom trimitem un icmp de tipul destination unreachable 

	if(next == NULL){
		uint8_t aux1[6];
		uint32_t aux2;

		//procedam tot la fel, interschimbam mac-urile si modificam adresele

		for(int i = 0 ; i < 6 ; i++){
			aux1[i] = eth_hdr->ether_dhost[i];
			eth_hdr->ether_dhost[i] = eth_hdr->ether_shost[i];
			eth_hdr->ether_shost[i] = aux1[i];
		}

		aux2 = ip_hdr->saddr;
		ip_hdr->saddr = inet_addr(get_interface_ip(interface));
		ip_hdr->daddr = aux2;

		ip_hdr->protocol = 1;
		ip_hdr->ttl = 64;
		ip_hdr->tot_len = htons(sizeof(struct icmphdr) + sizeof(struct iphdr));

		ip_hdr->check = 0;
		ip_hdr->check = ntohs((checksum((uint16_t *)ip_hdr, sizeof(struct iphdr))));	
			
		icmp_hdr->type = 3;
		icmp_hdr->code = 0;
		icmp_hdr->checksum = 0;
		icmp_hdr->checksum = ntohs(checksum((uint16_t *)icmp_hdr, sizeof(struct icmphdr)));

		//trimitem buff-erul modificat mai departe
		send_to_link(interface,buf,sizeof(struct icmphdr) + sizeof(struct iphdr) + sizeof(struct ether_header));
		return;
	}

	ip_hdr->check = aux1;

	//aflam interfata mac
	get_interface_mac(next->interface, eth_hdr->ether_shost);

	struct arp_entry * mac_entry = get_mac_entry(next->next_hop);

	//daca nu exista in arp_table atunci va trebui sa facem un arp request pentru a descoperi mac-ul
	if(mac_entry == NULL){
		char *auxbuf = (char *)malloc(len);
		memcpy(auxbuf,buf,len);
		queue_enq(*q,auxbuf); // adaugam in coada

		//construim arp_buf-erul asa cum este prezentat in descrierea temei

		char arp_buf[sizeof(struct ether_header) + sizeof(struct arp_header)];
		struct ether_header *arp_eth_hdr = (struct ether_header *) arp_buf;
		struct arp_header *arp_hdr = (struct arp_header *)(arp_buf + sizeof(struct ether_header));

		for(int i = 0 ; i < 6 ; i++){
			arp_eth_hdr->ether_dhost[i] = 0xff;
			arp_eth_hdr->ether_shost[i] = 0;
		}
		get_interface_mac(next->interface, arp_eth_hdr->ether_shost);
		arp_eth_hdr->ether_type = htons(0x0806);

		arp_hdr->htype = htons(1);
		arp_hdr->ptype = htons(0x800);
		arp_hdr->hlen = 6;
		arp_hdr->plen = 4;
		arp_hdr->op = htons(1);

		get_interface_mac(next->interface, arp_hdr->sha);
		arp_hdr->spa = inet_addr(get_interface_ip(next->interface));

		for(int i = 0 ; i < 6 ; i++){
			arp_hdr->tha[i] = 0xff;
		}
		arp_hdr->tpa = next->next_hop;

		send_to_link(next->interface,arp_buf,sizeof(struct ether_header) + sizeof(struct arp_header));

		return;
	}

	//in caz ca existe in arp_table atunci luam adresa mac corespunzatoare

	for(int i = 0 ; i < 6 ; i++){
		eth_hdr->ether_dhost[i] = mac_entry->mac[i];
	}

	//trimitem buff-erul modificat mai departe
	send_to_link(next->interface,buf,len);

	free(ip_dest_check);
}

void ARP_get(int interface, size_t len, char* buf, queue *q){
	struct ether_header *arp_eth_hdr = (struct ether_header *) buf;
	struct arp_header *arp_hdr = (struct arp_header *)(buf + sizeof(struct ether_header));

	//pentru 1 stim ca arp va fi de tipul request
	if(ntohs(arp_hdr->op) == 1){

		arp_hdr->op = htons (2);

		uint32_t aux;

		aux = arp_hdr->spa;
		arp_hdr->spa = arp_hdr->tpa;
		arp_hdr->tpa = aux;

		for(int i = 0 ; i < 6 ; i++)
			arp_hdr->tha[i] = arp_hdr->sha[i];
		get_interface_mac(interface, arp_hdr->sha);

		for(int i = 0 ; i < 6 ; i++)
			arp_eth_hdr->ether_dhost[i] = arp_eth_hdr->ether_shost[i];
		get_interface_mac(interface,arp_eth_hdr->ether_shost);

		send_to_link(interface, buf, len);
		return;
	}
	//pentru 2 stim ca arp va fi de tipul replay
	else{
		for(int i = 0 ; i < 6 ; i++)
		 	arp_table[arp_table_size].mac[i] = arp_hdr->sha[i];
		arp_table[arp_table_size].ip = arp_hdr->spa;
		arp_table_size++;

		//scoatem tot din coada pentru a trimite mesajele
		while(!queue_empty(*q)){
			char *ip_buf = malloc(MAX_PACKET_LEN);
			ip_buf = (char*)queue_deq(*q); 

			struct ether_header *ip_eth_hdr = (struct ether_header *) ip_buf;
			struct iphdr *ip_hdr = (struct iphdr *)(ip_buf + sizeof(struct ether_header));

			struct route_table_entry *next = get_best_route(ip_hdr->daddr);

			struct arp_entry * mac_entry = get_mac_entry(next->next_hop);

			if(mac_entry == NULL)
				break;

			for(int i = 0 ; i < 6 ; i++)
				ip_eth_hdr->ether_dhost[i] = mac_entry->mac[i];
			send_to_link(next->interface,ip_buf,sizeof(struct ether_header) + sizeof(struct iphdr));
		}
		return;
	}
}


int main(int argc, char *argv[])
{
	char buf[MAX_PACKET_LEN];

	// Do not modify this line
	init(argc - 2, argv + 2);

	arp_table_size = 0;

	//citim tabela de routare
	rtable_size = read_rtable(argv[1],rtable);

	//initializam coada
	queue q;
	q = queue_create();


	while (1) {

		int interface;
		size_t len;

		interface = recv_from_any_link(buf, &len);
		DIE(interface < 0, "recv_from_any_links");

		struct ether_header *eth_hdr = (struct ether_header *) buf;
		uint16_t type = ntohs(eth_hdr->ether_type);

		if(type == 0x0800){
			//cazul in care avem de a face cu un IP
			IP_get(interface, len, buf, &q);
		}
		else if (type == 0x0806){
			//cazul in care avem un ARP 
			ARP_get(interface, len, buf, &q);
		}
		

		/* Note that packets received are in network order,
		any header field which has more than 1 byte will need to be conerted to
		host order. For example, ntohs(eth_hdr->ether_type). The oposite is needed when
		sending a packet on the link, */


	}
}

