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
 * 2009-04-28 adicionado VLINK / UNVLINK / SETVIRTUAL
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

/*
 *STORAGE
 */
ProxyList  *proxys   = (ProxyList *)NULL;
IPBRList   *ipbrs    = (IPBRList *)NULL;
SpamsgList *spamsgs  = (SpamsgList *)NULL;
VLINKList  *vlinks   = (VLINKList *)NULL;

int MaxVlink = 0;

/*
 *
 * VLINK 
 *
 */

// verifica existencia de vlinks
struct struct_vlink* find_vlink_nome(const char *server_filho)
{
       VLINKList *aconf = vlinks;

        while(aconf && !match(aconf->server_filho, server_filho))
          aconf = aconf->next;

        return aconf; 
}

struct struct_vlink* find_vlink(const char *endereco)
{
       VLINKList *aconf = vlinks;

        while(aconf && !match(aconf->endereco, endereco))
          aconf = aconf->next;

        return aconf; 
}

static void get_desc(int parc, char *parv[], char *buf)
{
    int ii = 0;
    int bw = 0;

    for(; ii < parc; ++ii)
        bw += ircsprintf(buf + bw, "%s ", parv[ii]);
    buf[bw - 1] = '\0';
}

// add um novo VLINK
void VLINK(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
    VLINKList *aconf;
    char descricao[IRCD_BUFSIZE];

    if(!IsServices(source_p) && !IsServer(source_p))
      return;

    if ( parc < 5 )
    {
       sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "VLINK");
     return;
    }

    get_desc(parc - 4, parv + 4, descricao);

    if (!find_vlink(parv[1]))
    {
       // alocando memoria da baga�a! :X
       aconf = (VLINKList*) MyMalloc(sizeof(VLINKList));
       memset(aconf, 0, sizeof(VLINKList));

       DupString(aconf->endereco,       parv[1]);
       DupString(aconf->server_pai,     parv[2]);
       DupString(aconf->server_filho,   parv[3]);
       DupString(aconf->descricao,      descricao);
        
       aconf->next = vlinks;
       vlinks = aconf;

       //sendto_one(source_p, "NOTICE %s :Pai: %s Filho: %s End: %s - Desc: %s", source_p->name, parv[1], parv[2], parv[3], descricao);
       MaxVlink++;
       sendto_server(client_p, source_p->servptr, NULL, CAP_VLINK, NOCAPS, LL_ICLIENT, ":%s VLINK %s %s %s :%s", source_p->name, parv[1], parv[2], parv[3], descricao);
    }
}



// remove um VLINK
void UNVLINK(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
    VLINKList *aconf = vlinks;
    VLINKList *last_aconf;

    if(!IsServices(source_p) && !IsServer(source_p))
      return;

    if ( parc < 1 )
    {
       sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "UNVLINK");
     return;
    }

     last_aconf = NULL;

     while(aconf && irccmp(aconf->endereco, parv[1]))
     {
       last_aconf = aconf;
       aconf = aconf->next;
     }
     
     if (aconf) /* if akill does exist */
     {
        if(last_aconf) /* this is not the first vlink */
          last_aconf->next=aconf->next; /* adjust list link -> */
        else
          vlinks = aconf->next;

         /* removendo...(liberando memoria) */
          MyFree(aconf->server_pai);
          MyFree(aconf->server_filho);
          MyFree(aconf->endereco);
          MyFree(aconf->descricao);
          MyFree(aconf);
          MaxVlink--;
          sendto_server(client_p, source_p->servptr, NULL, CAP_VLINK, NOCAPS, LL_ICLIENT, ":%s UNVLINK %s", source_p->name, parv[1]);
     } 
}


