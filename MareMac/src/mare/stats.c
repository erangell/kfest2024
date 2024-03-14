/* mare/stats.c */

#include "externs.h"
#include <sys/resource.h>

#if !defined(__CYGWIN__) && !defined(__FreeBSD__) && !defined(__NetBSD__)
#endif

#ifdef __CYGWIN__
extern int getdomainname(char *name, size_t len);
#endif

extern int dns_query;

/* determine hostname of local machine */
char *localhost()
{
  static char host[256];
  char domain[128], *r=domain, *s;

  gethostname(host, 127);
  getdomainname(domain, 127);

  /* remove leading symbols from domain name */
  while(*r == '.')
    r++;

  /* strip . from node name */
  if(!(s=strchr(host, '.')))
    s=strnul(host);
  if(*r && strcmp(r, "(none)")) {
    *s++='.';
    strcpy(s, r);
  }

  return host;
}

/* display version to player */
void do_version(dbref player)
{
  notify(player, "\e[1;32m%s Version Information:\e[m", MUD_NAME);
  notify(player, "\e[1;36m    Last Compile Date: \e[37m%s\e[m",
         mud_compile_date);
  notify(player, "\e[1;36m    Version Reference: \e[37m%s\e[m",mud_version);
  notify(player, "\e[1;36m    Running On System: \e[37m%s\e[m",system_version);
  notify(player, "\e[1;36m    Compiled By Using: \e[37mgcc %s\e[m",
         mud_compiler_version);
  notify(player, "\e[1;36m    DB Format Version: \e[37m%s\e[m",
         get_db_version());
  if(*TECH_EMAIL)
    notify(player, "\e[1;36m    Sys-Admin Contact: \e[37m%s\e[m", TECH_EMAIL);
}

/* display uptime stats of the mud */
void do_uptime(dbref player)
{
  long nextsave=DUMP_INTERVAL-dump_interval;

  notify(player, "\e[1;32m%s Runtime Stats:\e[m", MUD_NAME);
  notify(player, "\e[1;36m    Mare Boot Time\e[;36m.....\e[1m: \e[37m%s\e[m",
         mktm(player, mud_up_time));
  if(mud_up_time != last_reboot_time)
    notify(player, "\e[1;36m    Last Restarted\e[;36m.....\e[1m: \e[37m%s\e[m",
           mktm(player, last_reboot_time));
  notify(player, "\e[1;36m    Current Time\e[;36m.......\e[1m: \e[37m%s\e[m",
         mktm(player, now));
  if(db_readonly)
    notify(player, "\e[1;36m    Next Database Save\e[;36m.\e[1m: \e[37m"
           "None \e[;36m(read-only mode)\e[m");
  else
    notify(player, 
       "\e[1;36m    Next Database Save\e[;36m.\e[1m: \e[37m%s \e[;36m(%s)\e[m",
       mktm(player, now+nextsave), time_format(TMS, nextsave));
  notify(player, "\e[1;36m    In Operation For\e[;36m...\e[1m: \e[37m%s\e[m",
         time_format(TMA, now-mud_up_time));
}

// Displays a human-readable number of bytes.
//   type=1: Also print the word 'Bytes' in the output.
//
static char *unparse_bytes(quad num, int type)
{
  static char metrics[8]="KMGTPEZY";
  static char buf[64];

  quad size=num;
  char *s=buf;
  int a=0;

  while(size >= 1048576)
    a++, size/=1024;

  s+=sprintf(s, "%lld", num);
  if(type)
    s+=sprintf(s, " Byte%s", num==1?"":"s");

  if(num > 1023) {
    if(size < 10240)
      sprintf(s, " (%lld.%02lld %cB)", size/1024,size*100/1024%100,metrics[a]);
    else if(size < 102400)
      sprintf(s, " (%lld.%lld %cB)", size/1024, size*10/1024%10, metrics[a]);
    else
      sprintf(s, " (%lld %cB)", size/1024, metrics[a]);
  }

  return buf;
}

