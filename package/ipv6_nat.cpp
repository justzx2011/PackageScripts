#include "ipv6_nat.h"
#include <netpacket/packet.h>
#include <linux/rtnetlink.h>
#include <linux/filter.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <algorithm>

//#define GOOGLE_STRIP_LOG 10
//#include <glog/logging.h>


#define RA_TRANS_INTV 10

#define IPV6_IP_PREFIX "2001:2:3:4::"
#define IPV6_PREFIX_LEN 64

#define IPV6_RA_MCAST_MAC "33:33:00:00:00:01"
#define IPV6_RA_MCAST_IP "ff02::1"


#define IS_ADDR_2001( ip ) (((const sockaddr_in6*)(ip))->sin6_addr.s6_addr16[0] == htons(0x2001) )

//GetRouterMac retry time
#define GRM_RETRY_TIME 10

static uint16_t CalcCheckSum( const struct ip6_hdr* iph, uint8_t proto, const uint16_t* pack, int packlen )
{
	uint32_t cksum = 0;
	int i;

	for( i = 0; i < 8; ++i )
		{
		cksum += iph->ip6_src.s6_addr16[i];
		}//end for
	for( i = 0; i < 8; ++i )
		{
		cksum += iph->ip6_dst.s6_addr16[i];
		}//end for
#if __BYTE_ORDER == __BIG_ENDIAN
	cksum += proto;
#else // __LITTLE_ENDIAN
	cksum += proto << 8;
#endif
	cksum += htons( packlen );
	
	while( packlen > 1 )
		{
	    cksum += *(pack++);
	    packlen -= sizeof(uint16_t);
		}//end while
 
	if( packlen > 0 )
		{
#if __BYTE_ORDER == __BIG_ENDIAN
	    cksum += *((uint8_t*)pack) << 8;
#else // __LITTLE_ENDIAN
	    cksum += *((uint8_t*)pack);
#endif
		}//end if

	cksum = (cksum >> 16) + (cksum & 0xffff);
	cksum = (cksum >> 16) + (cksum & 0xffff);
	
	return (uint16_t)(~cksum);
}//end CalcCheckSum

static bool GetRouterMac( int ifindex, uint8_t mac[ETH_ALEN] )
{
	struct
	{
		struct nlmsghdr nlmsg;
		struct rtgenmsg g;
	} req;
	char buff[2048];
	struct sockaddr_nl nl;
	struct nlmsghdr *nlp;
	struct ndmsg *r;
	struct rtattr *rta;
	int sd;
	ssize_t n, len;
	bool succ = false;

	sd = socket( AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE );
	if( sd < 0 )
		{
		return false;
		}//end if

	nl.nl_family = AF_NETLINK;
	nl.nl_pad = 0;
	nl.nl_pid = getpid();
	nl.nl_groups = 0;

	if( bind( sd, (struct sockaddr*)&nl, sizeof(nl) ) == 0 )
		{
		req.nlmsg.nlmsg_len = sizeof(req);
		req.nlmsg.nlmsg_type = RTM_GETNEIGH;
		req.nlmsg.nlmsg_flags = NLM_F_ROOT|NLM_F_MATCH|NLM_F_REQUEST;
		req.nlmsg.nlmsg_seq = 0;
		req.nlmsg.nlmsg_pid = 0;
		req.g.rtgen_family = AF_INET6;

		if( send( sd, &req, sizeof(req), 0 ) > 0 && (n = recv( sd, buff, sizeof(buff), 0 )) > 0 )
			{
			for( nlp = (struct nlmsghdr*)buff; nlp->nlmsg_type != NLMSG_DONE && NLMSG_OK( nlp, static_cast<int>(n) );
				 nlp = NLMSG_NEXT( nlp, n ) )
				{
				r = (struct ndmsg*)NLMSG_DATA(nlp);
				if( r->ndm_ifindex == ifindex && ( r->ndm_flags & NTF_ROUTER ) && ( r->ndm_state & NUD_REACHABLE ) )
					{
					len = nlp->nlmsg_len - NLMSG_LENGTH(sizeof(*r));
					for( rta = (struct rtattr*)(((char*)r) + NLMSG_ALIGN(sizeof(struct ndmsg)));
						RTA_OK(rta, len) && rta->rta_type < NDA_MAX; rta = RTA_NEXT(rta, len) )
						{
						if( rta->rta_type == NDA_LLADDR )
							{
							memcpy( mac, RTA_DATA(rta), ETH_ALEN );
							succ = true;
							break;
							}//end if
						}//end for
					break;
					}//end if
				}//end for
			}//end if
		}//end if

	close( sd );

	return succ;
}//end GetRouterMac

