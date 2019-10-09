#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#include <linux/ctype.h>
#include "nw_comm.h"
#include "util.h"

void hex_dump(char *str, unsigned char *pSrcBufVA, unsigned int SrcBufLen)
{
	unsigned char *pt;
	int x;
	
	pt = pSrcBufVA;
	printk("%s: %p, len = %d\r\n",str,  pSrcBufVA, SrcBufLen);
	for (x=0; x<SrcBufLen; x++) {
		if (x % 16 == 0) 
			printk("0x%04x : ", x);
		printk("%02x ", ((unsigned char)pt[x]));
		if (x%16 == 15) printk("\r\n");
	}
	printk("\r\n");
}


//Checksum calculation function  

unsigned short csum (unsigned short *buf, int nwords)  
{  
  unsigned long sum;  
  for (sum = 0; nwords > 0; nwords--)  
      sum += *buf++;  

  sum = (sum >> 16) + (sum & 0xffff);  
  sum += (sum >> 16);  
  return ~sum;  
}


int link_test(struct dllist **phead, struct dllist **ptail) {
 struct dllist *lnode;
 int i = 0;

 /* add some numbers to the double linked list */
 for(i = 0; i <= 5; i++) {
  lnode = (struct dllist *)kmalloc(sizeof(struct dllist), GFP_KERNEL);
  lnode->number = i;
  append_node(phead,ptail,lnode);
 }

 /* print the dll list */
 for(lnode = *phead; lnode != NULL; lnode = lnode->next) {
  printk("%d\n", lnode->number);
 }

 /* destroy the dll list */
 while(*phead != NULL) {
     remove_node(phead,ptail,*ptail);
	 kfree(*ptail);
 }

 return 0;
}

void append_node(struct dllist **phead, struct dllist **ptail, struct dllist *lnode) {
 if(*phead == NULL) {
     *phead = lnode;
  lnode->prev = NULL;
 } else {
     (*ptail)->next = lnode;
     lnode->prev = *ptail;
 }

 *ptail = lnode;
 lnode->next = NULL;
}

struct dllist * find_node(struct dllist *head, struct dllist *tail, char *mac)
{
  struct dllist *lnode_tmp;
  if(head == NULL) {
     return NULL;
  }
  for(lnode_tmp=head; lnode_tmp ; lnode_tmp = lnode_tmp->next )
  {
      if( 0== memcmp(mac,lnode_tmp->mac,ETH_MAC_LEN) )
	  	return lnode_tmp; 
  }
  return NULL;
}

void insert_node( struct dllist **ptail, struct dllist *lnode, struct dllist *after) {
 lnode->next = after->next;
 lnode->prev = after;

 if(after->next != NULL)
  after->next->prev = lnode;
 else
  *ptail = lnode;

 after->next = lnode;
}

void insert_before_node(struct dllist **phead, struct dllist *lnode, struct dllist *before) {
 lnode->next = before;
 lnode->prev = before->prev;

 if(before->prev != NULL)
  before->prev->next = lnode;
 else
  *phead = lnode;

 before->prev = lnode;
}

void remove_node(struct dllist **phead, struct dllist **ptail, struct dllist *lnode) {
 if(lnode->prev == NULL)
  *phead = lnode->next;
 else
  lnode->prev->next = lnode->next;

 if(lnode->next == NULL)
  *ptail = lnode->prev;
 else
  lnode->next->prev = lnode->prev;
}

struct dllist *	get_greater_than_signal_node(struct dllist *head, struct dllist *tail,int signal)
{
	struct dllist *lnode;
	struct dllist *lcurrent;

    for( lnode=NULL,lcurrent = head; lcurrent; lcurrent = lcurrent->next ) {
		if (lcurrent->signal >= signal ){
			lnode = lcurrent;
		}
		else{
		    break;
		}		
    }
	return lnode;
}

unsigned char BtoH(char ch)
{
	if (ch >= '0' && ch <= '9') return (ch - '0');        // Handle numerals
	if (ch >= 'A' && ch <= 'F') return (ch - 'A' + 0xA);  // Handle capitol hex digits
	if (ch >= 'a' && ch <= 'f') return (ch - 'a' + 0xA);  // Handle small hex digits
	return(255);
}

void AtoH(char * src, unsigned char* dest, int destlen)
{
	char *srcptr;
	unsigned char * destTemp;

	srcptr = src;	
	destTemp = (unsigned char *) dest; 

	while(destlen--)
	{
		*destTemp = BtoH(*srcptr++) << 4;    // Put 1st ascii byte in upper nibble.
		*destTemp += BtoH(*srcptr++);      // Add 2nd ascii byte to above.
		destTemp++;
	}
}

int	SetMacAddress(
	unsigned char *CurrentAddress,
	char *arg)
{
	int	i, mac_len;
	
	/* Mac address acceptable format 01:02:03:04:05:06 length 17 */
	mac_len = strlen(arg);
	if(mac_len != 17)  
	{
		printk("%s : invalid length (%d)\n", __FUNCTION__, mac_len);
		return 0;
	}

	if(strcmp(arg, "00:00:00:00:00:00") == 0)
	{
		printk("%s : invalid mac setting \n", __FUNCTION__);
		return 0;
	}

	for (i = 0; i < ETH_MAC_LEN; i++)
	{
		AtoH(arg, &CurrentAddress[i], 1);
		arg = arg + 3;
	}	

	return 1;
}

