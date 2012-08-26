#include "balancer.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_decochain_balancer (Abstract)
//
Forwarder_decochain_balancer::Forwarder_decochain_balancer( Forwarder* fwd ) : Forwarder_decochain( fwd )
{
}//end Forwarder_decochain_balancer::Forwarder_decochain_balancer

Forwarder_decochain_balancer::~Forwarder_decochain_balancer()
{
}//end Forwarder_decochain_balancer::~Forwarder_decochain_balancer


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_decochain_balancer_traffic
//
Forwarder_decochain_balancer_traffic::Forwarder_decochain_balancer_traffic( Traffic_monitor* traff_mon, bool responsible, Forwarder* fwd )
	: Forwarder_decochain_balancer( fwd ), traff_mon_( traff_mon ), responsible_( responsible )
{
}//end Forwarder_decochain_balancer_traffic::Forwarder_decochain_balancer_traffic

int Forwarder_decochain_balancer_traffic::Get_easiest_wan_if( const Packet_Hdr* pkt )
{
	return traff_mon_->Get_easiest_wan_if();
}//end Forwarder_decochain_balancer_traffic::Get_easiest_wan_if

bool Forwarder_decochain_balancer_traffic::send_pkt_out( Port_Info* pi, const Wan_Param* wan_param, const Lan_Param* lan_param, Packet_Hdr *pkt,
		ssize_t len, time_t cur_tm )
{
	if( Forwarder_decochain_balancer::send_pkt_out( pi, wan_param, lan_param, pkt, len, cur_tm ) )
		{
		traff_mon_->Add_traffic( pi->wan_if, static_cast<int>(len), cur_tm );
		return true;
		}//end if

	return false;
}//end Forwarder_decochain_balancer_traffic::send_pkt_out

bool Forwarder_decochain_balancer_traffic::send_pkt_in( Port_Info* pi, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len, time_t cur_tm )
{
	if( Forwarder_decochain_balancer::send_pkt_in( pi, lan_param, pkt, len, cur_tm ) )
		{
		traff_mon_->Add_traffic( pi->wan_if, static_cast<int>(len), cur_tm );
		return true;
		}//end if

	return false;
}//end Forwarder_decochain_balancer_traffic::send_pkt_in

Forwarder_decochain_balancer_traffic::~Forwarder_decochain_balancer_traffic()
{
	if( responsible_ )
		{
		delete traff_mon_;
		}//end if
}//end Forwarder_decochain_balancer_traffic::~Forwarder_decochain_balancer_traffic