static uint32_t GetMTU( const char *if_name )
{
	struct ifreq ifr;
	int sd;

	sd = socket( AF_INET6, SOCK_DGRAM, 0 );
	if( sd < 0 )
		{
		return 0;
		}//end if

	strcpy( ifr.ifr_name, if_name );
	ifr.ifr_mtu = 0;
	ioctl( sd, SIOCGIFMTU, (char*)&ifr );

	close( sd );

	return ifr.ifr_mtu;
}//end GetMTU

Ipv6_nat::Ipv6_nat(void) : run_(false), wan_num_(0), wan_param_(NULL), th_lan_(0), th_wan_(NULL), th_clean_(0), th_ra_(0), mtu_(0),
	abort_(NULL), abort_arg_(NULL)
{
	lan_param_.sd_lan = 0;
	memset( forwarder_, static_cast<int>(NULL), sizeof(forwarder_) );
}//end Ipv6_nat::Ipv6_nat

void Ipv6_nat::set_abort( AbortProc abort, void* arg )
{
	abort_ = abort;
	abort_arg_ = arg;
}//end Ipv6_nat::set_abort

int Ipv6_nat::set_static_port_map( const Static_Port_Map pm[], int num )
{
	int succ = 0;
	Forwarder *fwd;
	int i;

	for( i = 0; i < num; ++i )
		{
		fwd = NULL;
		if( strcasecmp( pm[i].proto, "tcp" ) == 0 )
			{
			fwd = forwarder_[TCP_ID];
			}
		else if( strcasecmp( pm[i].proto, "udp" ) == 0 )
			{
			fwd = forwarder_[UDP_ID];
			}//end if
		if( fwd != NULL && fwd->Add_static_map( pm[i].port, &pm[i].ip, pm[i].mac, pm[i].wan_if, pm[i].wan_port ) )
			{
			++succ;
			}//end if
		}//end for

	return succ;
}//end Ipv6_nat::set_static_port_map

