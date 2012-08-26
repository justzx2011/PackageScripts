#ifndef CTQY_PORT_MAP_H
#define CTQY_PORT_MAP_H

#include <netinet/ip6.h>
#include <net/ethernet.h>
#include <pthread.h>
#include <time.h>
#include <queue>

#define PORT_NUM 65536
#define MIN_PORT 5001

typedef struct
{
	int wan_if; // 0 ~ wan_num_-1
	uint16_t wan_port; // net order
	uint8_t mac[ETH_ALEN];
	struct in6_addr ip;
	uint16_t port; // net order
	uint16_t state; // session state
} Port_Info;

typedef struct str_Port_Map_List
{
	Port_Info pi;
	time_t last_tm; // last accessed time
	struct str_Port_Map_List* next;
} Port_Map_List;

class Port_map
{
public:
	Port_map(void);
	Port_map( int wan_num );
	Port_Info* Wan_map( int wan_if, uint16_t wan_port/*net order*/, time_t cur_tm );
	Port_Info* Lan_map( uint16_t port/*net order*/, const struct in6_addr* ip, time_t cur_tm );
	Port_Info* Add_static_map( uint16_t port/*net order*/, const struct in6_addr* ip, const uint8_t mac[ETH_ALEN], int wan_if,
				uint16_t wan_port/*net order*/ );
	Port_Info* Add_map( uint16_t port/*net order*/, const struct in6_addr* ip, const uint8_t mac[ETH_ALEN], int wan_if, time_t cur_tm );
	void Del_map( Port_Info* pi, time_t cur_tm );
	void Del_timeout_map( time_t cur_tm, time_t timeout, uint16_t statemask = 0, time_t timeout2 = 0 );
	
	int get_wan_num(void) const { return wan_num_; }
	const uint16_t* get_cur_alloc_port(void) const { return cur_alloc_port_; }

	~Port_map();

private:
	Port_Map_List* add_port_map( uint16_t port/*net order*/, const struct in6_addr* ip, const uint8_t mac[ETH_ALEN], int wan_if,
					uint16_t wan_port/*net order*/, time_t last_tm );
	void del_port_map( Port_Map_List **p, time_t cur_tm );

	static const time_t kCoolTime_ = 10;

	void init(void);

	const int wan_num_;
	Port_Map_List* lan_map_[PORT_NUM]; // list, net order
	Port_Map_List* (*wan_map_)[PORT_NUM]; // unique, net order

	uint16_t *cur_alloc_port_; // host order, for wan

	pthread_spinlock_t spin_;
	
	std::queue<Port_Map_List*> cool_queue_;
};

#endif // CTQY_PORT_MAP_H