// VLINKS NO M_LINKS
void VLINKLINKS(struct Client *source_p)
{
    VLINKList *temp = vlinks;
    const char *me_name, *nick;
    me_name = ID_or_name(&me, source_p->from);
    nick    = ID_or_name(source_p, source_p->from);

    while (temp)
    {
        if(match(temp->server_pai, "irc.unirc.net"))
            sendto_one(source_p, form_str(RPL_LINKS), me_name, nick, temp->server_filho, temp->server_pai, 1, temp->descricao);
        else
            sendto_one(source_p, form_str(RPL_LINKS), me_name, nick, temp->server_filho, temp->server_pai, 2, temp->descricao);
        temp = temp->next;
    }
}


// lista os VLINKs
void VLINKLIST(struct Client *source_p)
{
  VLINKList *temp = vlinks;
  
  sendto_one(source_p, "NOTICE %s :\2*** LISTA DE VIRTUAL SERVERS ***\2", source_p->name);
  while (temp)
  {
    sendto_one(source_p, "NOTICE %s :Pai: %s Filho: %s End: %s Desc: %s", source_p->name, temp->server_pai, temp->server_filho, temp->endereco, temp->descricao);
    temp = temp->next;
  }
}

// server_estab() enviando vlinks
void SEND_VLINK(struct Client *client_p)
{
    VLINKList *temp = vlinks;

    while(temp)
    {
       sendto_one(client_p, "VLINK %s %s %s :%s", temp->endereco, temp->server_pai, temp->server_filho, temp->descricao);
       temp = temp->next;
    }
}


// services_estab() removendo vlinks
void CLEAR_VLINKS()
{

    VLINKList *aconf;
    VLINKList *temp = vlinks;

       while(temp)
       {
           aconf = temp->next;
           MyFree(temp);
           temp = aconf;
       }

       vlinks = NULL;
       MaxVlink = 0;
}


char *VlinkGetDesc(char *server)
{
        VLINKList *temp = vlinks;

        while(temp && !match(temp->server_filho, server))
          temp = temp->next;

        if (temp && temp->descricao) 
            return temp->descricao;
        else
            return "";
}

//SETVLINK
void SETVLINK(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
    struct Client *target_p;

    if(!IsServices(source_p) && !IsServer(source_p))
      return;

    if(parc < 3)
        return;

    if((target_p = find_person(client_p, parv[1]))) {
        if(!target_p->vlink_nome) {
            DupString(target_p->vlink_nome, parv[2]);
            DupString(target_p->vlink_desc, parv[3]);
            
            sendto_one(client_p, "SETVLINK %s %s :%s", parv[1], parv[2], parv[3]);
        }
    }
}

/*
 *
 * PROXY 
 *
 */

// verifica existencia de proxys
struct struct_proxy* find_proxy(const char *host)
{
       ProxyList *pxy = proxys;

        while(pxy && !match(pxy->host, host))
          pxy = pxy->next;

        return pxy; 
}

// add um novo PROXY
void PROXY(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
    ProxyList *aconf;

    if(!IsServices(source_p) && !IsServer(source_p))
      return;

    if ( parc < 1 )
    {
       sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "PROXY");
     return;
    }

    if (!find_proxy(parv[1]))
    {
       // alocando memoria da baga�a! :X
       aconf = (ProxyList*) MyMalloc(sizeof(ProxyList));
       memset(aconf, 0, sizeof(ProxyList));

       DupString(aconf->host, parv[1]);

       aconf->next = proxys;
       proxys = aconf;

       sendto_server(client_p, source_p->servptr, NULL, CAP_PXY, NOCAPS, LL_ICLIENT,
                  ":%s PROXY %s", source_p->name, parv[1]);
    }
}

// remove um PROXY
void UNPROXY(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
    ProxyList *pxy = proxys;
    ProxyList *last_pxy;

    if(!IsServices(source_p) && !IsServer(source_p))
      return;

    if ( parc < 1 )
    {
       sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "PROXY");
     return;
    }

     last_pxy = NULL;

     while(pxy && irccmp(pxy->host, parv[1]))
     {
       last_pxy = pxy;
       pxy = pxy->next;
     }
     
     if (pxy) /* if akill does exist */
     {
        if(last_pxy) /* this is not the first proxy */
          last_pxy->next=pxy->next; /* adjust list link -> */
        else
          proxys = pxy->next;

         /* removendo...(liberando memoria) */
          MyFree(pxy->host);
          MyFree(pxy);

          sendto_server(client_p, source_p->servptr, NULL, CAP_PXY, NOCAPS, LL_ICLIENT,
                ":%s UNPROXY %s", source_p->name, parv[1]);
     } 
}