bool Ipv6_nat::Start( const char* lan_if, int wan_num, const char* wan_if[], const uint8_t wan_gw_mac[][ETH_ALEN], const Static_Port_Map pm[],
		int pm_num, char errormsg[] )
{
	static const struct sock_filter filter[] =
	{
		// check for ipv6 proto
		BPF_STMT(BPF_LD|BPF_B|BPF_ABS, sizeof(struct ether_header) + sizeof(uint32_t) + sizeof(uint16_t)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, IPPROTO_TCP, 2, 0),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, IPPROTO_UDP, 1, 0),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, IPPROTO_ICMPV6, 0, 1),
		// returns
		BPF_STMT(BPF_RET|BPF_K, 0x0000ffff ), // pass
		BPF_STMT(BPF_RET|BPF_K, 0) // reject
	};
	static const struct sock_fprog fprog =
	{
		sizeof(filter) / sizeof(filter[0]),
		(struct sock_filter*)filter
	};
	sockaddr_ll sa;	
	struct ifaddrs *ifa, *i;
	bool getwan[wan_num], wan2001[wan_num];
	int getmaccnt;
	int wi, j;

	errormsg[0] = '\0';

	if( run_ )
		{
		return true;
		}//end if

	run_ = true;

	wan_num_ = wan_num;

	if( (lan_param_.sd_lan = socket( AF_PACKET, SOCK_RAW, htons(ETH_P_IPV6) )) < 0 )
		{
		return false;
		}//end if

	wan_param_ = new Wan_Param[wan_num_];
	memset( wan_param_, 0, sizeof(wan_param_[0]) * wan_num_ );
	for( wi = 0; wi < wan_num_; ++wi )
		{
		if( (wan_param_[wi].sd_wan = socket( AF_PACKET, SOCK_RAW, htons(ETH_P_IPV6) )) < 0 )
			{
			Stop();
			return false;
			}//end if
		}//end for

	setuid( getuid() );

	// set filter
	if( setsockopt( lan_param_.sd_lan, SOL_SOCKET, SO_ATTACH_FILTER, &fprog, sizeof(fprog) ) < 0 )
		{
		Stop();
		return false;
		}//end if
	for( wi = 0; wi < wan_num_; ++wi )
		{
		if( setsockopt( wan_param_[wi].sd_wan, SOL_SOCKET, SO_ATTACH_FILTER, &fprog, sizeof(fprog) ) < 0 )
			{
			Stop();
			return false;
			}//end if
		}//end for

	// bind sockets
	memset( &sa, 0, sizeof(sa) );
	sa.sll_family = AF_PACKET;
	sa.sll_protocol = htons(ETH_P_IPV6);

	sa.sll_ifindex = if_nametoindex( lan_if );
	if( bind( lan_param_.sd_lan, (struct sockaddr*)&sa, sizeof(sa) ) < 0 )
		{
		Stop();
		return false;
		}//end if

	for( wi = 0; wi < wan_num_; ++wi )
		{
		sa.sll_ifindex = if_nametoindex( wan_if[wi] );
		if( bind( wan_param_[wi].sd_wan, (struct sockaddr*)&sa, sizeof(sa) ) < 0 )
			{
			Stop();
			return false;
			}//end if
		}//end for
	//LOG(INFO) << "sockets ready";

	// get MAC and ipv6 addr
	getmaccnt = 0;
	memset( getwan, false, sizeof(getwan[0]) * wan_num_ );
	memset( wan2001, false, sizeof(getwan[0]) * wan_num_ );
	if( getifaddrs( &ifa ) != 0 )
		{
		Stop();
		return false;
		}//end if
	for( i = ifa; i != NULL; i = i->ifa_next )
		{
		switch( i->ifa_addr->sa_family )
			{
			case AF_PACKET:
				{
				if( strcmp( i->ifa_name, lan_if ) == 0 )
					{
					memcpy( lan_param_.lan_mac, ((sockaddr_ll*)i->ifa_addr)->sll_addr, ETH_ALEN );
					++getmaccnt;
					}
				else{
					for( wi = 0; wi < wan_num_; ++wi )
						{
						if( strcmp( i->ifa_name, wan_if[wi] ) == 0)
							{
							memcpy( wan_param_[wi].wan_mac, ((sockaddr_ll*)i->ifa_addr)->sll_addr, ETH_ALEN );
							++getmaccnt;
							break;
							}//end if
						}//end for
					}//end if
				}break;//end AF_PACKET

			case AF_INET6:
				{
				if( IN6_IS_ADDR_LINKLOCAL(&((sockaddr_in6*)i->ifa_addr)->sin6_addr) && strcmp( i->ifa_name, lan_if ) == 0 )
					{
					lan_ip_ = ((sockaddr_in6*)i->ifa_addr)->sin6_addr;
					}
				else if( IN6_IS_ADDR_GLOBAL(&((sockaddr_in6*)i->ifa_addr)->sin6_addr) )
					{
					for( wi = 0; wi < wan_num_; ++wi )
						{
						if( !wan2001[wi] && strcmp( i->ifa_name, wan_if[wi] ) == 0 )
							{
							wan_param_[wi].wan_ip = ((sockaddr_in6*)i->ifa_addr)->sin6_addr;
							getwan[wi] = true;
							if( IS_ADDR_2001( i->ifa_addr ) )
								{
								wan2001[wi] = true;
								}//end if
							break;
							}//end if
						}//end for
					}//end if
				}break;//end AF_INET6

			default:break;
			}//end switch
		}//end for
	freeifaddrs( ifa );

	//LOG(INFO) << "getmaccnt:" << getmaccnt;

	if( getmaccnt < 1 + wan_num_ )
		{
		Stop();
		return false;
		}//end if

	// waiting for ipv6 addr
	//while( !getwan )
	for(;;)
		{
		for( wi = 0; wi < wan_num_ && getwan[wi]; ++wi );//end for
		if( wi >= wan_num_ ) break;
		sleep( 5 );
		if( getifaddrs( &ifa ) != 0 )
			{
			Stop();
			return false;
			}//end if
		for( i = ifa; i != NULL; i = i->ifa_next )
			{
			if( i->ifa_addr->sa_family == AF_INET6 && IN6_IS_ADDR_GLOBAL(&((sockaddr_in6*)i->ifa_addr)->sin6_addr) )
				{
				for( wi = 0; wi < wan_num_; ++wi )
					{
					if( !wan2001[wi] && strcmp( i->ifa_name, wan_if[wi] ) == 0 )
						{
						wan_param_[wi].wan_ip = ((sockaddr_in6*)i->ifa_addr)->sin6_addr;
						getwan[wi] = true;
						if( IS_ADDR_2001( i->ifa_addr ) )
							{
							wan2001[wi] = true;
							break;
							}//end if
						}//end if
					}//end for
				}//end if
			}//end for
		freeifaddrs( ifa );
		}//end while

	if( wan_gw_mac != NULL )
		{
		for( wi = 0; wi < wan_num_; ++wi )
			{
			memcpy( wan_param_[wi].wan_gw_mac, wan_gw_mac[wi], ETH_ALEN );
			}//end for
		}
	else{
		for( wi = 0; wi < wan_num_; ++wi )
			{
			for( j = 0; j < GRM_RETRY_TIME && !GetRouterMac( if_nametoindex( wan_if[wi] ), wan_param_[wi].wan_gw_mac ); ++j, sleep(1) );//end for
			if( j >= GRM_RETRY_TIME )
				{
				sprintf( errormsg, "Get %s's gateway MAC failed! Please manually specify the gateway MAC of it and try again.\n", wan_if[wi] );
				Stop();
				return false;
				}//end if
			}//end for
		}//end if

	//LOG(INFO) << "wan_gw_mac ready.";

	mtu_ = 0xffff;
	for( wi = 0; wi < wan_num_; ++wi )
		{
		mtu_ = std::min( GetMTU( wan_if[wi] ), mtu_ );
		}//end for
	if( mtu_ == 0 )
		{
		//LOG(ERROR) << "GetMTU error!";
		strcpy( errormsg, "There is something wrong with getting MTU of WAN interfaces." );
		Stop();
		return false;
		}//end if
	//LOG(INFO) << "mtu_ ready.";

	if( !CreateForwarders( wan_num_, forwarder_ ) )
		{
		Stop();
		return false;
		}//end if

	set_static_port_map( pm, pm_num );

	cur_tm_ = time( NULL );
	// create threads
	if( pthread_create( &th_lan_, NULL, (void* (*)(void*))lan_proc, this ) != 0 )
		{
		th_lan_ = 0;
		Stop();
		return false;
		}//end if
	//LOG(INFO) << "th_lan_ ready.";

	th_wan_ = new pthread_t[wan_num_];
	memset( th_wan_, 0, sizeof(th_wan_[0]) * wan_num_ );
	for( wi = 0; wi < wan_num_; ++wi )
		{
		Wan_Proc_Param *wp = new Wan_Proc_Param;
		wp->nat = this;
		wp->wan_if = wi;
		if( pthread_create( &th_wan_[wi], NULL, (void* (*)(void*))wan_proc, wp ) != 0 )
			{
			th_wan_[wi] = 0;
			//LOG(ERROR) << "th_wan_[" << wi << "] failed! errno: " << errno;
			Stop();
			return false;
			}//end if
		}//end for
	//LOG(INFO) << "th_wan_[] ready.";

	if( pthread_create( &th_clean_, NULL, (void* (*)(void*))clean_proc, this ) != 0 )
		{
		th_clean_ = 0;
		Stop();
		return false;
		}//end if
	if( pthread_create( &th_ra_, NULL, (void* (*)(void*))ra_proc, this ) != 0 )
		{
		th_ra_ = 0;
		Stop();
		return false;
		}//end if

	//LOG(INFO) << "threads ready.";

	return true;
}//end Ipv6_nat::Start

