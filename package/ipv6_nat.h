#ifndef CTQY_IPV6_NAT_H
#define CTQY_IPV6_NAT_H

#include "factory.h"

#define MAX_PKT_SIZE 4096
#define MAX_ERROR_MSG_SIZE 256


typedef struct
{
	char proto[8]; // "tcp" or "udp"
	int wan_if; // 0 ~ wan_num-1
	uint16_t wan_port; // net order
	uint8_t mac[ETH_ALEN];
	struct in6_addr ip; // global address
	uint16_t port; // net order
} Static_Port_Map;

class Ipv6_nat;
// thread delete it
typedef struct
{
	Ipv6_nat* nat;
	int wan_if;
} Wan_Proc_Param;

typedef void (*AbortProc)( void* arg );

class Ipv6_nat
{
public:
	Ipv6_nat(void);
	void set_abort( AbortProc abort, void* arg );
	bool Start( const char* lan_if, int wan_num, const char* wan_if[], const uint8_t wan_gw_mac[][ETH_ALEN], const Static_Port_Map pm[],
			int pm_num, char errormsg[] );
	void Stop(void);
	~Ipv6_nat();

private:
	int set_static_port_map( const Static_Port_Map pm[], int num );
	void fill_ra_pkt( Packet_Hdr *pkt );

	static void* lan_proc( Ipv6_nat *nat );
	static void* wan_proc( Wan_Proc_Param *param );
	static void* clean_proc( Ipv6_nat *nat );
	static void* ra_proc( Ipv6_nat *nat );

	volatile bool run_;

	int wan_num_;

	Lan_Param lan_param_;
	Wan_Param *wan_param_;// multiple

	pthread_t th_lan_; // in->out
	pthread_t *th_wan_; // out->in, multiple
	pthread_t th_clean_; // clean timeout sessions
	pthread_t th_ra_; // router advertisement

	Forwarder* forwarder_[PROTO_NUM];

	uint32_t mtu_;
	struct in6_addr lan_ip_; // link ip

	volatile time_t cur_tm_; // updated by clean_proc, read by lan_proc and wan_proc

	AbortProc abort_;
	void* abort_arg_;
};

#endif // CTQY_IPV6_NAT_H