// lista as PROXYs
void PROXYLIST(struct Client *source_p)
{
  ProxyList *temp = proxys;
  
  sendto_one(source_p, "NOTICE %s :\2*** LISTA DE PROXYS ***\2", source_p->name);
  while (temp)
  {
    sendto_one(source_p, "NOTICE %s :HOST: \2%s\2", source_p->name, temp->host);
    temp = temp->next;
  }
}

// server_estab() enviando proxys
void SEND_PROXYS(struct Client *client_p)
{
    ProxyList *temp = proxys;

    while(temp)
    {
       sendto_one(client_p, "PROXY %s", temp->host);
       temp = temp->next;
    }
}


// services_estab() removendo proxys
void CLEAR_PROXYS()
{

    ProxyList *aconf;
    ProxyList *temp = proxys;

       while(temp)
       {
           aconf = temp->next;
           MyFree(temp);
           temp = aconf;
       }

       proxys = NULL;
}





/*
 *
 * IPBR 
 *
 */

// verifica existencia de ipbrs
struct struct_ipbr* find_ipbr(const char *host)
{
       IPBRList *aconf = ipbrs;

        while(aconf && !match(aconf->host, host))
          aconf = aconf->next;

        return aconf; 
}

// add um novo IPBR
void IPBR(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
    IPBRList *aconf;

    if(!IsServices(source_p) && !IsServer(source_p))
      return;

    if ( parc < 1 )
    {
       sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "IPBR");
     return;
    }

    if (!find_ipbr(parv[1]))
    {
       // alocando memoria da baga�a! :X
       aconf = (IPBRList*) MyMalloc(sizeof(IPBRList));
       memset(aconf, 0, sizeof(IPBRList));

       DupString(aconf->host, parv[1]);

       aconf->next = ipbrs;
       ipbrs = aconf;

       sendto_server(client_p, source_p->servptr, NULL, CAP_IPBR, NOCAPS, LL_ICLIENT,
                  ":%s IPBR %s", source_p->name, parv[1]);
    }
}

// remove um IPBR
void UNIPBR(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
    IPBRList *aconf = ipbrs;
    IPBRList *last_aconf;

    if(!IsServices(source_p) && !IsServer(source_p))
      return;

    if ( parc < 1 )
    {
       sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "IPBR");
     return;
    }

     last_aconf = NULL;

     while(aconf && irccmp(aconf->host, parv[1]))
     {
       last_aconf = aconf;
       aconf = aconf->next;
     }
     
     if (aconf) /* if akill does exist */
     {
        if(last_aconf) /* this is not the first ipbr */
          last_aconf->next=aconf->next; /* adjust list link -> */
        else
          ipbrs = aconf->next;

         /* removendo...(liberando memoria) */
          MyFree(aconf->host);
          MyFree(aconf);

          sendto_server(client_p, source_p->servptr, NULL, CAP_IPBR, NOCAPS, LL_ICLIENT,
                ":%s UNIPBR %s", source_p->name, parv[1]);
     } 
}

// lista as IPBRs
void IPBRLIST(struct Client *source_p)
{
  IPBRList *temp = ipbrs;
  
  sendto_one(source_p, "NOTICE %s :\2*** LISTA DE IPs Brasileiros ***\2", source_p->name);
  while (temp)
  {
    sendto_one(source_p, "NOTICE %s :HOST: \2%s\2", source_p->name, temp->host);
    temp = temp->next;
  }
}