void* Ipv6_nat::lan_proc( Ipv6_nat *nat )
{
	union
	{
		Packet_Hdr pkt;
		char buff[MAX_PKT_SIZE];
	};
	ssize_t len;
	Forwarder *fwd;

	while( nat->run_ )
		{
		len = recv( nat->lan_param_.sd_lan, buff, MAX_PKT_SIZE, 0 );
		if( len > 0 )
			{
			fwd = NULL;
			switch( pkt.iph.ip6_nxt )
				{
				case IPPROTO_TCP:
					{
					fwd = nat->forwarder_[TCP_ID];
					}break;//end IPPROTO_TCP

				case IPPROTO_UDP:
					{
					fwd = nat->forwarder_[UDP_ID];
					}break;//end IPPROTO_UDP

				default:// IPPROTO_ICMPV6
					{
					switch( pkt.icmph.icmp6_type )
						{
						case ICMP6_ECHO_REQUEST:
							{
							fwd = nat->forwarder_[ICMP_ID];
							}break;//end ICMP6_ECHO_REQUEST

						case ND_ROUTER_SOLICIT:
							{
							nat->fill_ra_pkt( &pkt );
							send( nat->lan_param_.sd_lan, &pkt, sizeof( pkt.ra ) + ((char*)&pkt.ra - (char*)&pkt), 0 );
							}break;//end ND_ROUTER_SOLICIT
							
						case ND_NEIGHBOR_SOLICIT:
							{
							if( IN6_ARE_ADDR_EQUAL( &nat->lan_ip_, &pkt.ns.nd_ns_target )
								&& IN6_ARE_ADDR_EQUAL( &nat->lan_ip_, &pkt.iph.ip6_dst ) )
								{
								memcpy( pkt.eh.ether_dhost, pkt.eh.ether_shost, ETH_ALEN );
								memcpy( pkt.eh.ether_shost, nat->lan_param_.lan_mac, ETH_ALEN );
								pkt.iph.ip6_dst = pkt.iph.ip6_src;
								pkt.iph.ip6_src = nat->lan_ip_;
								pkt.iph.ip6_plen = htons( sizeof(pkt.na) );
								pkt.na.nd_na_type = ND_NEIGHBOR_ADVERT;
								pkt.na.nd_na_cksum = 0;
								pkt.na.nd_na_flags_reserved = ND_NA_FLAG_ROUTER | ND_NA_FLAG_SOLICITED;
								pkt.na.nd_na_cksum = CalcCheckSum( &pkt.iph, IPPROTO_ICMPV6, (uint16_t*)&pkt.na, sizeof( pkt.na ) );
								send( nat->lan_param_.sd_lan, buff, sizeof(pkt.na) + ((char*)&pkt.na - (char*)&pkt), 0 );
								}//end if
							}break;//end ND_NEIGHBOR_SOLICIT

						default:break;
						}//end switch
					}break;//end default(IPPROTO_ICMPV6)
				}//end switch
			if( fwd != NULL )
				{
				fwd->Forward_out( nat->wan_param_, &nat->lan_param_, &pkt, len, nat->cur_tm_ );
				}//end if
			}
		else if( len < 0 && (errno == EINTR  || errno == EMSGSIZE || errno == ENOBUFS || errno == ENOENT) )
			{
			// ignore that
			}
		else{
			if( nat->abort_ != NULL )
				{
				nat->abort_( nat->abort_arg_ );
				}//end if
			break;
			}//end while
		}//end while

	return NULL;
}//end Ipv6_nat::lan_proc