/* get detailed memory information on player */
void do_memory(dbref player, char *arg1)
{
  int a, b, thing, tot=0, playmem=0, totmem=0;

  if(!*arg1)
    thing=player;
  else if((thing=lookup_player(arg1)) == AMBIGUOUS)
    thing=db[player].owner;
  if(thing == NOTHING) {
    notify(player, "No such player '%s'.", arg1);
    return;
  } if(!controls(player, thing, POW_STATS)) {
    notify(player, "Permission denied.");
    return;
  }

  /* calculate memory usage */
  for(a=0;a<db_top;a++) {
    b=mem_usage(a);
    if(db[a].owner == db[thing].owner) {
      tot++;
      playmem += b;
    }
    totmem += b;
  }

  /* notify player */
  notify(player, "%s is using %s of memory.", db[thing].name,
         unparse_bytes(playmem, 1));
  notify(player, "%s is using %d Object%s out of the game's total, %d.",
         db[thing].name, tot, tot==1?"":"s", db_top);
  notify(player, 
         "%s is using approximately %d%% of Objects and %d%% of Memory on %s.",
         db[thing].name, (int)(tot*100LL/db_top), (int)(playmem*100LL/totmem),
         MUD_NAME);
}


/* database accounting information */

long db_start_time;
long db_total_uptime;
int db_dateinfo[4];

struct yearinfo {
  struct yearinfo *next;
  int year;
  int connects;
} *yearinfo=NULL;


void init_dbstat()
{
  if(!db_start_time)
    db_start_time=BEGINTIME;
}

#if 0
void update_accounting()
{
//long tt=time(NULL)+TIMEZONE*3600;
//int date=tt/86400;
}

void save_records(FILE *f, int rule)
{
  putchr(f, 1);    // Module version

  putlong(f, db_start_time);
  putlong(f, db_total_uptime);
}
#endif

