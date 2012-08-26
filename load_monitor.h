#ifndef CTQY_LOAD_MONITOR_H
#define CTQY_LOAD_MONITOR_H

#include <pthread.h>
#include <time.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Traffic_monitor
// (Maintain traffic of each wan interface during last span ~ 2*span seconds)
//
class Traffic_monitor
{
public:
	Traffic_monitor( int wan_num, time_t span );
	int Get_easiest_wan_if(void);
	void Add_traffic( int wan_if, int traffic, time_t cur_tm );
	~Traffic_monitor();

private:
	Traffic_monitor(void);

	const int wan_num_;
	const time_t span_;

	int index_;
	int (*traff_)[2]; // roll array
	time_t roll_tm_; // last rolled time

	pthread_spinlock_t spin_;

};


#endif // CTQY_LOAD_MONITOR_H
