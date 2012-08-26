#include "ipv6_nat.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#define VERSION "0.0.15"

static const char port_map_file[] = "/etc/ipv6nat_port_map.conf";

char* nextparam( char *a )
{
	while( !isspace( *(++a) ) && *a != '\0' );//end while
	if( *a == '\0' ) return NULL;
	*a = '\0';
	while( isspace( *(++a) ) );//end while
	if( *a == '\0' ) return NULL;

	return a;
}//end nextparam

int LoadPortMap( const char* file, Static_Port_Map** pm )
{
	struct stat st;
	FILE *f;
	char *p, *a, *b;
	int num = 0;

	*pm = NULL;

	if( stat( file, &st ) == 0 && st.st_size > 0 )
		{
		f = fopen( file, "r" );
		if( f != NULL )
			{
			p = new char[st.st_size + 1];
			if( fread( p, st.st_size, 1, f ) > 0 )
				{
				p[st.st_size] = '\0';
				for( a = p-1; a != NULL; a = strchr( a, '\n' ) )
					{
					while( isspace( *(++a) ) );//end while
					if( *a != '#' && *a != '\0' )
						{
						++num;
						}//end if
					}//end for
				if( num > 0 )
					{
					*pm = new Static_Port_Map[num];
					num = 0;
					for( a = p-1; a != NULL; a = strchr( a, '\n' ) )
						{
						while( isspace( *(++a) ) );//end while
						if( *a != '#' && *a != '\0' )
							{
							// read protocol
							if( (b = nextparam( a )) == NULL ) continue;
							strncpy( (*pm)[num].proto, a, sizeof((*pm)[num].proto) );
							(*pm)[num].proto[sizeof((*pm)[num].proto)-1] = '\0';
							a = b;

							// read WAN port
							if( (b = nextparam( a )) == NULL ) continue;
							(*pm)[num].wan_port = htons(atoi( a ));
							a = b;

							// read MAC
							if( (b = nextparam( a )) == NULL ) continue;
							if( ether_aton_r( a, (struct ether_addr*)(*pm)[num].mac ) == NULL ) continue;
							a = b;

							// read IPv6
							if( (b = nextparam( a )) == NULL ) continue;
							if( inet_pton( AF_INET6, a, &(*pm)[num].ip ) <= 0 ) continue;
							a = b;

							// read port
							(*pm)[num].port = htons(atoi( a ));
							
							// read wan_if
							if( (a = nextparam( a )) == NULL )
								{
								(*pm)[num].wan_if = 0;
								}
							else{
								(*pm)[num].wan_if = atoi( a );
								}//end if
							++num;
							}//end if
						}//end for
					if( num == 0 )
						{
						delete [] *pm;
						*pm = NULL;
						}//end if
					}//end if
				}//end if
			delete [] p;
			fclose( f );
			}//end if
		}//end if

	return num;
}//end LoadPortMap

void sig_handler( int sig )
{
	signal( sig, SIG_IGN );
}//end sig_handler

void abort( void* th_main )
{
	pthread_kill( (pthread_t)th_main, SIGTERM );
}//end abort

int main( int argc, char* argv[] )
{
	char errormsg[MAX_ERROR_MSG_SIZE];
	Ipv6_nat ipv6nat;
	Static_Port_Map* pm = NULL;
	int pm_num;
	int wanif_begin;
	int wan_num;
	int i;
	
	puts( "ipv6_nat "VERSION", Copyright (C) 2010-2011 CTQY <qiyi.caitian@gmail.com>\n" );

	// check root user
	if( getuid() != 0 )
		{
		puts( "Please run as root." );
		return 0;
		}//end if

	if( argc >= 3 )
		{
		if( strcmp( argv[2], "-w" ) == 0 )
			{
			if( argc >= 4 )
				{
				wan_num = atoi(argv[3]);
				wanif_begin = 4;
				}
			else{
				puts( "Insufficient parameters." );
				return 0;
				}//end if
			}
		else{
			wan_num = 1;
			wanif_begin = 2;
			}//end if
		if( wan_num >= 1 )
			{
			if( argc - wanif_begin >= wan_num )
				{
				const char* wan_if[wan_num];
				uint8_t wan_gw_mac_buff[wan_num][ETH_ALEN];
				const uint8_t (*wan_gw_mac)[ETH_ALEN] = NULL;

				printf( "%d WAN interface(s):\n", wan_num );
				// read wan_if and wan_gw_mac
				if( argc - wanif_begin == wan_num )
					{
					for( i = 0; i < wan_num; ++i )
						{
						wan_if[i] = argv[wanif_begin + i];
						puts( wan_if[i] );
						}//end for
					}
				else if( argc - wanif_begin >= wan_num * 2 )
					{
					for( i = 0; i < wan_num; ++i )
						{
						wan_if[i] = argv[wanif_begin + i*2];
						memcpy( wan_gw_mac_buff[i], ether_aton( argv[wanif_begin + i*2 + 1] )->ether_addr_octet, sizeof(wan_gw_mac_buff[i]) );
						puts( wan_if[i] );
						}//end for
					wan_gw_mac = wan_gw_mac_buff;
					}
				else{
					puts( "Insufficient WAN_router_MAC parameters." );
					return 0;
					}//end if

				ipv6nat.set_abort( abort, (void*)pthread_self() );

				pm_num = LoadPortMap( port_map_file, &pm );
				if( ipv6nat.Start( argv[1], wan_num, wan_if, wan_gw_mac, pm, pm_num, errormsg ) )
					{
					delete [] pm;
					signal( SIGCHLD, SIG_IGN );
					signal( SIGTSTP, SIG_IGN ); /* Various TTY signals */
					signal( SIGTTOU, SIG_IGN );
					signal( SIGTTIN, SIG_IGN );
					signal( SIGHUP, SIG_IGN );
					/*signal( SIGINT, sig_handler );
					signal( SIGQUIT, sig_handler );
					signal( SIGABRT, sig_handler );
					signal( SIGTERM, sig_handler );*/

					pause();
					ipv6nat.Stop();
					}
				else{
					delete [] pm;
					printf( "Start failed! %s\n", (errormsg[0] != '\0') ? errormsg : "Check if you've specified correct parameters." );
					}//end if
				}
			else{
				}//end if
			}
		else{
			puts( "WAN interface number must be greater than 1." );
			}//end if
		}
	else{
		puts( "NOTE: before running this, please:\n"
			"\t1.disable IPv6 kernel forwarding;\n"
			"\t2.block out-going IPv6 TCP reset packets on WAN interface;\n"
			"\t3.block out-going ICMPv6 neighbour-advertisement packets on LAN interface;\n"
			"\t4.block out-going ICMPv6 destination-unreachable packets.\n\n"
			"Usage:\n"
			"ipv6_nat LAN_interface [-w WAN_interface_number] WAN_interface0 [WAN0_router_MAC] [WAN_interface1 [WAN1_router_MAC]]...\n"
			"Example:\tipv6_nat eth0 eth1\n"
			"\t\tipv6_nat eth0 -w 2 eth1 eth2\n");
		}//end if

	return 0;
}//end main
