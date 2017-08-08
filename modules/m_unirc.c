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
 *  $Id: unirc.c 367 2007-08-27 23:21:06Z XOOM $
 *
 * 2009-01-31 adicionado IPBR
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
#include "unirc.h"



static void m_proxy(struct Client *, struct Client *, int, char **);
static void m_unproxy(struct Client *, struct Client *, int, char **);

static void m_ipbr(struct Client *, struct Client *, int, char **);
static void m_unipbr(struct Client *, struct Client *, int, char **);

static void m_zombie(struct Client *, struct Client *, int, char **);
static void m_unzombie(struct Client *, struct Client *, int, char **);

static void m_floodex(struct Client *, struct Client *, int, char **);
static void m_unfloodex(struct Client *, struct Client *, int, char **);

static void m_spamsg(struct Client *, struct Client *, int, char **);
static void m_unspamsg(struct Client *, struct Client *, int, char **);

static void m_svsinfo(struct Client *, struct Client *, int, char **);

static void m_vlink(struct Client *, struct Client *, int, char **);
static void m_unvlink(struct Client *, struct Client *, int, char **);
static void m_setvlink(struct Client *, struct Client *, int, char **);



struct Message proxy_msgtab = {
    "PROXY", 0, 0, 1, 0, MFLG_SLOW, 0,
    {m_unregistered, m_not_oper, m_proxy, m_proxy, m_proxy, m_ignore}
};
struct Message unproxy_msgtab = {
    "UNPROXY", 0, 0, 1, 0, MFLG_SLOW, 0,
    {m_unregistered, m_not_oper, m_unproxy, m_unproxy, m_unproxy, m_ignore}
};

struct Message vlink_msgtab = {
    "VLINK", 0, 0, 1, 0, MFLG_SLOW, 0,
    {m_unregistered, m_not_oper, m_vlink, m_vlink, m_vlink, m_ignore}
};
struct Message unvlink_msgtab = {
    "UNVLINK", 0, 0, 1, 0, MFLG_SLOW, 0,
    {m_unregistered, m_not_oper, m_unvlink, m_unvlink, m_unvlink, m_ignore}
};
struct Message setvlink_msgtab = {
    "SETVLINK", 0, 0, 3, 0, MFLG_SLOW, 0,
    {m_ignore, m_ignore, m_setvlink, m_ignore, m_ignore, m_ignore}
};

struct Message ipbr_msgtab = {
    "IPBR", 0, 0, 1, 0, MFLG_SLOW, 0,
    {m_unregistered, m_not_oper, m_ipbr, m_ipbr, m_ipbr, m_ignore}
};
struct Message unipbr_msgtab = {
    "UNIPBR", 0, 0, 1, 0, MFLG_SLOW, 0,
    {m_unregistered, m_not_oper, m_unipbr, m_unipbr, m_unipbr, m_ignore}
};

struct Message zombie_msgtab = {
    "ZOMBIE", 0, 0, 1, 0, MFLG_SLOW, 0,
    {m_unregistered, m_not_oper, m_zombie, m_zombie, m_zombie, m_ignore}
};
struct Message unzombie_msgtab = {
    "UNZOMBIE", 0, 0, 1, 0, MFLG_SLOW, 0,
    {m_unregistered, m_not_oper, m_unzombie, m_unzombie, m_unzombie, m_ignore}
};


struct Message floodex_msgtab = {
    "FLOODEX", 0, 0, 1, 0, MFLG_SLOW, 0,
    {m_unregistered, m_not_oper, m_floodex, m_floodex, m_floodex, m_ignore}
};
struct Message unfloodex_msgtab = {
    "UNFLOODEX", 0, 0, 1, 0, MFLG_SLOW, 0,
    {m_unregistered, m_not_oper, m_unfloodex, m_unfloodex, m_unfloodex, m_ignore}
};



struct Message spamsg_msgtab = {
    "SPAMSG", 0, 0, 1, 0, MFLG_SLOW, 0,
    {m_unregistered, m_not_oper, m_spamsg, m_spamsg, m_spamsg, m_ignore}
};
struct Message unspamsg_msgtab = {
    "UNSPAMSG", 0, 0, 1, 0, MFLG_SLOW, 0,
    {m_unregistered, m_not_oper, m_unspamsg, m_unspamsg, m_unspamsg, m_ignore}
};

