#include "protocol.h"

//#define GOOGLE_STRIP_LOG 0
//#include <glog/logging.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_decochain_protocol (Abstract)
//
Forwarder_decochain_protocol::Forwarder_decochain_protocol( Forwarder* fwd ) : Forwarder_decochain( fwd )
{
}//end Forwarder_decochain_protocol::Forwarder_decochain_protocol

bool Forwarder_decochain_protocol::send_pkt_out( Port_Info* pi, const Wan_Param* wan_param, const Lan_Param* lan_param, Packet_Hdr *pkt,
		ssize_t len, time_t cur_tm )
{
	//LOG(INFO) << "Forwarder_decochain_protocol::send_pkt_out";
	out_pkt_mod( pkt, &wan_param->wan_ip, pi->wan_port );
	if( !IN6_ARE_ADDR_EQUAL( &wan_param->wan_ip, &pkt->iph.ip6_dst ) )
		{
		//LOG(INFO) << "Forwarder_decochain_protocol::send_pkt_out: send out";
		return Forwarder_decochain::send_pkt_out( pi, wan_param, lan_param, pkt, len, cur_tm );
		}//end if

	// else: static port map, throw it back
	return Forward_in( pi->wan_if, lan_param, pkt, len, cur_tm );
}//end Forwarder_decochain_protocol::send_pkt_out

bool Forwarder_decochain_protocol::send_pkt_in( Port_Info* pi, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len, time_t cur_tm )
{
	//LOG(INFO) << "Forwarder_decochain_protocol::send_pkt_in";
	in_pkt_mod( pkt, &pi->ip, pi->port );
	return Forwarder_decochain::send_pkt_in( pi, lan_param, pkt, len, cur_tm );
}//end Forwarder_decochain_protocol::send_pkt_in

uint16_t Forwarder_decochain_protocol::inc_mod_checksum( uint16_t srccksum, const struct in6_addr *ip, const struct in6_addr *ipa, uint16_t port,
			uint16_t porta )
{
	if( srccksum != 0 )
		{
		uint32_t cksum = ~srccksum & 0xffff;

		cksum += ~ip->s6_addr16[0] & 0xffff;
		cksum += ~ip->s6_addr16[1] & 0xffff;
		cksum += ~ip->s6_addr16[2] & 0xffff;
		cksum += ~ip->s6_addr16[3] & 0xffff;
		cksum += ~ip->s6_addr16[4] & 0xffff;
		cksum += ~ip->s6_addr16[5] & 0xffff;
		cksum += ~ip->s6_addr16[6] & 0xffff;
		cksum += ~ip->s6_addr16[7] & 0xffff;
		cksum += ipa->s6_addr16[0];
		cksum += ipa->s6_addr16[1];
		cksum += ipa->s6_addr16[2];
		cksum += ipa->s6_addr16[3];
		cksum += ipa->s6_addr16[4];
		cksum += ipa->s6_addr16[5];
		cksum += ipa->s6_addr16[6];
		cksum += ipa->s6_addr16[7];

		cksum += ~port & 0xffff;
		cksum += porta;

		cksum = (cksum >> 16) + (cksum & 0xffff);
		cksum = (cksum >> 16) + (cksum & 0xffff);

		return (uint16_t)(~cksum);
		}//end if

	return 0;
}//end Forwarder_decochain_protocol::inc_mod_checksum

Forwarder_decochain_protocol::~Forwarder_decochain_protocol()
{
}//end Forwarder_decochain_protocol::~Forwarder_decochain_protocol


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_decochain_protocol_tcp
//
Forwarder_decochain_protocol_tcp::Forwarder_decochain_protocol_tcp( Forwarder* fwd ) : Forwarder_decochain_protocol( fwd )
{
}//end Forwarder_decochain_protocol_tcp::Forwarder_decochain_protocol_tcp

void Forwarder_decochain_protocol_tcp::Del_timeout_map( time_t cur_tm, time_t timeout, uint16_t statemask, time_t timeout2 )
{
	Forwarder_decochain_protocol::Del_timeout_map( cur_tm, kTcpTimeout_, kTcpHalfSyn_, kTcpSynTimeout_ );
}//end Forwarder_decochain_protocol_tcp::Del_timeout_map

