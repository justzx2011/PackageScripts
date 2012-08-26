#include "restrictor.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_decochain_restrictor
//
Forwarder_decochain_restrictor::Forwarder_decochain_restrictor( Ip_wan_map* ip_wan_map, bool responsible, Forwarder* fwd )
	: Forwarder_decochain( fwd ), ip_wan_map_( ip_wan_map ), responsible_( responsible )
{
}//end Forwarder_decochain_restrictor::Forwarder_decochain_restrictor

void Forwarder_decochain_restrictor::Del_timeout_map( time_t cur_tm, time_t timeout, uint16_t statemask, time_t timeout2 )
{
	if( responsible_ )
		{
		ip_wan_map_->RemoveTimeoutMap( cur_tm );
		}//end if
	Forwarder_decochain::Del_timeout_map( cur_tm, timeout, statemask, timeout2 );
}//end Forwarder_decochain_restrictor::Del_timeout_map

int Forwarder_decochain_restrictor::Get_easiest_wan_if( const Packet_Hdr* pkt )
{
	return ip_wan_map_->GetWanIf( pkt->iph.ip6_src, Forwarder_decochain::Get_easiest_wan_if( pkt ) );
}//end Forwarder_decochain_restrictor::Get_easiest_wan_if

bool Forwarder_decochain_restrictor::send_pkt_out( Port_Info* pi, const Wan_Param* wan_param, const Lan_Param* lan_param, Packet_Hdr *pkt,
		ssize_t len, time_t cur_tm )
{
	if( Forwarder_decochain::send_pkt_out( pi, wan_param, lan_param, pkt, len, cur_tm ) )
		{
		ip_wan_map_->UpdateTime( pi->ip, cur_tm );
		return true;
		}//end if

	return false;
}//end Forwarder_decochain_restrictor::send_pkt_out

bool Forwarder_decochain_restrictor::send_pkt_in( Port_Info* pi, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len, time_t cur_tm )
{
	if( Forwarder_decochain::send_pkt_in( pi, lan_param, pkt, len, cur_tm ) )
		{
		ip_wan_map_->UpdateTime( pi->ip, cur_tm );
		return true;
		}//end if

	return false;
}//end Forwarder_decochain_restrictor::send_pkt_in

Forwarder_decochain_restrictor::~Forwarder_decochain_restrictor()
{
	if( responsible_ )
		{
		delete ip_wan_map_;
		}//end if
}//end Forwarder_decochain_restrictor::~Forwarder_decochain_restrictor


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_decochain_restrictor_exclist
//
Forwarder_decochain_restrictor_exclist::Forwarder_decochain_restrictor_exclist( const bool exclist[PORT_NUM]/*net order*/, Ip_wan_map* ip_wan_map,
	bool responsible, Forwarder* fwd ) : Forwarder_decochain_restrictor( ip_wan_map, responsible, fwd )
{
	memcpy( exclist_, exclist, sizeof(exclist_) );
}//end Forwarder_decochain_restrictor_exclist::Forwarder_decochain_restrictor_exclist

int Forwarder_decochain_restrictor_exclist::Get_easiest_wan_if( const Packet_Hdr* pkt )
{
	if( !exclist_[Forwarder_decochain_restrictor::dst_port( pkt )] )
		{
		return Forwarder_decochain_restrictor::Get_easiest_wan_if( pkt );
		}//end if

	return Forwarder_decochain::Get_easiest_wan_if( pkt );
}//end Forwarder_decochain_restrictor_exclist::Get_easiest_wan_if

Forwarder_decochain_restrictor_exclist::~Forwarder_decochain_restrictor_exclist()
{
}//end Forwarder_decochain_restrictor_exclist::~Forwarder_decochain_restrictor_exclist
