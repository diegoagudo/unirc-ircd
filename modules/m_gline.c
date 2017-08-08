/*
 *  ircd-hybrid: an advanced Internet Relay Chat Daemon(ircd).
 *  m_gline.c: Votes towards globally banning a mask.
 *
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id: m_gline.c 367 2006-04-21 19:21:06Z jon $
 */

#include "stdinc.h"
#include "tools.h"
#include "handlers.h"
#include "s_gline.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "hostmask.h"
#include "numeric.h"
#include "fdlist.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "s_misc.h"
#include "send.h"
#include "msg.h"
#include "fileio.h"
#include "s_serv.h"
#include "hash.h"
#include "parse.h"
#include "modules.h"
#include "list.h"
#include "s_log.h"

#define GLINE_NOT_PLACED     0
#ifdef GLINE_VOTING
#define GLINE_ALREADY_VOTED -1
#endif /* GLINE_VOTING */
#define GLINE_PLACED         1

extern dlink_list gdeny_items;

/* internal functions */
static void set_local_gline(const struct Client *, const char *, const char *, const char *);
static void find_user_gline(const struct Client*, const char *);

static void mo_gline(struct Client *, struct Client *, int, char **);
static void mo_ungline(struct Client *, struct Client *, int, char **);

/*
 * gline enforces 3 parameters to force operator to give a reason
 * a gline is not valid with "No reason"
 * -db
 */
struct Message gline_msgtab = {
	"GLINE", 0, 0, 3, 0, MFLG_SLOW, 0,
	{m_unregistered, m_not_oper, mo_gline, mo_gline, mo_gline, m_ignore}
};

struct Message ungline_msgtab = {
	"UNGLINE", 0, 0, 2, 0, MFLG_SLOW, 0,
	{m_unregistered, m_not_oper, mo_ungline, mo_ungline, mo_ungline, m_ignore}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
	mod_add_cmd(&gline_msgtab);
	mod_add_cmd(&ungline_msgtab);
	add_capability("GLN", CAP_GLN, 1);
}

void
_moddeinit(void)
{
	mod_del_cmd(&gline_msgtab);
	mod_del_cmd(&ungline_msgtab);
	delete_capability("GLN");
}

const char *_version = "$Revision: 367 $";
#endif

/* mo_gline()
 *
 * inputs       - The usual for a m_ function
 * output       -
 * side effects -
 *
 * Place a G line if 3 opers agree on the identical user@host
 * 
 */
/* Allow this server to pass along GLINE if received and
 * GLINES is not defined.
 *
 */
static int
_find_gline(const char *user, const char *host)
{
	struct AccessItem *aconf;
	dlink_node *ptr = NULL;
	struct irc_ssaddr addr, caddr;
	int nm_t, cnm_t, bits, cbits;

	nm_t = parse_netmask(host, &addr, &bits);

	DLINK_FOREACH(ptr, temporary_glines.head)
	{
		aconf = map_to_conf(ptr->data);
		cnm_t = parse_netmask(aconf->host, &caddr, &cbits);

		if(cnm_t != nm_t || irccmp(user, aconf->user))
			continue;

		if((nm_t == HM_HOST && !irccmp(aconf->host, host)) ||
		   (nm_t == HM_IPV4 && bits == cbits && match_ipv4(&addr, &caddr, bits))
#ifdef IPV6
		   || (nm_t == HM_IPV6 && bits == cbits && match_ipv6(&addr, &caddr, bits))
#endif
			)
		{
			//dlinkDelete(ptr, &temporary_glines);
			//delete_one_address_conf(aconf->host, aconf);
			return (1);
		}
	}

	return (0);
}


static void
mo_gline(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
    char *user = NULL;
    char *host = NULL;	/* user and host of GLINE "victim" */
    char *reason = NULL;	/* reason for "victims" demise     */
    char *p;

    if(!IsServices(source_p) && !IsServer(source_p))
      return;

    if(parse_aline("GLINE", source_p, parc, parv, AWILD, &user, &host, NULL, NULL, &reason) < 0)
        return;

    if((p = strchr(host, '/')) != NULL)
    {
        int bitlen = strtol(++p, NULL, 10);
        int min_bitlen = strchr(host, ':') ? ConfigFileEntry.gline_min_cidr6 :
            ConfigFileEntry.gline_min_cidr;
        if(bitlen < min_bitlen)
        {
            sendto_one(source_p,
                   ":%s NOTICE %s :Cannot set G-Lines with CIDR length < %d",
                   me.name, source_p->name, min_bitlen);
            return;
        }
    }

    if(_find_gline(user, host) == 1)
        return;

    set_local_gline(source_p, user, host, reason);

    /* 4 param version for hyb-7 servers */
    sendto_server(NULL, source_p, NULL, CAP_GLN | CAP_TS6, NOCAPS,
              LL_ICLIENT, ":%s GLINE %s %s :%s", ID(source_p), user, host, reason);
    sendto_server(NULL, source_p, NULL, CAP_GLN, CAP_TS6,
              LL_ICLIENT, ":%s GLINE %s %s :%s", source_p->name, user, host, reason);


    /* 4 param version for hyb-7 servers */
    sendto_server(client_p, source_p->servptr, NULL, CAP_GLN, NOCAPS, LL_ICLIENT,
              ":%s GLINE %s@%s :%s", source_p->name, user, host, reason);

    /* 8 param for hyb-6 */
    sendto_server(client_p, NULL, NULL, NOCAPS, CAP_GLN, NOFLAGS,
              ":%s GLINE %s %s %s %s %s@%s :%s",
              source_p->servptr->name,
              source_p->name, source_p->username, source_p->host,
              source_p->servptr->name, user, host, reason);



    /* 8 param for hyb-6 */
    sendto_server(NULL, NULL, NULL, CAP_TS6, CAP_GLN, NOFLAGS,
              ":%s GLINE %s %s %s %s %s %s :%s",
              ID(&me),
              ID(source_p), source_p->username,
              source_p->host, source_p->servptr->name, user, host, reason);
    sendto_server(NULL, NULL, NULL, NOCAPS, CAP_GLN | CAP_TS6, NOFLAGS,
              ":%s GLINE %s %s %s %s %s %s :%s",
              me.name, source_p->name, source_p->username,
              source_p->host, source_p->servptr->name, user, host, reason);
}