/* display network and server statistics */
void do_info(dbref player, char *type)
{
#if !defined(__CYGWIN__) && !defined(__FreeBSD__) && !defined(__NetBSD__)
#endif

  int total, obj[8], pla[NUM_CLASSES];
  char nbuff[512], nbuf2[32];
  int a, b=0, c, d, e=0;
  
  /* If no argument, display dbsize and exit */
  if(!*type) {
    notify(player, "\e[1mThe universe of %s contains %d objects.\e[m",
           MUD_NAME, db_top);
    return;
  }

  calc_stats(AMBIGUOUS, &total, obj, pla);
  notify(player, "\e[1;32m%13.13s Network Stats                  "
         "\e[35m%-24.24s\e[m", MUD_NAME, mktm(player, now));
  strcpy(nbuff, "\304\304\304\304\304\304\304\304\304\304\304\304\304");
  a=strlen(MUD_NAME);
  if(a>13)
    a=13;
  nbuff[a]='\0';
  notify(player, "\e[1;34m%13.13s\304\304\304\304\304\304\304\304\304"
         "\304\304\304\304\304                  \e[35m%s\e[m", nbuff,
         wtime(-1, 0));
  notify(player, "%s", "");

  d=tolower(*type);

  if(d == 'd' || d == 'a') {
    notify(player, "\e[1;33mDatabase:\e[m");
    notify(player, "%s", "");
    if(power(player, POW_DB)) {
      for(b=0,a=0;a<db_top;a++)
        b+=mem_usage(a);
      notify(player, "  \e[1;36mDatabase Memory Usage:    \e[37m%s\e[m",
             unparse_bytes(b, 0));
    }
    if((c=fsize(db_file)) > 0) {
      notify(player, "  \e[1;36mDatabase File Size:       \e[37m%s\e[m",
             unparse_bytes(c, 0));
      for(a=0,total=0;a<db_top;a++)
        if(Typeof(a) != TYPE_GARBAGE)
          total++;
      notify(player, "  \e[1;36mAvg. Object Size:         \e[37m%s\e[m",
             unparse_bytes(c/total, 0));
    }
    notify(player, "  \e[1;36mNumber of DB Saves:       \e[37m%d\e[m",
           nstat[NS_DBSAVE]);

    notify(player, "%s", "");
    notify(player, "  \e[1;36mNumber of Attributes:     \e[37m%d\e[m",
           db_attrs);
    notify(player, "  \e[1;36mNumber of Object Lists:   \e[37m%d\e[m",
           db_lists);

    notify(player, "%s", "");
    a=-2; b=0;
    while(a < 8 || b < NUM_CLASSES) {
      /* build types column */
      if(a < 8) {
        sprintf(nbuf2, "%s%s:", (a == -2)?"Total":(a == -1)?"Active":
                typenames[a], (a >= 0 && a != TYPE_GARBAGE)?"s":"");
        sprintf(nbuff, "  \e[1;36m%-10.10s\e[37m%10d  \e[34m\263", nbuf2,
                (a == -2)?db_top:(a == -1)?total:obj[a]);
      } else
        sprintf(nbuff, "                        \e[1;34m\263");

      /* build classes column & print */
      if(b < NUM_CLASSES) {
        sprintf(nbuf2, "%ss:", classnames[b]);
        notify(player, "%s  \e[36m%-12.12s\e[37m%8d\e[m", nbuff, nbuf2,pla[b]);
      } else
        notify(player, "%s\e[m", nbuff);

      /* advance types to next slot */
      a++;
      while(a >= 0 && a < 8 && !obj[a])
        a++;
      b++;
    }

    notify(player, "%s", "");
    e=1;
  }

  if(d == 'i' || d == 'a') {
    notify(player, "\e[1;33mInput/Output:\e[m");
    notify(player, "%s", "");
    notify(player, "  \e[1;36mNetwork Packet Input:     \e[37m%s\e[m",
           unparse_bytes(nstat[NS_INPUT], 0));
    notify(player, "  \e[1;36mNetwork Packet Output:    \e[37m%s\e[m",
           unparse_bytes(nstat[NS_OUTPUT], 0));
    notify(player, "%s", "");
    notify(player, "  \e[1;36mNumber of Logins:         \e[37m%d\e[m",
           nstat[NS_LOGINS]);
    notify(player, "  \e[1;36mSuccessful Connects:      \e[37m%d\e[m",
           nstat[NS_CONNECTS]);
    notify(player, "  \e[1;36mFailed Connects:          \e[37m%d\e[m",
           nstat[NS_FAILED]);
    notify(player, "  \e[1;36mNew Users Created:        \e[37m%d\e[m",
           nstat[NS_NEWCR]);
    notify(player, "  \e[1;36mGuests Created:           \e[37m%d\e[m",
           nstat[NS_GUESTS]);
    notify(player, "%s", "");
    notify(player, "  \e[1;36mNameserver Queries:       \e[37m%d\e[m",
           nstat[NS_QUERY]);
    notify(player, "  \e[1;36mSuccessful DNS Lookups:   \e[37m%d\e[m",
           nstat[NS_RESOLV]);
#ifndef __CYGWIN__
    notify(player, "  \e[1;36mResolver Cache Size:      \e[37m%d\e[m",
           dns_cache_size());
    notify(player, "  \e[1;36mResolver Lookup Queue:    \e[37m%d\e[m",
           dns_query);
#endif
    notify(player, "%s", "");
    e=1;
  }

  if(d == 'm' || d == 'a') {
    c=mailfile_size();
    notify(player, "\e[1;33mMailfile Stats:\e[m");
    notify(player, "%s", "");
    if(c < 8)
      notify(player, "  \e[1;32mNone Present.\e[m");
    else {
      notify(player, "  \e[1;36mMailfile Size:            \e[37m%s\e[m",
             unparse_bytes(c, 0));
      notify(player, "  \e[1;36mMessages Sent:            \e[37m%d\e[m",
             nstat[NS_MAIL]);
      notify(player, "  \e[1;36mInternet Email Sent:      \e[37m%d\e[m",
             nstat[NS_EMAIL]);
      notify(player, "  \e[1;36mEmail Received:           \e[37m%d\e[m",
             nstat[NS_ERECV]);
      mail_count(player);
    }
    notify(player, "%s", "");
    e=1;
  }

  if(d == 'g' || d == 'a') {
    notify(player, "\e[1;33mGame Processes:\e[m");
    notify(player, "%s", "");
    if(ATIME_INTERVAL > 0)
      sprintf(nbuff, "#%d", atime_obj);
    else
      strcpy(nbuff, "\304\304\304");
    notify(player, "  \e[1;36mCurrent @atime Pos.:      \e[37m%-15s"
           "\e[36mQueue Process Calls:      \e[37m%d\e[m", nbuff,
           nstat[NS_QUEUE]);
    if(WANDER_INTERVAL > 0)
      sprintf(nbuff, "#%d", wander_obj);
    else
      strcpy(nbuff, "\304\304\304");
    notify(player, "  \e[1;36mCurrent Wander Pos.:      \e[37m%-15s"
           "\e[36mQueue Timer Overruns:     \e[37m%d\e[m", nbuff,
           nstat[NS_OVERRUNS]);
    notify(player, "  \e[1;36mNum. Wander Objs:         \e[37m%d\e[m",
           wander_count);
    notify(player, "                                           "
           "\e[1;36mImmediate Queue:          \e[37m%d\e[m",
           queue_size[Q_COMMAND]);
    notify(player, "  \e[1;36mDisconnected Rooms:       \e[37m%-15d"
           "\e[36mWaited Queue:             \e[37m%d\e[m",
           ndisrooms, queue_size[Q_WAIT]);

    notify(player, "  \e[1;36mExit Tree Depth:          \e[37m%d", treedepth);
    notify(player, "  \e[1;36mNext Free Object:         \e[37m#%d",
           get_upfront());
    notify(player, "%s", "");
    if(cmdfp[0] >= 999.95)
      sprintf(nbuff, "%-8d/%d", nstat[NS_NPCMDS], (int)cmdfp[0]);
    else
      sprintf(nbuff, "%-8d/%.1f", nstat[NS_NPCMDS], cmdfp[0]);
    for(a=0,b=0;a<db_top;a++)
      if(Typeof(a) == TYPE_PLAYER && inactive(a))
        b++;
    notify(player, "  \e[1;36mPlayer Commands/Sec:      \e[37m%-15.15s"
           "\e[36mTotal Inactive Players:   \e[37m%d\e[m", nbuff, b);
    if(cmdfp[1] >= 999.95)
      sprintf(nbuff, "%-8d/%d", nstat[NS_NQCMDS], (int)cmdfp[1]);
    else
      sprintf(nbuff, "%-8d/%.1f", nstat[NS_NQCMDS], cmdfp[1]);
    notify(player, "  \e[1;36mQueue Commands/Sec:       \e[37m%-15.15s"
           "\e[36mNumber of Failed Cmds:    \e[37m%d\e[m",
           nbuff, nstat[NS_NFCMDS]);
    if(cmdfp[0]+cmdfp[1] >= 999.95)
      sprintf(nbuff, "%-8d/%d", nstat[NS_NCMDS], (int)(cmdfp[0]+cmdfp[1]));
    else
      sprintf(nbuff, "%-8d/%.1f", nstat[NS_NCMDS], cmdfp[0]+cmdfp[1]);

    notify(player, "  \e[1;36mTotal Commands/Sec:       \e[37m%s\e[m", nbuff);
    notify(player, "%s", "");
    e=1;
  }

  if(d == 'c' || d == 'a') {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    notify(player, "\e[1;33mCpu Stats:\e[m");
    notify(player, "%s", "");
    a=getpid();
    b=getppid();
    if(!mare_watchdog || b < 2 || a == b)
      notify(player, "  \e[1;36mNetMARE Process Id:       \e[37m%d\e[m", a);
    else
      notify(player, "  \e[1;36mNetMARE Process Id:       \e[37m%-15d"
             "\e[36mWatchdog Process Id:      \e[37m%d\e[m", a, b);
    notify(player, "%s", "");

    /* Display CPU Usage if applicable */
    a=usage.ru_utime.tv_sec+usage.ru_stime.tv_sec;
    b=usage.ru_utime.tv_usec+usage.ru_stime.tv_usec;
    if(a+b) {
      a+=b/1000000;
      b/=10000;
      sprintf(nbuff, "%02d:%02d.%02d", a/60, a%60, b%100);
      if(!cpu_percent)
        strcpy(nbuf2, (now-last_reboot_time < 60)?"Startup":"Sleeping");
      else
        sprintf(nbuf2, "%.2f%%", (double)cpu_percent/1000);
      notify(player, "  \e[1;36mTotal CPU Time Used:      \e[37m%-15s"
             "\e[36mPercent CPU Usage:        \e[37m%s\e[m", nbuff, nbuf2);
    }

    notify(player, "  \e[1;36mNumber of Open Files:     \e[37m%-15d"
           "\e[36mRuntime Executions:       \e[37m%d\e[m",
           nstat[NS_FILES], nstat[NS_EXEC]);
    if((unsigned int)open_files >= 0x7fffffff)
      strcpy(nbuff, "Unlimited");
    else
      sprintf(nbuff, "%ld", open_files);
    notify(player, "  \e[1;36mMaximum Open Files:       \e[37m%-15s"
           "\e[36mRuntime Crashes:          \e[37m%d\e[m",
           nbuff, nstat[NS_CRASHES]);
    notify(player, "%s", "");

    /* Display resource usage counts for this process */
    if(usage.ru_msgrcv) {
      notify(player, tprintf("  \e[1;36mNetwork Msgs Received:    \e[37m%-15ld"
             "\e[36mHarddisk Reads:           \e[37m%ld\e[m",
             usage.ru_msgrcv, usage.ru_inblock));
      notify(player, tprintf("  \e[1;36mNetwork Msgs Sent:        \e[37m%-15ld"
             "\e[36mHarddisk Writes:          \e[37m%ld\e[m",
             usage.ru_msgsnd, usage.ru_oublock));
    }
    if(usage.ru_minflt) {
      notify(player, "  \e[1;36mMinor Page Faults:        \e[37m%-15ld"
             "\e[36mContext Switches:         \e[37m%ld\e[m",
             usage.ru_minflt, usage.ru_nvcsw);
      notify(player, "  \e[1;36mMajor Page Faults:        \e[37m%-15ld"
             "\e[36mForced Switches:          \e[37m%ld\e[m",
             usage.ru_majflt, usage.ru_nivcsw);
    }
    if(usage.ru_msgrcv || usage.ru_minflt)
      notify(player, "%s", "");

#if !defined(__CYGWIN__) && !defined(__FreeBSD__) && !defined(__NetBSD__)
#endif
    e=1;
  }

  if(!e)
    notify(player, "\e[1;31mPossible arguments: All Cpu Database "
           "Game I/O Mail\e[m");
  else
    notify(player, "\e[1;32mValues here are based on information since"
           " startup: \e[33m%s ago.\e[m", time_format(TMA, now-mud_up_time));
}


