#ifndef CTQY_IP_WAN_MAP_H
#define CTQY_IP_WAN_MAP_H

#include <netinet/ip6.h>
#include <pthread.h>
#include <queue>

typedef struct
{
	struct in6_addr ip;
	int wan_if;
} Ip_Wan_Info;

typedef struct str_Ip_Wan_Map_List
{
	Ip_Wan_Info iwi;
	time_t start_tm; // start time of this mapping
	time_t last_tm; // last activity time
	struct str_Ip_Wan_Map_List* next;
} Ip_Wan_Map_List;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IP - WAN if mapping
class Ip_wan_map
{
public:
	Ip_wan_map( time_t soft_timeout, time_t hard_timeout );
	int GetWanIf( const struct in6_addr& ip, int suggest_wan_if );
	void UpdateTime( const struct in6_addr& ip, time_t cur_tm );
	void RemoveTimeoutMap( time_t cur_tm ); // must be called periodically
	~Ip_wan_map();

private:
	Ip_wan_map(void);
	Ip_Wan_Map_List* search( const struct in6_addr& ip, int val );
	static unsigned int hash( const struct in6_addr& ip );

	static const int kMapNum_ = 251;
	static const time_t kCoolTime_ = 10;

	const time_t soft_timeout_;
	const time_t hard_timeout_;

	Ip_Wan_Map_List* map_[kMapNum_];

	pthread_spinlock_t spin_;

	std::queue<Ip_Wan_Map_List*> cool_queue_;
};

#endif // CTQY_IP_WAN_MAP_H