void* Ipv6_nat::wan_proc( Wan_Proc_Param *param )
{
	union
	{
		Packet_Hdr pkt;
		char buff[MAX_PKT_SIZE];
	};
	ssize_t len;
	Forwarder *fwd;

	//LOG(INFO) << "wan_proc[" << param->wan_if << "] starts.";

	while( param->nat->run_ )
		{
		len = recv( param->nat->wan_param_[param->wan_if].sd_wan, buff, MAX_PKT_SIZE, 0 );
		if( len > 0 )
			{
			fwd = NULL;
			switch( pkt.iph.ip6_nxt )
				{
				case IPPROTO_TCP:
					{
					fwd = param->nat->forwarder_[TCP_ID];
					}break;//end IPPROTO_TCP

				case IPPROTO_UDP:
					{
					fwd = param->nat->forwarder_[UDP_ID];
					}break;//end IPPROTO_UDP

				default:// IPPROTO_ICMPV6
					{
					if( pkt.icmph.icmp6_type == ICMP6_ECHO_REPLY )
						{
						fwd = param->nat->forwarder_[ICMP_ID];
						}//end if
					}break;//end default(IPPROTO_ICMPV6)
				}//end switch
			if( fwd != NULL )
				{
				fwd->Forward_in( param->wan_if, &param->nat->lan_param_, &pkt, len, param->nat->cur_tm_ );
				}//end if
			}
		else if( len < 0 && (errno == EINTR  || errno == EMSGSIZE || errno == ENOBUFS || errno == ENOENT) )
			{
			// ignore that
			}
		else{
			if( param->nat->abort_ != NULL )
				{
				param->nat->abort_( param->nat->abort_arg_ );
				}//end if
			break;
			}//end while
		}//end while

	delete param;
	return NULL;
}//end Ipv6_nat::wan_proc

