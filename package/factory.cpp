#include "factory.h"
#include "protocol.h"
#include "restrictor.h"
#include "balancer.h"


bool CreateForwarders( int wan_num, Forwarder* fwd[PROTO_NUM] )
{
	if( wan_num > 1 )
		{
		bool exclist[PORT_NUM];
		Ip_wan_map* iwm = new Ip_wan_map( 1200, 14400 );
		Traffic_monitor* tm = new Traffic_monitor( wan_num, 30 );

		memset( exclist, false, sizeof(exclist) );
		exclist[htons(22)] = true;
		exclist[htons(80)] = true;
		exclist[htons(443)] = true;

		fwd[TCP_ID] = new Forwarder_decochain_restrictor_exclist( exclist, iwm, true, new Forwarder_decochain_protocol_tcp( 
						new Forwarder_decochain_balancer_traffic( tm, true, new Forwarder_concrete( wan_num ) ) ) );
		fwd[UDP_ID] = new Forwarder_decochain_restrictor( iwm, false, new Forwarder_decochain_protocol_udp(
						new Forwarder_decochain_balancer_traffic( tm, false, new Forwarder_concrete( wan_num ) ) ) );
		fwd[ICMP_ID] = new Forwarder_decochain_protocol_icmp( new Forwarder_concrete( wan_num ) );
		}
	else{
		fwd[TCP_ID] = new Forwarder_decochain_protocol_tcp( new Forwarder_concrete( wan_num ) );
		fwd[UDP_ID] = new Forwarder_decochain_protocol_udp( new Forwarder_concrete( wan_num ) );
		fwd[ICMP_ID] = new Forwarder_decochain_protocol_icmp( new Forwarder_concrete( wan_num ) );
		}//end if

	return true;
}//end CreateForwarders