// server_estab() enviando ipbrs
void SEND_IPBRS(struct Client *client_p)
{
    IPBRList *temp = ipbrs;

    while(temp)
    {
       sendto_one(client_p, "IPBR %s", temp->host);
       temp = temp->next;
    }
}


// services_estab() removendo ipbrs
void CLEAR_IPBRS()
{

    IPBRList *aconf;
    IPBRList *temp = ipbrs;

       while(temp)
       {
           aconf = temp->next;
           MyFree(temp);
           temp = aconf;
       }

       ipbrs = NULL;
}


/*
 *
 * SPAMSG 
 *
 */

// verifica existencia de spamsgs
struct struct_spamsg* find_spamsg(const char *msg)
{
       SpamsgList *spm = spamsgs;

        while(spm && !match(spm->msg, msg))
          spm = spm->next;

        return spm; 
}

// add um novo SPAMSG
void SPAMSG(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
    SpamsgList *aconf;

    if(!IsServices(source_p) && !IsServer(source_p))
      return;

    if ( parc < 1 )
    {
       sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "SPAMSG");
     return;
    }

    if (!find_spamsg(parv[1]))
    {
       // alocando memoria da baga�a! :X
       aconf = (SpamsgList*) MyMalloc(sizeof(SpamsgList));
       memset(aconf, 0, sizeof(SpamsgList));

       DupString(aconf->msg, parv[1]);

       aconf->next = spamsgs;
       spamsgs = aconf;

       sendto_server(client_p, source_p->servptr, NULL, CAP_SPM, NOCAPS, LL_ICLIENT,
                  ":%s SPAMSG %s", source_p->name, parv[1]);
    }
}

// remove um SPAMSG
void UNSPAMSG(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
    SpamsgList *spm = spamsgs;
    SpamsgList *last_spm;

    if(!IsServices(source_p) && !IsServer(source_p))
      return;

    if ( parc < 1 )
    {
       sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "SPAMSG");
     return;
    }

     last_spm = NULL;

     while(spm && irccmp(spm->msg, parv[1]))
     {
       last_spm = spm;
       spm = spm->next;
     }
     
     if (spm) /* if akill does exist */
     {
        if(last_spm) /* this is not the first spamsg */
          last_spm->next=spm->next; /* adjust list link -> */
        else
          spamsgs = spm->next;

         /* removendo...(liberando memoria) */
          MyFree(spm->msg);
          MyFree(spm);

          sendto_server(client_p, source_p->servptr, NULL, CAP_SPM, NOCAPS, LL_ICLIENT,
                ":%s UNSPAMSG %s", source_p->name, parv[1]);
     }
}

// lista as SPAMSGs
void SPAMSGLIST(struct Client *source_p)
{
  SpamsgList *temp = spamsgs;
  
  sendto_one(source_p, "NOTICE %s :\2*** LISTA DE SPAMSGS ***\2", source_p->name);
  while (temp)
  {
    sendto_one(source_p, "NOTICE %s :SPAMSG: \2%s\2", source_p->name, temp->msg);
    temp = temp->next;
  }
}

// server_estab() enviando spamsgs
void SEND_SPAMSGS(struct Client *client_p)
{
    SpamsgList *temp = spamsgs;

    while(temp)
    {
       sendto_one(client_p, "SPAMSG %s", temp->msg);
       temp = temp->next;
    }
}


// services_estab() removendo spamsgs
void CLEAR_SPAMSGS()
{

    SpamsgList *aconf;
    SpamsgList *temp = spamsgs;

       while(temp)
       {
           aconf = temp->next;
           MyFree(temp);
           temp = aconf;
       }

       spamsgs = NULL;
}


