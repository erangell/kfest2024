/* io/dns.c */
/* Non-blocking nameserver interface routines */

#include "externs.h"
#include <sys/socket.h>
#include <errno.h>

#ifndef __CYGWIN__  /* Cygwin-32 does not support lowlevel DNS queries */

/* Remove conflicts with some distributions' .h files */
#undef putshort
#undef putlong
#undef C_ANY

#include <arpa/nameser.h>
#include <resolv.h>

static struct resolve {
  struct resolve *next;
  struct resolve *nextquery;
  char *hostname;
  long expire;
  unsigned int ip;
  unsigned int id:16;
  unsigned int state:8;
} *resolves, *queries;

int dns_query;	/* Size of nameserver lookup queue */

#define STATE_FINISHED	0
#define STATE_FAILED	1

#define NUM_TRIES	3


/* open the nameserver for querying */
int dns_open()
{
  int s, on=1;


  /* open socket for nameserver communication */
  if((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("dns_open");
    return -1;
  } if(setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&on, sizeof(on)) < 0) {
    perror("dns_open: setsockopt");
    close(s);
    return -1;
  }

  return s;
}

/* send hostname lookup request to nameservers */
static void send_dns_request(struct resolve *rp)
{
}

void dns_receive()
{
}

/* called once per second */
void dns_events()
{
  struct resolve *rp, *next, *last=NULL;

  for(rp=queries;rp;rp=next) {
    next=rp->nextquery;

    /* remove any finished or failed queries from list */
    if(rp->state < 2) {
      dns_query--;
      if(last)
        last->next=next;
      else
        queries=next;
      continue;
    }

    /* query has not expired yet */
    if(now < rp->expire) {
      last=rp;
      continue;
    }

    if(rp->state < NUM_TRIES+1) {
      rp->state++;
      rp->expire=now+4;
      send_dns_request(rp);
      last=rp;
      continue;
    }

    /* exceeded maximum number of retries */

    log_resolv("Query for %s timed out.", ipaddr(rp->ip));
    rp->state=STATE_FAILED;
    rp->expire=now+7200;

    /* remove event from query list */
    dns_query--;
    if(last)
      last->next=next;
    else
      queries=next;
  }
}

/* Non-blocking gethostbyaddr(addr). Returns 0 if lookup is incomplete. */
char *dns_lookup(int ip)
{
  FILE *f;
  struct resolve *rp;
  char *r, *s, *addr=ipaddr(ip);
  static unsigned short lookup_id=0;
  static char buf[256];

  /* scan /etc/hosts for matching entry */
  if((f=fopen("/etc/hosts", "r"))) {
    while(ftext(buf, 256, f)) {
      if((s=strchr(buf, '#')))
        *s='\0';
      if(!isdigit(*buf))
        continue;

      /* match ip address */
      s=buf;
      r=strspc(&s);
      if(strcmp(r, addr))
        continue;
      if(!(r=strspc(&s)))
        continue;
      fclose(f);
      return r;
    }
    fclose(f);
  }

  /* if no nameserver present, just return address in dot notation */
  if(resolv == -1)
    return addr;

  /* search for a previously cached entry */
  for(rp=resolves;rp;rp=rp->next)
    if(rp->ip == ip)
      break;

  /* add a new record if not found */
  if(!rp) {
    rp=malloc(sizeof(struct resolve));
    rp->next=resolves;
    rp->state=0;
    rp->expire=0;
    rp->ip=ip;
    resolves=rp;
  }

  /* if the entry is new or has expired, send dns request */
  if(rp->state < 2 && rp->expire <= now) {
    rp->state=2;
    rp->expire=now+4;
    rp->id=++lookup_id;

    /* add entry to the list of queries */
    rp->nextquery=queries;
    queries=rp;
    dns_query++;

    send_dns_request(rp);
    return NULL;
  }

  if(rp->state == STATE_FINISHED)
    return rp->hostname;
  if(rp->state == STATE_FAILED)
    return addr;
  return NULL;
}

int dns_cache_size()
{
  struct resolve *rp;
  int num=0;
  
  for(rp=resolves;rp;rp=rp->next,num++);
  return num;
}

#else  /* __CYGWIN__ */

#include <netdb.h>

/* Under Cygwin-32, dns_lookup is mainly an alias for gethostbyaddr(). */
char *dns_lookup(int ip)
{
  struct hostent *hent;
  struct sockaddr_in addr;
  unsigned char *s;

  addr.sin_addr.s_addr=ip;
  hent = gethostbyaddr((char *)&addr.sin_addr.s_addr, sizeof(int), AF_INET);

  /* Check for unicode characters */
  if(hent)
    for(s=(char *)hent->h_name;*s >= ' ' && *s < 127;s++);

  return (hent && !*s)?(char *)hent->h_name:inet_ntoa(addr.sin_addr);
}

#endif