void* Ipv6_nat::clean_proc( Ipv6_nat *nat )
{
	int i;

	while( nat->run_ )
		{
		for( i = 0; i < PROTO_NUM && nat->run_; ++i )
			{
			nat->cur_tm_ = time( NULL );
			if( nat->forwarder_[i] != NULL )
				{
				nat->forwarder_[i]->Del_timeout_map( nat->cur_tm_ );
				}//end if
			}//end for
		// wait for one minute before check again
		for( i = 0; i < 60 && nat->run_; ++i )
			{
			nat->cur_tm_ = time( NULL );
			sleep( 1 );
			}//end for
		}//end while

	return NULL;
}//end Ipv6_nat::clean_proc

void* Ipv6_nat::ra_proc( Ipv6_nat *nat )
{
	Packet_Hdr pkt;
	int i;

	nat->fill_ra_pkt( &pkt );

	while( nat->run_ )
		{
		// send router advertisement
		send( nat->lan_param_.sd_lan, &pkt, sizeof( pkt.ra ) + ((char*)&pkt.ra - (char*)&pkt), 0 );
		for( i = 0; i < RA_TRANS_INTV && nat->run_; ++i )
			{
			sleep( 1 );
			}//end for
		}//end while

	return NULL;
}//end Ipv6_nat::ra_proc

