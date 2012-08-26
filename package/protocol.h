#ifndef CTQY_PROTOCOL_H
#define CTQY_PROTOCOL_H

#include "forwarder.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_decochain_protocol (Abstract)
//
// Chain of Responsibility: src_port() and dst_port()
// Decorator: others
//
class Forwarder_decochain_protocol : public Forwarder_decochain
{
public:
	Forwarder_decochain_protocol( Forwarder* fwd );

	// internal functions: packet sending
	// two template methods:
	virtual bool send_pkt_out( Port_Info* pi, const Wan_Param* wan_param, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len,
					time_t cur_tm );
	virtual bool send_pkt_in( Port_Info* pi, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len, time_t cur_tm );

	// force subclasses to implement these two:
	virtual uint16_t src_port( const Packet_Hdr *pkt ) = 0;
	virtual uint16_t dst_port( const Packet_Hdr *pkt ) = 0;

	virtual ~Forwarder_decochain_protocol();

protected:
	// packet modifying functions:
	static uint16_t inc_mod_checksum( uint16_t srccksum, const struct in6_addr *ip, const struct in6_addr *ipa, uint16_t port, uint16_t porta );

	virtual void in_pkt_mod( Packet_Hdr *pkt, const struct in6_addr *dip, uint16_t dport ) = 0;
	virtual void out_pkt_mod( Packet_Hdr *pkt, const struct in6_addr *sip, uint16_t sport ) = 0;

private:
	Forwarder_decochain_protocol(void);

};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_decochain_protocol_tcp
//
class Forwarder_decochain_protocol_tcp : public Forwarder_decochain_protocol
{
public:
	Forwarder_decochain_protocol_tcp( Forwarder* fwd );

	virtual void Del_timeout_map( time_t cur_tm, time_t timeout = kTcpTimeout_, uint16_t statemask = kTcpHalfSyn_,
					time_t timeout2 = kTcpSynTimeout_ );

	// internal functions: port mapping
	virtual Port_Info* Add_map( const Packet_Hdr* pkt, uint16_t port/*net order*/, int wan_if, time_t cur_tm );

	// internal function: packet sending
	virtual bool send_pkt_out( Port_Info* pi, const Wan_Param* wan_param, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len,
					time_t cur_tm );
	virtual bool send_pkt_in( Port_Info* pi, const Lan_Param* lan_param, Packet_Hdr *pkt, ssize_t len, time_t cur_tm );
	virtual uint16_t src_port( const Packet_Hdr *pkt );
	virtual uint16_t dst_port( const Packet_Hdr *pkt );

	virtual ~Forwarder_decochain_protocol_tcp();

protected:
	// packet modifying
	virtual void in_pkt_mod( Packet_Hdr *pkt, const struct in6_addr *dip, uint16_t dport );
	virtual void out_pkt_mod( Packet_Hdr *pkt, const struct in6_addr *sip, uint16_t sport );

private:
	Forwarder_decochain_protocol_tcp(void);

	static const time_t kTcpTimeout_ = 28800;
	static const time_t kTcpSynTimeout_ = 30;

	static const uint16_t kTcpHalfSyn_ = 0x01;
	static const uint16_t kTcpSrcFin_ = 0x02;
	static const uint16_t kTcpDstFin_ = 0x04;
	static const uint16_t kTcpFin_ = kTcpSrcFin_ | kTcpDstFin_;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_decochain_protocol_udp
//
class Forwarder_decochain_protocol_udp : public Forwarder_decochain_protocol
{
public:
	Forwarder_decochain_protocol_udp( Forwarder* fwd );

	virtual void Del_timeout_map( time_t cur_tm, time_t timeout = kUdpTimeout_, uint16_t statemask = 0, time_t timeout2 = 0 );

	virtual uint16_t src_port( const Packet_Hdr *pkt );
	virtual uint16_t dst_port( const Packet_Hdr *pkt );

	virtual ~Forwarder_decochain_protocol_udp();

protected:
	// packet modifying
	virtual void in_pkt_mod( Packet_Hdr *pkt, const struct in6_addr *dip, uint16_t dport );
	virtual void out_pkt_mod( Packet_Hdr *pkt, const struct in6_addr *sip, uint16_t sport );

private:
	Forwarder_decochain_protocol_udp(void);

	static const time_t kUdpTimeout_ = 30;

};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarder_decochain_protocol_icmp
//
class Forwarder_decochain_protocol_icmp : public Forwarder_decochain_protocol
{
public:
	Forwarder_decochain_protocol_icmp( Forwarder* fwd );

	virtual void Del_timeout_map( time_t cur_tm, time_t timeout = kIcmpTimeout_, uint16_t statemask = 0, time_t timeout2 = 0 );

	virtual uint16_t src_port( const Packet_Hdr *pkt );
	virtual uint16_t dst_port( const Packet_Hdr *pkt );

	virtual ~Forwarder_decochain_protocol_icmp();

protected:
	// packet modifying
	virtual void in_pkt_mod( Packet_Hdr *pkt, const struct in6_addr *dip, uint16_t dport );
	virtual void out_pkt_mod( Packet_Hdr *pkt, const struct in6_addr *sip, uint16_t sport );

private:
	Forwarder_decochain_protocol_icmp(void);

	static const time_t kIcmpTimeout_ = 20;

};

#endif // CTQY_PROTOCOL_H
