#include "ip_wan_map.h"
#include <string.h>
#include <time.h>


Ip_wan_map::Ip_wan_map( time_t soft_timeout, time_t hard_timeout ) : soft_timeout_( soft_timeout ), hard_timeout_( hard_timeout )
{
	memset( map_, static_cast<int>(NULL), sizeof( map_ ) );
	pthread_spin_init( &spin_, PTHREAD_PROCESS_PRIVATE );
}//end Ip_wan_map::Ip_wan_map

int Ip_wan_map::GetWanIf( const struct in6_addr& ip, int suggest_wan_if )
{
	Ip_Wan_Map_List *node;
	int val = hash( ip );

	pthread_spin_lock( &spin_ );
	node = search( ip, val );
	if( node == NULL )
		{
		node = new Ip_Wan_Map_List;
		node->iwi.ip = ip;
		node->iwi.wan_if = suggest_wan_if;
		node->start_tm = node->last_tm = time( NULL );
		node->next = map_[val];
		map_[val] = node;
		}
	else{
		node->last_tm = time( NULL );
		}//end if
	pthread_spin_unlock( &spin_ );

	return node->iwi.wan_if;
}//end Ip_wan_map::GetWanIf

void Ip_wan_map::UpdateTime( const struct in6_addr& ip, time_t cur_tm )
{
	Ip_Wan_Map_List *node = search( ip, hash( ip ) );
	if( node != NULL )
		{
		node->last_tm = cur_tm;
		}//end if
}//end Ip_wan_map::UpdateTime

void Ip_wan_map::RemoveTimeoutMap( time_t cur_tm )
{
	Ip_Wan_Map_List **p, *lp;
	int i;

	for( i = 0; i < kMapNum_; ++i )
		{
		p = &map_[i];
		while( *p != NULL )
			{
			if( (*p)->last_tm + soft_timeout_ < cur_tm || (*p)->start_tm + hard_timeout_ < cur_tm )
				{// remove it
				pthread_spin_lock( &spin_ );
				lp = *p;
				*p = lp->next;
				pthread_spin_unlock( &spin_ );

				lp->last_tm = cur_tm;
				cool_queue_.push( lp );
				}
			else{
				p = &(*p)->next;
				}//end if
			}//end while
		}//end for
	while( !cool_queue_.empty() && cool_queue_.front()->last_tm + kCoolTime_ < cur_tm )
		{
		delete cool_queue_.front();
		cool_queue_.pop();
		}//end while
}//end Ip_wan_map::RemoveTimeoutMap

Ip_Wan_Map_List* Ip_wan_map::search( const struct in6_addr& ip, int val )
{
	Ip_Wan_Map_List* node;
	for( node = map_[val]; node != NULL && !IN6_ARE_ADDR_EQUAL( &ip, &node->iwi.ip ); node = node->next );//end for
	return node;
}//end Ip_wan_map::search

unsigned int Ip_wan_map::hash( const struct in6_addr& ip )
{
	unsigned long h = 0;
	unsigned long g;
	unsigned int i;

	for( i = 0; i < sizeof(ip); ++i )
		{
		h = ( h << 4 ) + ip.s6_addr[i];
		g = h & 0xF0000000L;
		if( g != 0 )
			{
			h ^= g >> 24;
			}//end if
		h &= ~g;
		}//end while

	return h % kMapNum_;
}//end Ip_wan_map::hash

Ip_wan_map::~Ip_wan_map()
{
	Ip_Wan_Map_List *p, *lp;
	int i;

	pthread_spin_destroy( &spin_ );
	for( i = 0; i < kMapNum_; ++i )
		{
		if( map_[i] != NULL )
			{
			p = map_[i];
			do	{
				lp = p;
				p = lp->next;
				delete lp;
				} while( p != NULL );
			}//end if
		}//end for
	while( !cool_queue_.empty() )
		{
		delete cool_queue_.front();
		cool_queue_.pop();
		}//end while
}//end Ip_wan_map::~Ip_wan_map
