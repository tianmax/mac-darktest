#include<errno.h>
#include<error.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <math.h>
#include <netinet/ip.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <linux/wireless.h>
#include <linux/if.h>
#include <net/ethernet.h>
#include <netinet/ether.h>


typedef unsigned char      uchar; 

int64_t timeval_to_int64(const struct timeval* tv)
{
  return (int64_t)(((u_int64_t)(*tv).tv_sec)* 1000000ULL + ((u_int64_t)(*tv).tv_usec));
}

static int config_radio_interface(const char device[])
{
  int sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
  struct iwreq    wrq;
  memset(&wrq, 0, sizeof(wrq));
  strncpy(wrq.ifr_name, device, IFNAMSIZ);
  wrq.u.mode = IW_MODE_MONITOR;
  if (0 > ioctl(sd, SIOCSIWMODE, &wrq)) {
    perror("ioctl(SIOCSIWMODE) \n");
    syslog(LOG_ERR, "ioctl(SIOCSIWMODE): %s\n", strerror(errno));
    return 1;
  }
  return 0;
}

static int up_radio_interface(const char device[])
{
  int sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, device, IFNAMSIZ);
  if (-1 == ioctl(sd, SIOCGIFFLAGS, &ifr)) {
    perror("ioctl(SIOCGIFFLAGS)\n");
    syslog(LOG_ERR, "ioctl(SIOCGIFFLAGS): %s\n", strerror(errno));
    return 1;
  }
  const int flags = IFF_UP|IFF_RUNNING|IFF_PROMISC;
  if (ifr.ifr_flags  == flags)
    return 0;
  ifr.ifr_flags = flags;
  if (-1 == ioctl(sd, SIOCSIFFLAGS, &ifr)) {
    perror("ioctl(SIOCSIFFLAGS)\n");
    syslog(LOG_ERR, "ioctl(SIOCSIFFLAGS): %s\n", strerror(errno));
    return 1;
  }  
  return 0;
}
static int down_radio_interface(const char device[])
{
  int sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, device, IFNAMSIZ);
  if (-1 == ioctl(sd, SIOCGIFFLAGS, &ifr)) {
    perror("ioctl(SIOCGIFLAGS)\n");
    syslog(LOG_ERR, "ioctl(SIOCGIFFLAGS): %s\n", strerror(errno));
    return 1;
  }
  if (0 == ifr.ifr_flags)
    return 0;
  ifr.ifr_flags = 0;
  if (-1 == ioctl(sd, SIOCSIFFLAGS, &ifr)) {
    perror("ioctl(SIOCSIWMODE)\n");
    syslog(LOG_ERR, "ioctl(SIOCSIFFLAGS): %s\n", strerror(errno));
    return 1;
  }
  return 0;
}


static int open_infd(const char device[])
{
  int skbsz ;
  skbsz = 1U << 23 ; 
  int in_fd ;
  in_fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (in_fd < 0) {
    perror("socket(PF_PACKET)\n");
    return -1;
  }
  struct ifreq ifr;
  strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
  
  if (0 > ioctl(in_fd, SIOCGIFINDEX, &ifr)) {
    perror("ioctl(SIOGIFINDEX)\n");
    return -1;
  }
  //printf("the ifindex of device is %d\n",ifr.ifr_ifindex);
  struct sockaddr_ll sll;
  memset(&sll, 0, sizeof(sll));
  sll.sll_family  = AF_PACKET;
  sll.sll_ifindex = ifr.ifr_ifindex;
  sll.sll_protocol= htons(ETH_P_ALL);
  if (0 > bind(in_fd, (struct sockaddr *) &sll, sizeof(sll))) {
    perror("bind()\n");
    return -1;
  }
  if (0 > setsockopt(in_fd, SOL_SOCKET, SO_RCVBUF, &skbsz, sizeof(skbsz))) {
    perror("setsockopt(in_fd, SO_RCVBUF)\n");
    return -1;
  }
  int skbsz_l = sizeof(skbsz);
  if (0 > getsockopt(in_fd, SOL_SOCKET, SO_RCVBUF, &skbsz,
		     (socklen_t*)&skbsz_l)) {
    perror("getsockopt(in_fd, SO_RCVBUF)\n");
    return -1;
  }
  printf("size is %d %d \n ", skbsz_l, skbsz);
  int rcv_timeo = 600;
  struct timeval rto = { rcv_timeo, 0};
  if (rcv_timeo > 0 &&
      0 > setsockopt(in_fd, SOL_SOCKET, SO_RCVTIMEO, &rto, sizeof(rto))) {
    perror( "setsockopt(in_fd, SO_RCVTIMEO)\n");

    return -1;
  }
  return in_fd ;
}

int checkup(char * device){
  int in_fd ;
  if (down_radio_interface(device)){
    perror("down radio interface \n");
    exit(1);
  }

  if (up_radio_interface(device)){
    perror("up radio interface \n");
  }

  if (config_radio_interface(device)){
    perror("config radio intereface ");
  }
  in_fd = open_infd(device);

  return in_fd;
}

int static drops=0; 
int k_pkt_stats(int in_fd)
{
  struct tpacket_stats kstats;
  socklen_t sl = sizeof (struct tpacket_stats);
  if (0 != getsockopt(in_fd, SOL_PACKET, PACKET_STATISTICS, &kstats, &sl)) {
    perror("getsockopt(PACKET_STATISTICS)\n");
    return 0;
  }
  if (0 == kstats.tp_drops)
    return 1;
  if(kstats.tp_drops >0) {
	fprintf( stderr, "#drops =%d \n", kstats.tp_drops-drops );
   }
		drops= kstats.tp_drops ;
  struct timeval now; 
  struct timeval _tstamp;
  gettimeofday(&now, NULL);
  int delay = -1000;
  if (0 == ioctl(in_fd, SIOCGSTAMP, &_tstamp)) {
    delay = timeval_to_int64(&now) - timeval_to_int64(&_tstamp);
  } else {
    perror("ioctl(SIOCGTSTAMP)\n");
  }
  syslog( LOG_ERR,"last %d/%d blocks dropped, block delay is %d ms\n", kstats.tp_drops, kstats.tp_packets, delay);
  return 1;
}
