/* unirc.h
 * by --XOOM
 *
 * 2009-01-31 adicionado IPBR
 */

#include "ircd_defs.h"



/*
 *
 * proxy 
 *
 */
typedef struct struct_proxy ProxyList;
struct struct_proxy
{
  char                  *host;
  struct struct_proxy   *next;
};
struct struct_proxy;

extern struct struct_proxy* find_proxy(const char *);

extern void PROXY(struct Client *, struct Client *, int , char **);
extern void UNPROXY(struct Client *, struct Client *, int , char **);

extern void SEND_PROXYS(struct Client *);
extern void CLEAR_PROXYS();

extern void PROXYLIST(struct Client *);
/** fim proxy **/


/*
 *
 * vlink 
 *
 */
typedef struct struct_vlink VLINKList;
struct struct_vlink
{
  char                  *server_pai;
  char                  *server_filho;
  char                  *endereco;
  char                  *descricao;
  struct struct_vlink   *next;
};
struct struct_vlink;

extern struct struct_vlink* find_vlink(const char *);
extern struct struct_vlink* find_vlink_nome(const char *);

extern void VLINK(struct Client *, struct Client *, int , char **);
extern void UNVLINK(struct Client *, struct Client *, int , char **);
extern void SETVLINK(struct Client *, struct Client *, int , char **);
extern void VLINKLINKS(struct Client *source_p );

extern int MaxVlink;

extern void SEND_VLINK(struct Client *);
extern void CLEAR_VLINK();

extern char *VlinkGetDesc(char *server);

extern void VLINKLIST(struct Client *);
/** fim vlink **/



/*
 *
 * ipbr 
 *
 */
typedef struct struct_ipbr IPBRList;
struct struct_ipbr
{
  char                  *host;
  struct struct_ipbr    *next;
};
struct struct_ipbr;

extern struct struct_ipbr* find_ipbr(const char *);

extern void IPBR(struct Client *, struct Client *, int , char **);
extern void UNIPBR(struct Client *, struct Client *, int , char **);

extern void SEND_IPBRS(struct Client *);
extern void CLEAR_IPBRS();

extern void IPBRLIST(struct Client *);
/** fim ipbr **/


/////////////////////////////////////////////////////////////////////////////


// FUNCAO FILTRA SPAM!
int check_nospam(char *);


/////////////////////////////////////////////////////////////////////////////

/*
 *
 * spamsg 
 *
 */
typedef struct struct_spamsg SpamsgList;
struct struct_spamsg
{
  char                  *msg;
  struct struct_spamsg  *next;
};
struct struct_spamsg;

extern struct struct_spamsg* find_spamsg(const char *);

extern void SPAMSG(struct Client *, struct Client *, int , char **);
extern void UNSPAMSG(struct Client *, struct Client *, int , char **);
extern void SPAMSGLIST(struct Client *);

extern void SEND_SPAMSGS(struct Client *);
extern void CLEAR_SPAMSGS();

int verifica_spamsg(char *);
/** fim spamsg **/

/////////////////////////////////////////////////////////////////////////////

// GLINE
extern void SEND_GLINES(struct Client *);
extern void CLEAR_GLINES();

/////////////////////////////////////////////////////////////////////////////

/*
 *
 * zombie
 *
 */
extern void ZOMBIE(struct Client *, struct Client *, int , char **);
extern void UNZOMBIE(struct Client *, struct Client *, int , char **);
/** fim zombie **/

/////////////////////////////////////////////////////////////////////////////

/*
 *
 * floodex
 *
 */
extern void FLOODEX(struct Client *, struct Client *, int , char **);
extern void UNFLOODEX(struct Client *, struct Client *, int , char **);
/** fim floodex **/

/////////////////////////////////////////////////////////////////////////////

/*
 *
 * svsinfo
 *
 */
extern void SVSINFO(struct Client *, struct Client *, int , char **);
/** fim svsinfo **/

/////////////////////////////////////////////////////////////////////////////