int verifica_spamsg(char *text)
{
    SpamsgList *temp = spamsgs;
    static char auxbuf[512];
    char *a = auxbuf;
    char *t = text;

    /* lets first remove codes and set it up case --fabulous */
    while (*t) {
        switch (*t) {
            case 2: //bold
                t++;
                continue;
            break;
            case 3: //mIRC colors
                if (IsDigit(t[1])) { /* Is the first char a number? */
                    t += 2;            /* Skip over the ^C and the first digit */
                    if (IsDigit(*t))
                        t++;             /* Is this a double digit number? */
                    if (*t == ',') {   /* Do we have a background color next? */
                        if (IsDigit(t[1]))
                            t += 2;        /* Skip over the first background digit */
                        if (IsDigit(*t))
                            t++;           /* Is it a double digit? */
                    }
                } else
                      t++;
                continue;
            break;
            case 7:
                t++;
                continue;
            break;
            case 0x16:
                t++;
                continue;
            break;
            case 0x1f:
                t++;
                continue;
            break;
        }
        *a++ = ToLower(*t++);
    }
    *a = 0;

     while (temp)
     {
        if(strstr(auxbuf, temp->msg))
            return -1;
        temp = temp->next;
     }

return 0;
}

/*
 *
 * ZOMBIE 
 *
 */

// add um novo ZOMBIE
void ZOMBIE(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
    struct Client *target_p;

    if(!IsOper(source_p) && !IsServices(source_p) && !IsServer(source_p))
      return;

    if ( parc < 1 )
    {
       sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "ZOMBIE");
     return;
    }

    if((target_p = find_person(client_p, parv[1])) == NULL)
    {
        if(MyConnect(source_p))
            sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL),
                   me.name, source_p->name, parv[1]);
        return;
    }

    if(target_p)
    {

       if(IsServices(target_p) || IsZombie(target_p))
          return;

       sendto_server(client_p, source_p->servptr, NULL, CAP_ZOMBIE, NOCAPS, LL_ICLIENT,
                  ":%s ZOMBIE %s", source_p->name, parv[1]);

       sendto_realops_flags(UMODE_ALL, L_ALL,"Zombie adicionado para \2%s!%s@%s\2 por \2%s\2",
                    target_p->name, target_p->username, target_p->sockhost, parv[0]);

       if(IsClient(target_p))
       {
           SetZombie(target_p);
       }

    } else {
        if(MyConnect(source_p))
            sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL),
                   me.name, source_p->name, parv[1]);
    }
}



// remove um ZOMBIE
void UNZOMBIE(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
    struct Client *target_p;

    if(!IsOper(source_p) && !IsServices(source_p) && !IsServer(source_p))
      return;

    if ( parc < 1 )
    {
       sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "UNZOMBIE");
     return;
    }

    if((target_p = find_person(client_p, parv[1])) == NULL)
    {
        if(MyConnect(source_p))
            sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL),
                   me.name, source_p->name, parv[1]);
        return;
    }

    if(target_p)
    {

       if(!IsZombie(target_p))
          return;

       sendto_server(client_p, source_p->servptr, NULL, CAP_ZOMBIE, NOCAPS, LL_ICLIENT,
                  ":%s UNZOMBIE %s", source_p->name, parv[1]);

       sendto_realops_flags(UMODE_ALL, L_ALL,"Zombie removido de \2%s!%s@%s\2 por \2%s\2",
                    target_p->name, target_p->username, target_p->sockhost, parv[0]);

       ClearZombie(target_p);

    } else {
        if(MyConnect(source_p))
            sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL),
                   me.name, source_p->name, parv[1]);
    }
}

/*
 *
 **********************************************************************************************
 *
 */



/*
 *
 * FLOODEX 
 *
 */