void Ipv6_nat::fill_ra_pkt( Packet_Hdr *pkt )
{
	memcpy( pkt->eh.ether_dhost, ether_aton( IPV6_RA_MCAST_MAC )->ether_addr_octet, ETH_ALEN );
	memcpy( pkt->eh.ether_shost, lan_param_.lan_mac, ETH_ALEN );
	pkt->eh.ether_type = htons( ETHERTYPE_IPV6 );

	pkt->iph.ip6_flow = 0;
	pkt->iph.ip6_vfc = 6 << 4;
	pkt->iph.ip6_nxt = IPPROTO_ICMPV6;
	pkt->iph.ip6_hlim = 0xff;
	pkt->iph.ip6_plen = htons( sizeof( pkt->ra ) );
	pkt->iph.ip6_src = lan_ip_;
	inet_pton( AF_INET6, IPV6_RA_MCAST_IP, &pkt->iph.ip6_dst );

	pkt->ra.ra.nd_ra_type = ND_ROUTER_ADVERT;
	pkt->ra.ra.nd_ra_code = 0;
	pkt->ra.ra.nd_ra_cksum = 0;
	pkt->ra.ra.nd_ra_curhoplimit = 64;
	pkt->ra.ra.nd_ra_flags_reserved = 0;
	pkt->ra.ra.nd_ra_router_lifetime = htons( 1800 );
	pkt->ra.ra.nd_ra_reachable = 0;
	pkt->ra.ra.nd_ra_retransmit = 0;

	pkt->ra.opth.nd_opt_type = ND_OPT_SOURCE_LINKADDR;
	pkt->ra.opth.nd_opt_len = 1;
	memcpy( pkt->ra.mac, lan_param_.lan_mac, ETH_ALEN );

	pkt->ra.mtu.nd_opt_mtu_type = ND_OPT_MTU;
	pkt->ra.mtu.nd_opt_mtu_len = 1;
	pkt->ra.mtu.nd_opt_mtu_reserved = 0;
	pkt->ra.mtu.nd_opt_mtu_mtu = htonl( mtu_ );

	pkt->ra.pi.nd_opt_pi_type = ND_OPT_PREFIX_INFORMATION;
	pkt->ra.pi.nd_opt_pi_len = 4;
	pkt->ra.pi.nd_opt_pi_prefix_len = IPV6_PREFIX_LEN;
	pkt->ra.pi.nd_opt_pi_flags_reserved = ND_OPT_PI_FLAG_ONLINK | ND_OPT_PI_FLAG_AUTO;
	pkt->ra.pi.nd_opt_pi_valid_time = htonl( 2592000 );
	pkt->ra.pi.nd_opt_pi_preferred_time = htonl( 604800 );
	pkt->ra.pi.nd_opt_pi_reserved2 = 0;
	inet_pton( AF_INET6, IPV6_IP_PREFIX, &pkt->ra.pi.nd_opt_pi_prefix );

	pkt->ra.ra.nd_ra_cksum = CalcCheckSum( &pkt->iph, IPPROTO_ICMPV6, (uint16_t*)&pkt->ra, sizeof( pkt->ra ) );
}//end Ipv6_nat::fill_ra_pkt

void Ipv6_nat::Stop(void)
{
	int i;

	if( run_ )
		{
		run_ = false;

		if( wan_param_ != NULL )
			{
			for( i = 0; i < wan_num_; ++i )
				{
				shutdown( wan_param_[i].sd_wan, SHUT_RDWR );
				close( wan_param_[i].sd_wan );
				wan_param_[i].sd_wan = 0;
				}//end for
			}//end if
		shutdown( lan_param_.sd_lan, SHUT_RDWR );
		close( lan_param_.sd_lan );
		lan_param_.sd_lan = 0;

		if( th_lan_ != 0 )
			{
			pthread_join( th_lan_, NULL );
			pthread_detach( th_lan_ );
			th_lan_ = 0;
			}//end if
		if( th_wan_ != NULL )
			{
			for( i = 0; i < wan_num_; ++i )
				{
				pthread_join( th_wan_[i], NULL );
				pthread_detach( th_wan_[i] );
				}//end for
			delete [] th_wan_;
			th_wan_ = NULL;
			}//end if
		if( th_clean_ != 0 )
			{
			pthread_join( th_clean_, NULL );
			pthread_detach( th_clean_ );
			th_clean_ = 0;
			}//end if
		if( th_ra_ != 0 )
			{
			pthread_join( th_ra_, NULL );
			pthread_detach( th_ra_ );
			th_ra_ = 0;
			}//end if

		for( i = 0; i < PROTO_NUM; ++i )
			{
			delete forwarder_[i];
			forwarder_[i] = NULL;
			}//end for

		delete [] wan_param_;
		wan_param_ = NULL;
		}//end if
}//end Ipv6_nat::Stop

Ipv6_nat::~Ipv6_nat()
{
	Stop();
}//end Ipv6_nat::~Ipv6_nat