/* Available @list options */
static const struct {
  char *name;
  void (*func)();
  unsigned int flags:8;
  char perms;
} optlist[]={
  {"Attributes", list_attributes, 0, 0},
  {"Commands",   list_commands, 0, 0},
  {"Comlocks",   list_comlocks, 0, POW_COM},
  {"Config",     list_config, 0, 0},
  {"Flags",      list_flags, 0, 0},
  {"Functions",  list_functions, 0, 0},
  {"Players",    list_players, 0, 0},
  {"Powers",     list_powers, 2, 0},
  {"Sitelocks",  list_sitelocks, 0, POW_SITELOCK},
};

// Lists the contents of various in-memory database/server structures.
//
void do_list(dbref player, char *arg1, char *arg2)
{
  char buf[512], *s=buf;
  int a;

  int has_combat=0;
  if(*arg1) {
    for(a=0;a < NELEM(optlist);a++)
      if((!optlist[a].perms || power(player, optlist[a].perms)) &&
         (!(optlist[a].flags & 1) || has_combat) &&
         (!(optlist[a].flags & 2) || Immortal(player)) &&
         (string_prefix(optlist[a].name, arg1))) {
        optlist[a].func(player, arg2);
        return;
      }
    notify(player, "No such @list option '%s'.", arg1);
  } else
    notify(player, "Usage: @list <category>=[<argument>]");

  s+=sprintf(s, "Categories are:");
  for(a=0;a < NELEM(optlist);a++)
    if((!optlist[a].perms || power(player, optlist[a].perms)) &&
       (!(optlist[a].flags & 1) || has_combat) &&
       (!(optlist[a].flags & 2) || Immortal(player)))
      s+=sprintf(s, " %s", optlist[a].name);
  notify(player, "%s", buf);
}

