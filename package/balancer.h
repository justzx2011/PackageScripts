#ifndef CTQY_BALANCER_H
#define CTQY_BALANCER_H

#include "forwarder.h"
#include "load_monitor.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_decochain_balancer (Abstract)
// (Load balancer)
//
// Chain of Responsibility: Get_easiest_wan_if()
// Decorator: others
//
class Forwarder_decochain_balancer : public Forwarder_decochain
{
public:
	Forwarder_decochain_balancer( Forwarder* fwd );

	// internal function: load balance
	// force subclasses to implement this
	virtual int Get_easiest_wan_if( const Packet_Hdr* pkt ) = 0;

	virtual ~Forwarder_decochain_balancer();

private:
	Forwarder_decochain_balancer(void);

};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_decochain_balancer_traffic
// (Upload/download traffic balancer)
//
// Chain of Responsibility: Get_easiest_wan_if()
// Decorator: others
//
class Forwarder_decochain_balancer_traffic : public Forwarder_decochain_balancer
{
public:
	Forwarder_decochain_balancer_traffic( Traffic_monitor* traff_mon, bool responsible, Forwarder* fwd );

	// internal function: load balance
	virtual int Get_easiest_wan_if( const Packet_Hdr* pkt );

	// internal functions: packet sending
	virtual bool send_pkt_out( Port_Info* pi, const Wan_Param* wan_param, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len,
					time_t cur_tm );
	virtual bool send_pkt_in( Port_Info* pi, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len, time_t cur_tm );

	virtual ~Forwarder_decochain_balancer_traffic();

private:
	Forwarder_decochain_balancer_traffic(void);

	Traffic_monitor* const traff_mon_;
	const bool responsible_;
};


#endif // CTQY_BALANCER_H
