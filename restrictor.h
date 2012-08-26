#ifndef CTQY_RESTRICTOR_H
#define CTQY_RESTRICTOR_H

#include "forwarder.h"
#include "ip_wan_map.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_decochain_restrictor
// (Restrict sessions with same source IP using the same WAN interface)
//
// Chain of Responsibility: Get_easiest_wan_if()
// Decorator: all
// Rely on: Get_easiest_wan_if()
//
class Forwarder_decochain_restrictor : public Forwarder_decochain
{
public:
	Forwarder_decochain_restrictor( Ip_wan_map* ip_wan_map, bool responsible, Forwarder* fwd );

	// interfaces: port mapping
	virtual void Del_timeout_map( time_t cur_tm, time_t timeout, uint16_t statemask, time_t timeout2 );

	// internal function: load balance
	virtual int Get_easiest_wan_if( const Packet_Hdr* pkt );

	// internal functions: packet sending
	virtual bool send_pkt_out( Port_Info* pi, const Wan_Param* wan_param, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len,
					time_t cur_tm );
	virtual bool send_pkt_in( Port_Info* pi, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len, time_t cur_tm );

	virtual ~Forwarder_decochain_restrictor();

private:
	Forwarder_decochain_restrictor(void);

	Ip_wan_map* const ip_wan_map_;
	const bool responsible_;

};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_decochain_restrictor_exclist
// (Restrict sessions with same source IP using the same WAN interface, except services(port number) in the exclusion list)
//
// Chain of Responsibility: Get_easiest_wan_if()
// Decorator: all
// Rely on: dst_port() and Get_easiest_wan_if()
//
class Forwarder_decochain_restrictor_exclist : public Forwarder_decochain_restrictor
{
public:
	Forwarder_decochain_restrictor_exclist( const bool exclist[PORT_NUM]/*net order*/, Ip_wan_map* ip_wan_map, bool responsible, Forwarder* fwd );

	// internal function: load balance
	virtual int Get_easiest_wan_if( const Packet_Hdr* pkt );

	virtual ~Forwarder_decochain_restrictor_exclist();

private:
	Forwarder_decochain_restrictor_exclist(void);

	bool exclist_[PORT_NUM];// net order

};

#endif // CTQY_RESTRICTOR_H