/* set_local_gline()
 *
 * inputs	- pointer to client struct of oper
 *		- pointer to victim user
 *		- pointer to victim host
 *		- pointer reason
 * output	- NONE
 * side effects	-
 */
static void
set_local_gline(const struct Client *source_p, const char *user,
		const char *host, const char *reason)
{
	char buffer[IRCD_BUFSIZE];
	struct ConfItem *conf;
	struct AccessItem *aconf;
	time_t cur_time;

	set_time();
	cur_time = CurrentTime;


	conf = make_conf_item(GLINE_TYPE);
	aconf = (struct AccessItem *) map_to_conf(conf);

	ircsprintf(buffer, "%s - Date: %s", reason, smalldate_pt(cur_time));
	DupString(aconf->reason, buffer);
	DupString(aconf->user, user);
	DupString(aconf->host, host);

	aconf->hold = CurrentTime + ConfigFileEntry.gline_time;
	add_temp_line(conf);

    find_user_gline(source_p, host);

	/*sendto_realops_flags(UMODE_ALL, L_ALL,
			     "%s adicionou AKILL para [%s@%s] [%s]",
			     source_p->name, aconf->user, aconf->host, aconf->reason);
   */
   
	ilog(L_TRACE, "%s adicionou AKILL para [%s@%s] [%s]",
	     get_oper_name(source_p), aconf->user, aconf->host, aconf->reason);
	log_oper_action(LOG_GLINE_TYPE, source_p, "[%s@%s] [%s]\n",
			aconf->user, aconf->host, aconf->reason);
	/* Now, activate gline against current online clients */
	rehashed_klines = 1;
}

// create by -- XOOM
// KILLED tds com o msm host!
// --------------------------------------------------------------------------
static void find_user_gline(const struct Client* client_p, const char *host)
{
 struct Client *target_p;
 dlink_node *gcptr;
 dlink_node *gcptr_next;

  DLINK_FOREACH_SAFE(gcptr, gcptr_next, global_client_list.head)
  {
    target_p = gcptr->data;

       if( irccmp(host, target_p->sockhost) == 0 )
       {

        if(IsServer(client_p))
                sendto_one(target_p, ":%s KILL %s :%s",
                       client_p->name, target_p->name, "You are banned from UnIRC Network.");
        else
            sendto_one(target_p, ":%s!%s@%s KILL %s :%s",
                   client_p->name, client_p->username, client_p->host,
                   target_p->name, "You are banned from UnIRC Network.");

       }
  }
}


static int
remove_gline_match(const char *user, const char *host)
{
	struct AccessItem *aconf;
	dlink_node *ptr = NULL;
	struct irc_ssaddr addr, caddr;
	int nm_t, cnm_t, bits, cbits;

	nm_t = parse_netmask(host, &addr, &bits);

	DLINK_FOREACH(ptr, temporary_glines.head)
	{
		aconf = map_to_conf(ptr->data);
		cnm_t = parse_netmask(aconf->host, &caddr, &cbits);

		if(cnm_t != nm_t || irccmp(user, aconf->user))
			continue;

		if((nm_t == HM_HOST && !irccmp(aconf->host, host)) ||
		   (nm_t == HM_IPV4 && bits == cbits && match_ipv4(&addr, &caddr, bits))
#ifdef IPV6
		   || (nm_t == HM_IPV6 && bits == cbits && match_ipv6(&addr, &caddr, bits))
#endif
			)
		{
			dlinkDelete(ptr, &temporary_glines);
			delete_one_address_conf(aconf->host, aconf);
			return (1);
		}
	}

	return (0);
}

/*
** m_ungline
** added May 29th 2000 by Toby Verrall <toot@melnet.co.uk>
** added to hybrid-7 7/11/2000 --is
**
**      parv[0] = sender nick
**      parv[1] = gline to remove
*/
static void
mo_ungline(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
    char *user, *host;

    if(!IsServices(source_p) && !IsServer(source_p))
      return;

    if(parse_aline("UNGLINE", source_p, parc, parv, 0, &user, &host, NULL, NULL, NULL) < 0)
        return;

    if(remove_gline_match(user, host))
    {

        sendto_server(client_p, source_p->servptr, NULL, CAP_GLN, NOCAPS, LL_ICLIENT,
                ":%s UNGLINE %s@%s", source_p->name, user, host);

        sendto_one(source_p, ":%s NOTICE %s :G-LINE de [%s@%s] foi removido",
               me.name, source_p->name, user, host);

        /*sendto_realops_flags(UMODE_ALL, L_ALL,
                     "%s removeu G-LINE de: [%s@%s]",
                     source_p->name, user, host);
          */
        ilog(L_NOTICE, "%s removed G-Line for [%s@%s]",
             get_oper_name(source_p), user, host);
    }
    /*else
    {
        sendto_one(source_p, ":%s NOTICE %s :No G-Line for %s@%s",
               me.name, source_p->name, user, host);
    }*/
}



