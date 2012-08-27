HDR = load_monitor.h balancer.h protocol.h ip_wan_map.h restrictor.h port_map.h forwarder.h factory.h ipv6_nat.h
SRC = load_monitor.cpp balancer.cpp protocol.cpp ip_wan_map.cpp restrictor.cpp port_map.cpp forwarder.cpp factory.cpp ipv6_nat.cpp entry.cpp

all: $(HDR) $(SRC)
	g++ -D__OPTIMIZE__=1 -D__USE_EXTERN_INLINES=1 -Wall -pipe -O3 -Xlinker -s -pthread `pkg-config --cflags --libs glib-2.0` $(SRC) -o ipv6_nat -lrt
install:
	cp ipv6_nat /usr/sbin/;\
	if [ ! -f "/etc/ipv6nat_port_map.conf" ];\
	then\
		cp ipv6nat_port_map.conf /etc/ ;\
	fi

uninstall:
	rm /usr/sbin/ipv6_nat
