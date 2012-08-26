#include "forwarder.h"
#include <stdlib.h>

//#define GOOGLE_STRIP_LOG 0
//#include <glog/logging.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder (Abstract)
//
Forwarder::Forwarder(void)
{
}//end Forwarder::Forwarder

bool Forwarder::Forward_out( const Wan_Param wan_param[], const Lan_Param* lan_param, Packet_Hdr* pkt, ssize_t len, time_t cur_tm )
{
	Port_Info *pi = Lan_map( src_port( pkt ), &pkt->iph.ip6_src, cur_tm );
	if( pi == NULL && IN6_IS_ADDR_GLOBAL( &pkt->iph.ip6_dst ) )
		{
		int wan_num = get_wan_num();
		int wan_if = 0;

		if( wan_num > 1 )
			{
			for( ; wan_if < wan_num && !IN6_ARE_ADDR_EQUAL( &wan_param[wan_if].wan_ip, &pkt->iph.ip6_dst ); ++wan_if );//end for
			if( wan_if >= wan_num )
				{
				wan_if = Get_easiest_wan_if( pkt );
				}//end if
			}//end if
		pi = Add_map( pkt, src_port( pkt ), wan_if, cur_tm );
		}//end if
	if( pi != NULL )
		{
		//LOG(INFO) << "send pkt out";
		return send_pkt_out( pi, &wan_param[pi->wan_if], lan_param, pkt, len, cur_tm );
		}//end if

	return false;
}//end Forwarder::Forward_out

bool Forwarder::Forward_in( int wan_if, const Lan_Param* lan_param, Packet_Hdr* pkt, ssize_t len, time_t cur_tm )
{
	Port_Info *pi = Wan_map( wan_if, dst_port( pkt ), cur_tm );
	if( pi != NULL )
		{
		return send_pkt_in( pi, lan_param, pkt, len, cur_tm );
		}//end if

	return false;
}//end Forwarder::Forward_in

Forwarder::~Forwarder()
{
}//end Forwarder::~Forwarder


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_concrete
//
Forwarder_concrete::Forwarder_concrete(void)
{
}//end Forwarder_concrete::Forwarder_concrete

Forwarder_concrete::Forwarder_concrete( int wan_num ) : pm_( wan_num )
{
}//end Forwarder_concrete::Forwarder_concrete

Port_Info* Forwarder_concrete::Add_static_map( uint16_t port/*net order*/, const struct in6_addr* ip, const uint8_t mac[ETH_ALEN], int wan_if,
			uint16_t wan_port/*net order*/ )
{
	return pm_.Add_static_map( port, ip, mac, wan_if, wan_port );
}//end Forwarder_concrete::Add_static_map

void Forwarder_concrete::Del_timeout_map( time_t cur_tm, time_t timeout, uint16_t statemask, time_t timeout2 )
{
	pm_.Del_timeout_map( cur_tm, timeout, statemask, timeout2 );
}//end Forwarder_concrete::Del_timeout_map

int Forwarder_concrete::Get_easiest_wan_if( const Packet_Hdr* pkt )
{
	// default: random load balance
	//LOG(INFO) << "concrete::Get_easiest_wan_if: default: random load balance.";
	return rand() % pm_.get_wan_num();
}//end Forwarder_concrete::Get_easiest_wan_if

Port_Info* Forwarder_concrete::Wan_map( int wan_if, uint16_t wan_port/*net order*/, time_t cur_tm )
{
	return pm_.Wan_map( wan_if, wan_port, cur_tm );
}//end Forwarder_concrete::Wan_map

Port_Info* Forwarder_concrete::Lan_map( uint16_t port/*net order*/, const struct in6_addr* ip, time_t cur_tm )
{
	return pm_.Lan_map( port, ip, cur_tm );
}//end Forwarder_concrete::Lan_map

Port_Info* Forwarder_concrete::Add_map( const Packet_Hdr* pkt, uint16_t port/*net order*/, int wan_if, time_t cur_tm )
{
	return pm_.Add_map( port, &pkt->iph.ip6_src, pkt->eh.ether_shost, wan_if, cur_tm );
}//end Forwarder_concrete::Add_map

void Forwarder_concrete::Del_map( Port_Info* pi, time_t cur_tm )
{
	pm_.Del_map( pi, cur_tm );
}//end Forwarder_concrete::Del_map

int Forwarder_concrete::get_wan_num(void) const
{
	return pm_.get_wan_num();
}//end Forwarder_concrete::get_wan_num

bool Forwarder_concrete::send_pkt_out( Port_Info* pi, const Wan_Param* wan_param, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len,
		time_t cur_tm )
{
	memcpy( pkt->eh.ether_dhost, wan_param->wan_gw_mac, ETH_ALEN );
	memcpy( pkt->eh.ether_shost, wan_param->wan_mac, ETH_ALEN );
	--pkt->iph.ip6_hlim;
#ifdef FAKE_SEND
	return true;
#else
	return send( wan_param->sd_wan, pkt, len, 0 ) > 0;
#endif // FAKE_SEND
}//end Forwarder_concrete::send_pkt_out