/**
 * simple_strtoul - convert a string to an unsigned long
 * @cp: The start of the string
 * @endp: A pointer to the end of the parsed string will be placed here
 * @base: The number base to use
 */
unsigned long simple_strtoul(const char *cp,char **endp,unsigned int base)
{
	unsigned long result = 0,value;

	if (!base) {
		base = 10;
		if (*cp == '0') {
			base = 8;
			cp++;
			if ((*cp == 'x') && isxdigit(cp[1])) {
				cp++;
				base = 16;
			}
		}
	}
	while (isxdigit(*cp) &&
	       (value = isdigit(*cp) ? *cp-'0' : toupper(*cp)-'A'+10) < base) {
		result = result*base + value;
		cp++;
	}
	if (endp)
		*endp = (char *)cp;
	return result;
}

/**
 * simple_strtol - convert a string to a signed long
 * @cp: The start of the string
 * @endp: A pointer to the end of the parsed string will be placed here
 * @base: The number base to use
 */
long simple_strtol(const char *cp,char **endp,unsigned int base)
{
	if(*cp=='-')
		return -simple_strtoul(cp+1,endp,base);
	return simple_strtoul(cp,endp,base);
}


/**
 * strpbrk - Find the first occurrence of a set of characters
 * @cs: The string to be searched
 * @ct: The characters to search for
 */
 #if 0
char * strpbrk(char * cs, char * ct)
{
	char *sc1,*sc2;

	if ((cs==0)||(ct==0)) return NULL;
 
	for( sc1 = cs; *sc1 != '\0'; ++sc1) {
		for( sc2 = ct; *sc2 != '\0'; ++sc2) {
			if (*sc1 == *sc2)
				return (char *) sc1;
		}
	}
	return NULL;
}

/**
 * strsep - Split a string into tokens
 * @s: The string to be searched
 * @ct: The characters to search for
 *
 * strsep() updates @s to point after the token, ready for the next call.
 *
 * It returns empty tokens, too, behaving exactly like the libc function
 * of that name. In fact, it was stolen from glibc2 and de-fancy-fied.
 * Same semantics, slimmer shape. ;)
 */
char * strsep(char **s, char *ct)
{
	char *sbegin = *s, *end;

	if (sbegin == NULL)
		return NULL;

	end = strpbrk(sbegin, ct);
	if (end)
		*end++ = '\0';
	*s = end;

	return sbegin;
}
#endif

#define SKIP(p) while(*p && isspace(*p)) p++;

int parser_rssi(char *mac, char *buf)
{
#if 0
			ra0 	  adhocEntry:
			HT Operating Mode : 0
			MAC 			   AID BSS RSSI0  RSSI1  RSSI2	PhMd	  BW	MCS   SGI	STBC
			00:1F:1F:1F:6D:FA  1   0   -27	  -27	 0		HTMIX	  20M	15	  0 	0	  6 		, 10, 40%
			00:0E:2E:44:68:A8  2   0   -25	  -35	 0		OFDM	  20M	7	  0 	0	  0 		, 0, 0%					 
#endif
   char *p;
   char smac[18]={0};
   int rssi0=0,rssi1=0;
   char *start, *end;
   char tmp[10]={0};
   snprintf(smac,18,"%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],mac[2],mac[3],mac[4],mac[5]);   
   p = strstr(buf,smac);
   if(!p) {
   	  printk("parser_rssi error, %s\r\n", smac );
   	 return 0;
   }
   p+=17;
   SKIP(p)
   p = strstr(p," ");
   if(!p){
   	 printk("parser_rssi error...\r\n");
	 return 0;
   }
   SKIP(p)
   p = strstr(p," ");
   if(!p){
   	   	 printk("parser_rssi error.....\r\n");
	 return 0;
   }
   SKIP(p)

   start = p;
   end = strstr(p," ");
   memcpy(tmp, start, (unsigned int)end - (unsigned int)start );

   rssi0 = simple_strtol(tmp, 0, 10);

   p =end;
   SKIP(p)
   start = p;
   end = strstr(p," ");
   memcpy(tmp, start, (unsigned int)end - (unsigned int)start );

   rssi1 = simple_strtol(tmp, 0, 10);

   printk("rssi0=%d, rssi1=%d\r\n",rssi0,rssi1);

   return (rssi0 + rssi1) /2;
}


void display_node_info(struct dllist *head)
{
	struct dllist *lcurrent;
	int i=0;
    for( lcurrent = head; lcurrent; lcurrent = lcurrent->next ) {
		printk("========[%d] node ifno =============================\n", i++);
		printk("      MAC = %02X:%02X:%02X:%02X:%02X:%02X \n", PRINT_MAC(lcurrent->mac));
		printk("      Singal=%d, txpower=%d, role=%d, left=%d, timestamp=0x%x \n",lcurrent->signal, lcurrent->txpower, lcurrent->role,  lcurrent->left, lcurrent->timestamp);
    }
}


