#include "port_map.h"
#include <string.h>

#define STATIC_MAP_TM 0


Port_map::Port_map(void) : wan_num_( 1 )
{
	init();
}//end Port_map::Port_map

Port_map::Port_map( int wan_num ) : wan_num_( wan_num )
{
	init();
}//end Port_map::Port_map

void Port_map::init(void)
{
	int i;

	memset( lan_map_, static_cast<int>(NULL), sizeof(lan_map_) );

	wan_map_ = new Port_Map_List*[wan_num_][PORT_NUM];
	cur_alloc_port_ = new uint16_t[wan_num_];
	for( i = 0; i < wan_num_; ++i )
		{
		memset( wan_map_[i], static_cast<int>(NULL), sizeof(wan_map_[i]) );
		cur_alloc_port_[i] = MIN_PORT - 1;
		}//end for
	pthread_spin_init( &spin_, PTHREAD_PROCESS_PRIVATE );
}//end Port_map::init

Port_Info* Port_map::Wan_map( int wan_if, uint16_t wan_port/*net order*/, time_t cur_tm )
{
	Port_Info *pi = NULL;
	Port_Map_List *p;

	if( wan_if < wan_num_ )
		{
		p = wan_map_[wan_if][wan_port];
		if( p != NULL )
			{
			if( p->last_tm != STATIC_MAP_TM )
				{
				p->last_tm = cur_tm;
				}//end if
			pi = &p->pi;
			}//end if
		}//end if

	return pi;
}//end Port_map::Wan_map

Port_Info* Port_map::Lan_map( uint16_t port/*net order*/, const struct in6_addr* ip, time_t cur_tm )
{
	Port_Info *pi = NULL;
	Port_Map_List *p;

	for( p = lan_map_[port]; p != NULL && !IN6_ARE_ADDR_EQUAL( ip, &p->pi.ip ); p = p->next );//end for
	if( p != NULL )
		{
		if( p->last_tm != STATIC_MAP_TM )
			{
			p->last_tm = cur_tm;
			}//end if
		pi = &p->pi;
		}//end if

	return pi;
}//end Port_map::Lan_map

Port_Info* Port_map::Add_static_map( uint16_t port/*net order*/, const struct in6_addr* ip, const uint8_t mac[ETH_ALEN], int wan_if,
			uint16_t wan_port/*net order*/ )
{
	Port_Info *pi = NULL;

	if( wan_if < wan_num_ )
		{
		pthread_spin_lock( &spin_ );
		if( wan_map_[wan_if][wan_port] == NULL )
			{
			pi = &add_port_map( port, ip, mac, wan_if, wan_port, STATIC_MAP_TM )->pi;
			}//end if
		pthread_spin_unlock( &spin_ );
		}//end if

	return pi;
}//end Port_map::Add_static_map

Port_Info* Port_map::Add_map( uint16_t port/*net order*/, const struct in6_addr* ip, const uint8_t mac[ETH_ALEN], int wan_if, time_t cur_tm )
{
	Port_Info *pi = NULL;
	uint16_t s;

	if( wan_if < wan_num_ )
		{
		// find empty wan port
		++cur_alloc_port_[wan_if];
		if( cur_alloc_port_[wan_if] >= PORT_NUM )
			{
			cur_alloc_port_[wan_if] = MIN_PORT;
			}//end if
		if( wan_map_[wan_if][htons(cur_alloc_port_[wan_if])] != NULL )
			{
			s = cur_alloc_port_[wan_if];
			do	{
				++cur_alloc_port_[wan_if];
				if( cur_alloc_port_[wan_if] >= PORT_NUM )
					{
					cur_alloc_port_[wan_if] = MIN_PORT;
					}//end if
				} while( wan_map_[wan_if][htons(cur_alloc_port_[wan_if])] != NULL && cur_alloc_port_[wan_if] != s );
			}//end if

		pthread_spin_lock( &spin_ );
		if( wan_map_[wan_if][htons(cur_alloc_port_[wan_if])] == NULL )
			{
			pi = &add_port_map( port, ip, mac, wan_if, htons(cur_alloc_port_[wan_if]), cur_tm )->pi;
			}//end if
		pthread_spin_unlock( &spin_ );
		}//end if

	return pi;
}//end Port_map::Add_map

void Port_map::Del_map( Port_Info* pi, time_t cur_tm )
{
	Port_Map_List **p, *lp;

	pthread_spin_lock( &spin_ );
	for( p = &lan_map_[pi->port]; *p != NULL && (*p)->pi.wan_port != pi->wan_port; p = &(*p)->next );//end for
	lp = *p;
	if( lp != NULL && lp->last_tm != STATIC_MAP_TM )
		{
		del_port_map( p, cur_tm );
		}//end if
	pthread_spin_unlock( &spin_ );
}//end Port_map::Del_map

void Port_map::Del_timeout_map( time_t cur_tm, time_t timeout, uint16_t statemask, time_t timeout2 )
{
	Port_Map_List **p, *lp;
	int hi, i;

	for( hi = MIN_PORT; hi < PORT_NUM; ++hi )
		{
		i = htons(hi);
		if( lan_map_[i] != NULL )
			{
			pthread_spin_lock( &spin_ );
			if( lan_map_[i] != NULL )
				{
				p = &lan_map_[i];
				do	{
					lp = *p;
					if( lp->last_tm != STATIC_MAP_TM && lp->last_tm + ((statemask & lp->pi.state) ? timeout2 : timeout) < cur_tm )
						{
						del_port_map( p, cur_tm );
						}
					else{
						p = &(*p)->next;
						}//end if
					} while( *p != NULL );
				}//end if
			pthread_spin_unlock( &spin_ );
			}//end if
		}//end for

	while( !cool_queue_.empty() && cool_queue_.front()->last_tm + kCoolTime_ < cur_tm )
		{
		delete cool_queue_.front();
		cool_queue_.pop();
		}//end while
}//end Port_map::Del_timeout_map

Port_Map_List* Port_map::add_port_map( uint16_t port/*net order*/, const struct in6_addr* ip, const uint8_t mac[ETH_ALEN], int wan_if,
				uint16_t wan_port/*net order*/, time_t last_tm )
{
	Port_Map_List *p = new Port_Map_List;
	memcpy( p->pi.mac, mac, ETH_ALEN );
	p->pi.ip = *ip;
	p->pi.port = port;
	p->pi.wan_if = wan_if;
	p->pi.wan_port = wan_port;
	p->pi.state = 0;
	p->last_tm = last_tm;
	p->next = lan_map_[port];
	lan_map_[port] = p;
	wan_map_[wan_if][wan_port] = p;
	return p;
}//end Port_map::add_port_map

void Port_map::del_port_map( Port_Map_List **p, time_t cur_tm )
{
	Port_Map_List *lp = *p;
	*p = lp->next;
	wan_map_[lp->pi.wan_if][lp->pi.wan_port] = NULL;
	lp->last_tm = cur_tm;
	cool_queue_.push( lp );
}//end Port_map::del_port_map

Port_map::~Port_map()
{
	Port_Map_List *p, *lp;
	int i;

	pthread_spin_destroy( &spin_ );

	delete [] cur_alloc_port_;
	for( i = 0; i < PORT_NUM; ++i )
		{
		if( lan_map_[i] != NULL )
			{
			p = lan_map_[i];
			do	{
				lp = p;
				p = lp->next;
				delete lp;
				} while( p != NULL );
			}//end if
		}//end for
	delete [] wan_map_;

	while( !cool_queue_.empty() )
		{
		delete cool_queue_.front();
		cool_queue_.pop();
		}//end while
}//end Port_map::~Port_map