bool Forwarder_concrete::send_pkt_in( Port_Info* pi, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len, time_t cur_tm )
{
	memcpy( pkt->eh.ether_dhost, pi->mac, ETH_ALEN );
	memcpy( pkt->eh.ether_shost, lan_param->lan_mac, ETH_ALEN );
	--pkt->iph.ip6_hlim;
#ifdef FAKE_SEND
	return true;
#else
	return send( lan_param->sd_lan, pkt, len, 0 ) > 0;
#endif // FAKE_SEND
}//end Forwarder_concrete::send_pkt_in

uint16_t Forwarder_concrete::src_port( const Packet_Hdr *pkt )
{
	//LOG(ERROR) << "concrete::src_port: shouldn't reach here!";
	return 0;
}//end Forwarder_concrete::src_port

uint16_t Forwarder_concrete::dst_port( const Packet_Hdr *pkt )
{
	//LOG(ERROR) << "concrete::src_port: shouldn't reach here!";
	return 0;
}//end Forwarder_concrete::dst_port

Forwarder_concrete::~Forwarder_concrete()
{
}//end Forwarder_concrete::~Forwarder_concrete


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_decochain (Decorator and Chain of Responsibility)
//
Forwarder_decochain::Forwarder_decochain( Forwarder* fwd ) : fwd_( fwd )
{
}//end Forwarder_decochain::Forwarder_decochain

Port_Info* Forwarder_decochain::Add_static_map( uint16_t port/*net order*/, const struct in6_addr* ip, const uint8_t mac[ETH_ALEN], int wan_if,
			uint16_t wan_port/*net order*/ )
{
	return fwd_->Add_static_map( port, ip, mac, wan_if, wan_port );
}//end Forwarder_decochain::Add_static_map

void Forwarder_decochain::Del_timeout_map( time_t cur_tm, time_t timeout, uint16_t statemask, time_t timeout2 )
{
	fwd_->Del_timeout_map( cur_tm, timeout, statemask, timeout2 );
}//end Forwarder_decochain::Del_timeout_map

int Forwarder_decochain::Get_easiest_wan_if( const Packet_Hdr* pkt )
{
	return fwd_->Get_easiest_wan_if( pkt );
}//end Forwarder_decochain::Get_easiest_wan_if

Port_Info* Forwarder_decochain::Wan_map( int wan_if, uint16_t wan_port/*net order*/, time_t cur_tm )
{
	return fwd_->Wan_map( wan_if, wan_port, cur_tm );
}//end Forwarder_decochain::Wan_map

Port_Info* Forwarder_decochain::Lan_map( uint16_t port/*net order*/, const struct in6_addr* ip, time_t cur_tm )
{
	return fwd_->Lan_map( port, ip, cur_tm );
}//end Forwarder_decochain::Lan_map

Port_Info* Forwarder_decochain::Add_map( const Packet_Hdr* pkt, uint16_t port/*net order*/, int wan_if, time_t cur_tm )
{
	return fwd_->Add_map( pkt, port, wan_if, cur_tm );
}//end Forwarder_decochain::Add_map

void Forwarder_decochain::Del_map( Port_Info* pi, time_t cur_tm )
{
	fwd_->Del_map( pi, cur_tm );
}//end Forwarder_decochain::Del_map

int Forwarder_decochain::get_wan_num(void) const
{
	return fwd_->get_wan_num();
}//end Forwarder_decochain::get_wan_num

bool Forwarder_decochain::send_pkt_out( Port_Info* pi, const Wan_Param* wan_param, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len,
		time_t cur_tm )
{
	return fwd_->send_pkt_out( pi, wan_param, lan_param, pkt, len, cur_tm );
}//end Forwarder_decochain::send_pkt_out

bool Forwarder_decochain::send_pkt_in( Port_Info* pi, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len, time_t cur_tm )
{
	return fwd_->send_pkt_in( pi, lan_param, pkt, len, cur_tm );
}//end Forwarder_decochain::send_pkt_in

uint16_t Forwarder_decochain::src_port( const Packet_Hdr *pkt )
{
	return fwd_->src_port( pkt );
}//end Forwarder_decochain::src_port

uint16_t Forwarder_decochain::dst_port( const Packet_Hdr *pkt )
{
	return fwd_->dst_port( pkt );
}//end Forwarder_decochain::dst_port

Forwarder_decochain::~Forwarder_decochain()
{
	//LOG(INFO) << "decorator::delete fwd_";
	delete fwd_;
}//end Forwarder_decochain::~Forwarder_decochain