Port_Info* Forwarder_decochain_protocol_tcp::Add_map( const Packet_Hdr* pkt, uint16_t port/*net order*/, int wan_if, time_t cur_tm )
{
	Port_Info *pi = NULL;

	//LOG(INFO) << "TCP sync check: " << pkt->tcph.syn;
	if( pkt->tcph.syn )
		{
		//LOG(INFO) << "tcp::Forwarder_decochain_protocol::Add_map";
		pi = Forwarder_decochain_protocol::Add_map( pkt, port, wan_if, cur_tm );
		if( pi != NULL )
			{
			//LOG(INFO) << "tcp::Add half syn flag";
			pi->state = kTcpHalfSyn_;
			}//end if
		}//end if

	return pi;
}//end Forwarder_decochain_protocol_tcp::Add_map

bool Forwarder_decochain_protocol_tcp::send_pkt_out( Port_Info* pi, const Wan_Param* wan_param, const Lan_Param* lan_param, Packet_Hdr *pkt,
		ssize_t len, time_t cur_tm )
{
	// must call the template method here
	if( Forwarder_decochain_protocol::send_pkt_out( pi, wan_param, lan_param, pkt, len, cur_tm ) )
		{
		if( pkt->tcph.fin )
			{
			pi->state |= kTcpSrcFin_;
			}
		else if( pkt->tcph.rst || ( pkt->tcph.ack && (pi->state & kTcpFin_) == kTcpFin_ ) )
			{
			// close this map
			Forwarder_decochain_protocol::Del_map( pi, cur_tm );
			}//end if
		//LOG(INFO) << "modify tcp state (srcfin, Del_map on rst or fin)";

		return true;
		}//end if
	//LOG(INFO) << "send failed, don't modify tcp state (srcfin, Del_map on rst or fin)";

	return false;
}//end Forwarder_decochain_protocol_tcp::send_pkt_out

bool Forwarder_decochain_protocol_tcp::send_pkt_in( Port_Info* pi, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len, time_t cur_tm )
{
	// must call the template method here
	if( Forwarder_decochain_protocol::send_pkt_in( pi, lan_param, pkt, len, cur_tm ) )
		{
		if( pkt->tcph.syn )
			{
			pi->state &= ~kTcpHalfSyn_; // connected, remove half syn flag
			}
		else if( pkt->tcph.fin )
			{
			pi->state |= kTcpDstFin_;
			}
		else if( pkt->tcph.rst || ( pkt->tcph.ack && (pi->state & kTcpFin_) == kTcpFin_ ) )
			{
			// close this map
			Forwarder_decochain_protocol::Del_map( pi, cur_tm );
			}//end if
		//LOG(INFO) << "modify tcp state (remove half syn, dstfin, Del_map on rst or fin)";
		return true;
		}//end if
	//LOG(INFO) << "send failed, don't modify tcp state (remove half syn, dstfin, Del_map on rst or fin)";

	return false;
}//end Forwarder_decochain_protocol_tcp::send_pkt_in

uint16_t Forwarder_decochain_protocol_tcp::src_port( const Packet_Hdr *pkt )
{
	return pkt->tcph.source;
}//end Forwarder_decochain_protocol_tcp::src_port

uint16_t Forwarder_decochain_protocol_tcp::dst_port( const Packet_Hdr *pkt )
{
	return pkt->tcph.dest;
}//end Forwarder_decochain_protocol_tcp::dst_port

void Forwarder_decochain_protocol_tcp::in_pkt_mod( Packet_Hdr *pkt, const struct in6_addr *dip, uint16_t dport )
{
	//LOG(INFO) << "tcp::in_pkt_mod";
	pkt->tcph.check = inc_mod_checksum( pkt->tcph.check, &pkt->iph.ip6_dst, dip, pkt->tcph.dest, dport );
	pkt->iph.ip6_dst = *dip;
	pkt->tcph.dest = dport;
}//end Forwarder_decochain_protocol_tcp::in_pkt_mod

void Forwarder_decochain_protocol_tcp::out_pkt_mod( Packet_Hdr *pkt, const struct in6_addr *sip, uint16_t sport )
{
	//LOG(INFO) << "tcp::out_pkt_mod";
	pkt->tcph.check = inc_mod_checksum( pkt->tcph.check, &pkt->iph.ip6_src, sip, pkt->tcph.source, sport );
	pkt->iph.ip6_src = *sip;
	pkt->tcph.source = sport;
}//end Forwarder_decochain_protocol_tcp::out_pkt_mod

Forwarder_decochain_protocol_tcp::~Forwarder_decochain_protocol_tcp()
{
}//end Forwarder_decochain_protocol_tcp::~Forwarder_decochain_protocol_tcp


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_decochain_protocol_udp
//
Forwarder_decochain_protocol_udp::Forwarder_decochain_protocol_udp( Forwarder* fwd ) : Forwarder_decochain_protocol( fwd )
{
}//end Forwarder_decochain_protocol_udp::Forwarder_decochain_protocol_udp

