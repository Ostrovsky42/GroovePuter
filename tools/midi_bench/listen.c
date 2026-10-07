// Passive capture of everything a MIDI device sends, CLOCK_MONOTONIC ns from
// the first byte received. Use it while GroovePuter is the master (INTERNAL).
//
// Usage: listen /dev/snd/midiCxD0 SECONDS > log.tsv
// Pitfall: open the port BEFORE pressing Play. Bytes queued while nobody read
// the port (up to 16 USB packets) arrive at t=0, so a stale FA would shift the
// step grid by a whole pulse.
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
static int64_t nowns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (int64_t)t.tv_sec*1000000000LL+t.tv_nsec;}
int main(int argc,char**argv){
  if(argc<3){fprintf(stderr,"usage: listen /dev/snd/midiCxD0 SECONDS\n");return 2;}
  int fd=open(argv[1],O_RDONLY|O_NONBLOCK); if(fd<0){perror("open");return 1;}
  int64_t end=nowns()+(int64_t)(atof(argv[2])*1e9), t0=0;
  unsigned char buf[64],msg[3]; int status=0,need=0,have=0;
  while(nowns()<end){struct pollfd p={fd,POLLIN,0}; if(poll(&p,1,20)<=0)continue;
    int64_t at=nowns(); if(!t0)t0=at; int r=read(fd,buf,sizeof buf);
    for(int i=0;i<r;i++){unsigned char c=buf[i];
      if(c>=0xF8){printf("%lld\tRT %02X\n",(long long)(at-t0),c);continue;}
      if(c&0x80){status=c;have=0;msg[0]=c;need=((c&0xE0)==0xC0)?1:2;continue;}
      if(!status)continue;
      msg[1+have++]=c;
      if(have==need){printf("%lld\t%02X %d %d\n",(long long)(at-t0),msg[0],msg[1],need==2?msg[2]:0);have=0;}}}
  return 0;}