/* Available @dbtop options */
static const struct {
  int val;
  char *name;
  int flags;  /* +1=Player-only, +2=Can be negative, +4=Time in Seconds */
} dbtop[]={
  { 1, "Age", 5},
  { 2, "Attrdefs", 0},
  { 3, "Attributes", 0},
  { 4, "Bytes", 0},
  { 5, "Children", 0},
  { 6, "Contents", 0},
  { 8, "Exits", 0},
  {12, "Inactivity", 5},
  {13, "Inzone", 0},
  {15, "Mail", 1},
  {16, "Memory", 1},
  {17, "Money", 1},
  {18, "Objects", 1},
  {19, "Parents", 0},
  {20, "Sessions", 1},
  {21, "Steps", 1},
  {26, "Zones", 0},
};

static void dbtop_internal(dbref player, int index, int range)
{
  struct {
    dbref obj;
    int value;
  } top[100];

  ALIST *attr;
  ATRDEF *k;
  char buf[128];
  dbref a, b, *ptr;
  int m, size=0, negative=(range < 0)?1:0;

  if(range < 0)
    range=-range;

  /* Display our category */
  notify(player, "** %s:", dbtop[index].name);

  /* Loop for all objects in the database */
  for(a=0;a < db_top;a++) {
    if((dbtop[index].flags & 1) && Typeof(a) != TYPE_PLAYER)
      continue;  /* This entry is players-only */

    m=0;
    switch(dbtop[index].val) {
    case 1: m=db[a].data->age; break;
    case 2: for(k=db[a].atrdefs;k;k=k->next,m++); break;
    case 3: for(attr=db[a].attrs;attr;attr=attr->next,m++); break;
    case 4: m=mem_usage(a); break;
    case 5: for(ptr=db[a].children;ptr && *ptr != NOTHING;ptr++,m++); break;
    case 6: for(b=db[a].contents;b!=NOTHING;b=db[b].next,m++); break;
    case 8: for(b=db[a].exits;b!=NOTHING;b=db[b].next,m++); break;
    case 12:
      if(!(db[a].flags & PLAYER_CONNECT)) {
        if((m=db[a].data->last) < db[a].data->lastoff)
          m=db[a].data->lastoff;
        if(!m)
          m=db[a].create_time;  /* Player has never connected */
        if(m)
          m=now-m;  /* Reference time has `number of seconds ago' */
      }
      break;
    case 13:
      if(Typeof(a) == TYPE_ZONE)
        for(ptr=db[a].zone;ptr && *ptr != NOTHING;ptr++,m++);
      break;
    case 15: m=mail_stats(a, 0); break;
    case 16: m=playmem_usage(a); break;
    case 17: m=db[a].pennies; break;
    case 18:
      for(b=0;b<db_top;b++)
        if(db[b].owner == a)
          m++;
      break;
    case 19: for(ptr=db[a].parents;ptr && *ptr != NOTHING;ptr++,m++); break;
    case 20: m=db[a].data->sessions; break;
    case 21: m=db[a].data->steps; break;
    case 26:
      if(Typeof(a) == TYPE_ROOM)
        for(ptr=db[a].zone;ptr && *ptr != NOTHING;ptr++,m++);
      break;
    }

    if(!m && !negative && !(dbtop[index].flags & 2))
      continue;  /* Don't list objects with a value of 0 */

    /* Find slot to insert object */
    if(negative)
      for(b=size;b > 0 && top[b-1].value > m;b--);
    else
      for(b=size;b > 0 && top[b-1].value < m;b--);
    if(size < range)
      size++;

    /* Insert object */
    if(b < size) {
      memmove(top+b+1, top+b, (size-b-1)*sizeof(*top));
      top[b].value=m;
      top[b].obj=a;
    }
  }

  if(!size) {
    notify(player, "Nothing of importance found in the database.");
    return;
  }

  /* Print results */
  for(m=0;m < size;m++) {
    if(dbtop[index].flags & 4) {
      strcpy(buf, time_format(TMA, top[m].value));
      notify(player, "%3d) %s has %s.", m+1,
             unparse_object(player, top[m].obj), buf);
    } else
      notify(player, "%3d) %s has %d %s", m+1,
             unparse_object(player, top[m].obj), top[m].value,
             dbtop[index].name);
  }
}

/* Database top command */
void do_dbtop(dbref player, char *arg1, char *arg2)
{
  char buf[512], *s=buf;
  int a, range=20;

  /* <arg2> overrides number of results to display: top-most or bottom-most */
  if(*arg2) {
    if(*arg2 == '-' && !*(arg2+1))
      range=-20;
    else
      range=atoi(arg2);
    if(!range || abs(range) > 100) {
      notify(player, "Range must be within 1 to 100.");
      return;
    }
  }

  if(*arg1) {
    for(a=0;a < NELEM(dbtop);a++)
      if(string_prefix(dbtop[a].name, arg1)) {
        dbtop_internal(player, a, range);
        return;
      }
    notify(player, "No such @dbtop option '%s'.", arg1);
  } else
    notify(player, "Usage: @dbtop <category>=[-][<range>]");

  s+=sprintf(s, "Categories are:");
  for(a=0;a < NELEM(dbtop);a++)
    s+=sprintf(s, " %s", dbtop[a].name);
  notify(player, "%s", buf);
}