struct Message svsinfo_msgtab = {
    "SVSINFO", 0, 0, 1, 0, MFLG_SLOW, 0,
    {m_unregistered, m_not_oper, m_svsinfo, m_svsinfo, m_svsinfo, m_ignore}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
	mod_add_cmd(&proxy_msgtab);
	mod_add_cmd(&unproxy_msgtab);
	add_capability("PXY", CAP_PXY, 1);
	mod_add_cmd(&ipbr_msgtab);
	mod_add_cmd(&unipbr_msgtab);
	add_capability("IPBR", CAP_IPBR, 1);
	mod_add_cmd(&vlink_msgtab);
	mod_add_cmd(&unvlink_msgtab);
	mod_add_cmd(&setvlink_msgtab);
	add_capability("VLINK", CAP_VLINK, 1);
	mod_add_cmd(&zombie_msgtab);
	mod_add_cmd(&unzombie_msgtab);
	add_capability("ZOMBIE", CAP_ZOMBIE, 1);
	mod_add_cmd(&floodex_msgtab);
	mod_add_cmd(&unfloodex_msgtab);
	add_capability("FLOODEX", CAP_FLOODEX, 1);
	mod_add_cmd(&spamsg_msgtab);
	mod_add_cmd(&unspamsg_msgtab);
	add_capability("SPM", CAP_SPM, 1);
	mod_add_cmd(&svsinfo_msgtab);
	add_capability("SVSINFO", CAP_SVSINFO, 1);
}

void
_moddeinit(void)
{
	mod_del_cmd(&proxy_msgtab);
	mod_del_cmd(&unproxy_msgtab);
	delete_capability("PXY");
	mod_del_cmd(&ipbr_msgtab);
	mod_del_cmd(&unipbr_msgtab);
	delete_capability("IPBR");
	mod_del_cmd(&vlink_msgtab);
	mod_del_cmd(&unvlink_msgtab);
	mod_del_cmd(&setvlink_msgtab);
	delete_capability("VLINK");
	mod_del_cmd(&zombie_msgtab);
	mod_del_cmd(&unzombie_msgtab);
	delete_capability("ZOMBIE");
	mod_del_cmd(&floodex_msgtab);
	mod_del_cmd(&unfloodex_msgtab);
	delete_capability("FLOODEX");
	mod_del_cmd(&spamsg_msgtab);
	mod_del_cmd(&unspamsg_msgtab);
	delete_capability("SPM");
	mod_del_cmd(&svsinfo_msgtab);
	delete_capability("SVSINFO");
}

const char *_version = "$Revision: 367 $";
#endif



static void m_proxy(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
   PROXY(client_p, source_p, parc, parv);
}
static void m_unproxy(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
   UNPROXY(client_p, source_p, parc, parv);
}

static void m_ipbr(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
   IPBR(client_p, source_p, parc, parv);
}
static void m_unipbr(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
   UNIPBR(client_p, source_p, parc, parv);
}

static void m_zombie(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
   ZOMBIE(client_p, source_p, parc, parv);
}
static void m_unzombie(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
   UNZOMBIE(client_p, source_p, parc, parv);
}

static void m_floodex(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
   FLOODEX(client_p, source_p, parc, parv);
}
static void m_unfloodex(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
   UNFLOODEX(client_p, source_p, parc, parv);
}


static void m_spamsg(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
   SPAMSG(client_p, source_p, parc, parv);
}
static void m_unspamsg(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
   UNSPAMSG(client_p, source_p, parc, parv);
}


static void m_svsinfo(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
   SVSINFO(client_p, source_p, parc, parv);
}

static void m_vlink(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
   VLINK(client_p, source_p, parc, parv);
}

static void m_unvlink(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
   UNVLINK(client_p, source_p, parc, parv);
}

static void m_setvlink(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
   SETVLINK(client_p, source_p, parc, parv);
}


