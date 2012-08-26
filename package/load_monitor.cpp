#include "load_monitor.h"
#include <string.h>
#include <glib.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Traffic_monitor
//
Traffic_monitor::Traffic_monitor( int wan_num, time_t span ) : wan_num_( wan_num ), span_( span ), index_( 0 ),
	traff_( new int[wan_num_][2] ), roll_tm_( 0 )
{
	memset( traff_, 0, sizeof( traff_[0] ) * wan_num_ );
	pthread_spin_init( &spin_, PTHREAD_PROCESS_PRIVATE );
}//end Traffic_monitor::Traffic_monitor

int Traffic_monitor::Get_easiest_wan_if(void)
{
	int mintraff = 0x7fffffff;
	int tmp;
	int easiest_if = 0;
	int wan_if;

	pthread_spin_lock( &spin_ );
	for( wan_if = 0; wan_if < wan_num_; ++wan_if )
		{
		tmp = traff_[wan_if][0] + traff_[wan_if][1];
		if( tmp < mintraff )
			{
			mintraff = tmp;
			easiest_if = wan_if;
			}//end if
		}//end for
	pthread_spin_unlock( &spin_ );

	return easiest_if;
}//end Traffic_monitor::Get_easiest_wan_if

void Traffic_monitor::Add_traffic( int wan_if, int traffic, time_t cur_tm )
{
	int i;

	if( roll_tm_ + span_ < cur_tm )
		{
		pthread_spin_lock( &spin_ );
		if( roll_tm_ + span_ < cur_tm )
			{
			roll_tm_ = cur_tm;
			// roll it
			index_ = 1 - index_;
			for( i = 0; i < wan_num_; ++i )
				{
				traff_[i][index_] = 0;
				}//end for
			}//end if
		pthread_spin_unlock( &spin_ );
		}//end if

	g_atomic_int_add( &traff_[wan_if][index_], traffic );
}//end Traffic_monitor::Add_traffic

Traffic_monitor::~Traffic_monitor()
{
	pthread_spin_destroy( &spin_ );
	delete [] traff_;
}//end Traffic_monitor::~Traffic_monitor