// add um novo FLOODEX
void FLOODEX(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
    struct Client *target_p;

    if(!IsAdmin(source_p) && !IsServices(source_p) && !IsServer(source_p))
      return;

    if ( parc < 1 )
    {
       sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "FLOODEX");
     return;
    }

    if((target_p = find_person(client_p, parv[1])) == NULL)
    {
        if(MyConnect(source_p))
            sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL),
                   me.name, source_p->name, parv[1]);
        return;
    }

    if(target_p)
    {

       if(IsServices(target_p) || IsCanFlood(target_p))
          return;

       sendto_server(client_p, source_p->servptr, NULL, CAP_FLOODEX, NOCAPS, LL_ICLIENT,
                  ":%s FLOODEX %s", source_p->name, parv[1]);

       sendto_realops_flags(UMODE_ALL, L_ALL, "FloodEx adicionado para \2%s!%s@%s\2 por \2%s\2",
                    target_p->name, target_p->username, target_p->sockhost, parv[0]);

       if(IsClient(target_p))
       {
           SetCanFlood(target_p);
       }

    } else {
        if(MyConnect(source_p))
            sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL),
                   me.name, source_p->name, parv[1]);
    }
}



// remove um UNFLOODEX
void UNFLOODEX(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
    struct Client *target_p;

    if(!IsAdmin(source_p) && !IsServices(source_p) && !IsServer(source_p))
      return;

    if ( parc < 1 )
    {
       sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "UNFLOODEX");
     return;
    }

    if((target_p = find_person(client_p, parv[1])) == NULL)
    {
        if(MyConnect(source_p))
            sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL),
                   me.name, source_p->name, parv[1]);
        return;
    }

    if(target_p)
    {

       if(!IsCanFlood(target_p))
          return;

       sendto_server(client_p, source_p->servptr, NULL, CAP_FLOODEX, NOCAPS, LL_ICLIENT,
                  ":%s UNFLOODEX %s", source_p->name, parv[1]);

       sendto_realops_flags(UMODE_ALL, L_ALL, "FloodEx removido de \2%s!%s@%s\2 por \2%s\2",
                    target_p->name, target_p->username, target_p->sockhost, parv[0]);

       ClearCanFlood(target_p);

    } else {
        if(MyConnect(source_p))
            sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL),
                   me.name, source_p->name, parv[1]);
    }
}

/*
 *
 **********************************************************************************************
 *
 */

 /*
 *
 * GLINE 
 *
 */

// server_estab() enviando glines
void SEND_GLINES(struct Client *client_p)
{
    struct AddressRec *arec = NULL;
    int i = 0;

    for(; i < ATABLE_SIZE; ++i)
    {
        for(arec = atable[i]; arec; arec = arec->next)
        {
            if(arec->type == CONF_GLINE)
            {
                const struct AccessItem *aconf = arec->aconf;

                sendto_one(client_p, "GLINE %s@%s :%s", aconf->user, aconf->host, aconf->reason);
            }
        }
    }
}

// funcao para remover glines da memoria
int
remove_gline_match2(const char *user, const char *host)
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

// services_estab() removendo glines
void CLEAR_GLINES()
{
    struct AddressRec *arec = NULL;
    int i = 0;

    for(; i < ATABLE_SIZE; ++i)
    {
        for(arec = atable[i]; arec; arec = arec->next)
        {
            if(arec->type == CONF_GLINE)
            {
                const struct AccessItem *aconf = arec->aconf;

                if(remove_gline_match2(aconf->user, aconf->host))
                {
                    // nada para por aqui!
                }

            }
        }
    }
}


/*
 *
 **********************************************************************************************
 *
 */

// SVSINFO
void SVSINFO(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{

    if(!IsServices(source_p) && !IsServer(source_p))
      return;

    if ( parc < 1 )
    {
       sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "SVSINFO");
     return;
    }

       CLEAR_GLINES();       // limpando glines
       CLEAR_SPAMSGS();      // limpando spamsgs
       CLEAR_PROXYS();       // limpando proxys
       CLEAR_IPBRS();        // limpando ipbrs
       CLEAR_VLINKS();       // limpando vlinks

       sendto_server(client_p, source_p->servptr, NULL, CAP_SVSINFO, NOCAPS, LL_ICLIENT,
                  ":%s SVSINFO -1", source_p->name);

       sendto_realops_flags(UMODE_ALL, L_ALL, "Sincronizando os dados entre os servidores...");


}


