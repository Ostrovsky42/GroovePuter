// Exact external MIDI Clock master for GroovePuter's MIDI IN follow.
// Sends F8 at the requested BPM (optionally a pre-roll before Start, as SEQTRAK
// does), FA immediately before the downbeat F8, FC at the end, and logs every
// byte the device sends back with CLOCK_MONOTONIC timestamps (ns from the
// downbeat). Talks to the ALSA raw MIDI device file directly: no libraries.
//
// Usage: clockmaster /dev/snd/midiCxD0 BPM SECONDS [PREROLL_SECONDS] > log.tsv
// Output: "TX\t<ns>\tF8 <pulse>" / "TX\t<ns>\tFA|FC" / "RX\t<ns>\t<status> <d1> <d2>"
//         / "RX\t<ns>\tRT <byte>". Pulse 0 is the downbeat after FA.
#define _GNU_SOURCE
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
static int64_t nowns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (int64_t)t.tv_sec*1000000000LL+t.tv_nsec;}
int main(int argc,char**argv){
  if(argc<4){fprintf(stderr,"usage: clockmaster /dev/snd/midiCxD0 BPM SECONDS [PREROLL_SECONDS]\n");return 2;}
  int fd=open(argv[1],O_RDWR|O_NONBLOCK); if(fd<0){perror("open");return 1;}
  double bpm=atof(argv[2]); double secs=atof(argv[3]); double preroll=argc>4?atof(argv[4]):0;
  int64_t period=(int64_t)(60e9/(bpm*24.0));
  unsigned char b; while(read(fd,&b,1)==1){} // drain
  unsigned char stop=0xFC; write(fd,&stop,1); usleep(300000);
  int64_t t0=nowns()+200000000LL+(int64_t)(preroll*1e9); // pulse 0 (downbeat)
  long pulse=-(long)(preroll*bpm*24.0/60.0); int faSent=0; long total=(long)(secs*bpm*24.0/60.0);
  unsigned char buf[64]; int status=0,need=0,have=0; unsigned char msg[3];
  while(pulse<=total){
    int64_t due=t0+pulse*period;
    for(;;){
      int64_t n=nowns(); if(n>=due) break;
      int64_t wait=(due-n)/1000000; struct pollfd p={fd,POLLIN,0};
      if(poll(&p,1,wait>1?(int)(wait-1):0)>0){
        int64_t at=nowns()-t0; int r=read(fd,buf,sizeof buf);
        for(int i=0;i<r;i++){unsigned char c=buf[i];
          if(c>=0xF8){printf("RX\t%lld\tRT %02X\n",(long long)at,c);continue;}
          if(c&0x80){status=c;have=0;msg[0]=c;need=((c&0xE0)==0xC0)?1:2;continue;}
          if(!status)continue; msg[1+have++]=c;
          if(have==need){ if(need==2) printf("RX\t%lld\t%02X %d %d\n",(long long)at,msg[0],msg[1],msg[2]); have=0; }
        }
      } else if (wait<=1) { while(nowns()<due){} }
    }
    if(!faSent && pulse>=0){ unsigned char fa=0xFA; write(fd,&fa,1); printf("TX\t%lld\tFA\n",(long long)(nowns()-t0)); faSent=1; }
    unsigned char f8=0xF8; write(fd,&f8,1);
    printf("TX\t%lld\tF8 %ld\n",(long long)(nowns()-t0),pulse); pulse++;
  }
  write(fd,&stop,1); printf("TX\t%lld\tFC\n",(long long)(nowns()-t0));
  int64_t end=nowns()+500000000LL;
  while(nowns()<end){struct pollfd p={fd,POLLIN,0}; if(poll(&p,1,50)>0){int64_t at=nowns()-t0;int r=read(fd,buf,sizeof buf);
    for(int i=0;i<r;i++){unsigned char c=buf[i]; if(c>=0xF8)continue; if(c&0x80){status=c;have=0;msg[0]=c;need=((c&0xE0)==0xC0)?1:2;continue;}
      if(!status)continue; msg[1+have++]=c; if(have==need){ if(need==2) printf("RX\t%lld\t%02X %d %d\n",(long long)at,msg[0],msg[1],msg[2]); have=0; }}}}
  return 0;
}