void Forwarder_decochain_protocol_udp::Del_timeout_map( time_t cur_tm, time_t timeout, uint16_t statemask, time_t timeout2 )
{
	Forwarder_decochain_protocol::Del_timeout_map( cur_tm, kUdpTimeout_, 0, 0 );
}//end Forwarder_decochain_protocol_udp::Del_timeout_map

uint16_t Forwarder_decochain_protocol_udp::src_port( const Packet_Hdr *pkt )
{
	return pkt->udph.source;
}//end Forwarder_decochain_protocol_udp::src_port

uint16_t Forwarder_decochain_protocol_udp::dst_port( const Packet_Hdr *pkt )
{
	return pkt->udph.dest;
}//end Forwarder_decochain_protocol_udp::dst_port

void Forwarder_decochain_protocol_udp::in_pkt_mod( Packet_Hdr *pkt, const struct in6_addr *dip, uint16_t dport )
{
	//LOG(INFO) << "udp::in_pkt_mod";
	pkt->udph.check = inc_mod_checksum( pkt->udph.check, &pkt->iph.ip6_dst, dip, pkt->udph.dest, dport );
	pkt->iph.ip6_dst = *dip;
	pkt->udph.dest = dport;
}//end Forwarder_decochain_protocol_udp::in_pkt_mod

void Forwarder_decochain_protocol_udp::out_pkt_mod( Packet_Hdr *pkt, const struct in6_addr *sip, uint16_t sport )
{
	//LOG(INFO) << "udp::out_pkt_mod";
	pkt->udph.check = inc_mod_checksum( pkt->udph.check, &pkt->iph.ip6_src, sip, pkt->udph.source, sport );
	pkt->iph.ip6_src = *sip;
	pkt->udph.source = sport;
}//end Forwarder_decochain_protocol_udp::out_pkt_mod

Forwarder_decochain_protocol_udp::~Forwarder_decochain_protocol_udp()
{
}//end Forwarder_decochain_protocol_udp::~Forwarder_decochain_protocol_udp


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_decochain_protocol_icmp
//
Forwarder_decochain_protocol_icmp::Forwarder_decochain_protocol_icmp( Forwarder* fwd ) : Forwarder_decochain_protocol( fwd )
{
}//end Forwarder_decochain_protocol_icmp::Forwarder_decochain_protocol_icmp

void Forwarder_decochain_protocol_icmp::Del_timeout_map( time_t cur_tm, time_t timeout, uint16_t statemask, time_t timeout2 )
{
	Forwarder_decochain_protocol::Del_timeout_map( cur_tm, kIcmpTimeout_, 0, 0 );
}//end Forwarder_decochain_protocol_icmp::Del_timeout_map

uint16_t Forwarder_decochain_protocol_icmp::src_port( const Packet_Hdr *pkt )
{
	return pkt->icmph.icmp6_id;
}//end Forwarder_decochain_protocol_icmp::src_port

uint16_t Forwarder_decochain_protocol_icmp::dst_port( const Packet_Hdr *pkt )
{
	return pkt->icmph.icmp6_id;
}//end Forwarder_decochain_protocol_icmp::dst_port

void Forwarder_decochain_protocol_icmp::in_pkt_mod( Packet_Hdr *pkt, const struct in6_addr *dip, uint16_t id )
{
	//LOG(INFO) << "icmp::in_pkt_mod";
	pkt->icmph.icmp6_cksum = inc_mod_checksum( pkt->icmph.icmp6_cksum, &pkt->iph.ip6_dst, dip, pkt->icmph.icmp6_id, id );
	pkt->iph.ip6_dst = *dip;
	pkt->icmph.icmp6_id = id;
}//end Forwarder_decochain_protocol_icmp::in_pkt_mod

void Forwarder_decochain_protocol_icmp::out_pkt_mod( Packet_Hdr *pkt, const struct in6_addr *sip, uint16_t id )
{
	//LOG(INFO) << "icmp::out_pkt_mod";
	pkt->icmph.icmp6_cksum = inc_mod_checksum( pkt->icmph.icmp6_cksum, &pkt->iph.ip6_src, sip, pkt->icmph.icmp6_id, id );
	pkt->iph.ip6_src = *sip;
	pkt->icmph.icmp6_id = id;
}//end Forwarder_decochain_protocol_icmp::out_pkt_mod

Forwarder_decochain_protocol_icmp::~Forwarder_decochain_protocol_icmp()
{
}//end Forwarder_decochain_protocol_icmp::~Forwarder_decochain_protocol_icmp
