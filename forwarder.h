#ifndef CTQY_FORWARDER_H
#define CTQY_FORWARDER_H

#include "port_map.h"
#include <netinet/ether.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/icmp6.h>

#define MAX_PKT_SIZE 4096

#define IN6_IS_ADDR_GLOBAL( ip ) ((((const in6_addr*)(ip))->s6_addr[0] & 0xe0) == 0x20) // aggregatable global unicast - RFC2374


#pragma pack(push)
#pragma pack(1)
typedef struct
{
	struct ether_header eh;
	struct ip6_hdr iph;
	union
	{
		struct tcphdr tcph;
		struct udphdr udph;
		struct icmp6_hdr icmph;
		struct
		{
			struct nd_router_advert ra;
			struct nd_opt_hdr opth;
			uint8_t mac[ETH_ALEN];
			struct nd_opt_mtu mtu;
			struct nd_opt_prefix_info pi;
		} ra;
		struct nd_neighbor_solicit ns;
		struct nd_neighbor_advert na;
	};
} Packet_Hdr;
#pragma pack(pop)


typedef struct
{
	int sd_lan;
	uint8_t lan_mac[ETH_ALEN];
} Lan_Param;

typedef struct
{
	int sd_wan;
	struct in6_addr wan_ip;
	uint8_t wan_mac[ETH_ALEN];
	uint8_t wan_gw_mac[ETH_ALEN];
} Wan_Param;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder (Abstract)
//
class Forwarder
{
public:
	Forwarder(void);

	// template method
	bool Forward_out( const Wan_Param wan_param[], const Lan_Param* lan_param, Packet_Hdr* pkt, ssize_t len, time_t cur_tm );
	bool Forward_in( int wan_if, const Lan_Param* lan_param, Packet_Hdr* pkt, ssize_t len, time_t cur_tm );

	// interfaces: port mapping
	virtual Port_Info* Add_static_map( uint16_t port/*net order*/, const struct in6_addr* ip, const uint8_t mac[ETH_ALEN], int wan_if,
						uint16_t wan_port/*net order*/ ) = 0;
	// Del_timeout_map must be called periodically!
	virtual void Del_timeout_map( time_t cur_tm, time_t timeout = 0, uint16_t statemask = 0, time_t timeout2 = 0 ) = 0;

	// internal functions: port mapping and load balance
	virtual int Get_easiest_wan_if( const Packet_Hdr* pkt ) = 0;
	virtual Port_Info* Wan_map( int wan_if, uint16_t wan_port/*net order*/, time_t cur_tm ) = 0;
	virtual Port_Info* Lan_map( uint16_t port/*net order*/, const struct in6_addr* ip, time_t cur_tm ) = 0;
	virtual Port_Info* Add_map( const Packet_Hdr* pkt, uint16_t port/*net order*/, int wan_if, time_t cur_tm ) = 0;
	virtual void Del_map( Port_Info* pi, time_t cur_tm ) = 0;
	virtual int get_wan_num(void) const = 0;

	// internal functions: packet modifying and sending
	virtual bool send_pkt_out( Port_Info* pi, const Wan_Param* wan_param, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len,
					time_t cur_tm ) = 0;
	virtual bool send_pkt_in( Port_Info* pi, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len, time_t cur_tm ) = 0;
	virtual uint16_t src_port( const Packet_Hdr *pkt ) = 0;
	virtual uint16_t dst_port( const Packet_Hdr *pkt ) = 0;

	virtual ~Forwarder();

};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_concrete
//
class Forwarder_concrete : public Forwarder
{
public:
	Forwarder_concrete(void);
	Forwarder_concrete( int wan_num );

	// interfaces: port mapping
	virtual Port_Info* Add_static_map( uint16_t port/*net order*/, const struct in6_addr* ip, const uint8_t mac[ETH_ALEN], int wan_if,
						uint16_t wan_port/*net order*/ );
	virtual void Del_timeout_map( time_t cur_tm, time_t timeout = 0, uint16_t statemask = 0, time_t timeout2 = 0 );

	// internal functions: port mapping and load balance
	virtual int Get_easiest_wan_if( const Packet_Hdr* pkt );
	virtual Port_Info* Wan_map( int wan_if, uint16_t wan_port/*net order*/, time_t cur_tm );
	virtual Port_Info* Lan_map( uint16_t port/*net order*/, const struct in6_addr* ip, time_t cur_tm );
	virtual Port_Info* Add_map( const Packet_Hdr* pkt, uint16_t port/*net order*/, int wan_if, time_t cur_tm );
	virtual void Del_map( Port_Info* pi, time_t cur_tm );
	virtual int get_wan_num(void) const;

	// packet sending
	virtual bool send_pkt_out( Port_Info* pi, const Wan_Param* wan_param, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len,
					time_t cur_tm );
	virtual bool send_pkt_in( Port_Info* pi, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len, time_t cur_tm );

	// Former decorators did it. Hence these functions shouldn't be called.
	virtual uint16_t src_port( const Packet_Hdr *pkt );
	virtual uint16_t dst_port( const Packet_Hdr *pkt );

	virtual ~Forwarder_concrete();

private:
	Port_map pm_;

};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_decochain (Decorator and Chain of Responsibility)
//
// Chain of Responsibility interfaces: Get_easiest_wan_if(), src_port() and dst_port()
// Decorator interfaces: others
//
class Forwarder_decochain : public Forwarder
{
public:
	Forwarder_decochain( Forwarder* fwd );

	// interfaces: port mapping
	virtual Port_Info* Add_static_map( uint16_t port/*net order*/, const struct in6_addr* ip, const uint8_t mac[ETH_ALEN], int wan_if,
						uint16_t wan_port/*net order*/ );
	virtual void Del_timeout_map( time_t cur_tm, time_t timeout = 0, uint16_t statemask = 0, time_t timeout2 = 0 );

	// internal functions: port mapping and load balance
	virtual int Get_easiest_wan_if( const Packet_Hdr* pkt ); // Chain of Responsibility
	virtual Port_Info* Wan_map( int wan_if, uint16_t wan_port/*net order*/, time_t cur_tm );
	virtual Port_Info* Lan_map( uint16_t port/*net order*/, const struct in6_addr* ip, time_t cur_tm );
	virtual Port_Info* Add_map( const Packet_Hdr* pkt, uint16_t port/*net order*/, int wan_if, time_t cur_tm );
	virtual void Del_map( Port_Info* pi, time_t cur_tm );
	virtual int get_wan_num(void) const;

	// internal functions: packet sending
	virtual bool send_pkt_out( Port_Info* pi, const Wan_Param* wan_param, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len,
					time_t cur_tm );
	virtual bool send_pkt_in( Port_Info* pi, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len, time_t cur_tm );
	virtual uint16_t src_port( const Packet_Hdr *pkt ); // Chain of Responsibility
	virtual uint16_t dst_port( const Packet_Hdr *pkt ); // Chain of Responsibility

	virtual ~Forwarder_decochain();

private:
	Forwarder_decochain(void);

	Forwarder* const fwd_;
};

#endif // CTQY_FORWARDER_H
