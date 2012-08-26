#ifndef CTQY_FACTORY_H
#define CTQY_FACTORY_H

#include "forwarder.h"

enum PROTO_ID
{
	TCP_ID = 0,
	UDP_ID,
	ICMP_ID,
	PROTO_NUM
};

bool CreateForwarders( int wan_num, Forwarder* fwd[PROTO_NUM] );

#endif // CTQY_FACTORY_H
