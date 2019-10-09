#ifndef _UTIL_H_
#define _UTIL_H_



struct dllist {
 int number;
 int status;
 int signal;
 int txpower;
 int timestamp;
 unsigned int role;
 unsigned int left;
 unsigned char mac[ETH_MAC_LEN];
 unsigned short id;
 unsigned char *table;
 unsigned int table_len;
 struct dllist *next;
 struct dllist *prev;
};



typedef union _LARGE_INTEGER {
    struct {
        int HighPart;
        unsigned int LowPart;
    } u;
    long long  QuadPart;
} LARGE_INTEGER;


typedef struct _LIST_ENTRY
{
	struct _LIST_ENTRY *pNext;
} LIST_ENTRY, *PLIST_ENTRY;

typedef struct _LIST_HEADR
{
	PLIST_ENTRY pHead;
	PLIST_ENTRY pTail;
	unsigned char size;
} LIST_HEADER, *PLIST_HEADER;


typedef struct timer_list	NDIS_MINIPORT_TIMER;
typedef struct timer_list	OS_TIMER;
typedef void (*TIMER_FUNCTION)(unsigned long);

#define OS_SEM_LOCK(__lock)					\
{												\
	spin_lock_bh((spinlock_t *)(__lock));		\
}

#define OS_SEM_UNLOCK(__lock)					\
{												\
	spin_unlock_bh((spinlock_t *)(__lock));		\
}

#define NdisAllocateSpinLock(__lock)      \
{                                       \
    spin_lock_init((spinlock_t *)(__lock));               \
}

#define NdisFreeSpinLock(lock)          \
	do{}while(0)


typedef struct _LIST_RESOURCE_OBJ_ENTRY
{
	struct _LIST_RESOURCE_OBJ_ENTRY *pNext;
	void *pRscObj;
} LIST_RESOURCE_OBJ_ENTRY, *PLIST_RESOURCE_OBJ_ENTRY;

typedef struct  _BR_TIMER_STRUCT    {
	OS_TIMER    	TimerObj;       // Ndis Timer object
	unsigned char	Valid;			// Set to True when call RTMPInitTimer
	unsigned char	State;          // True if timer cancelled
	unsigned char	PeriodicType;	// True if timer is periodic timer 
	unsigned char	Repeat;         // True if periodic timer
	unsigned long	TimerValue;     // Timer value in milliseconds
	unsigned long	cookie;			// os specific object
	void			*pAd;

}BR_TIMER_STRUCT, *PBR_TIMER_STRUCT;


#define PRINT_MAC(addr)	\	
	addr[0], addr[1], addr[2], addr[3], addr[4], addr[5] 

#define IS_BROADCAST(x) (x[0]==0xff && x[1]==0xff && x[2]==0xff && x[3]==0xff && x[4]==0xff && x[5]==0xff)
#define SAME_MAC(x,y) (x[0]==y[0] && x[1]==y[1] && x[2]==y[2] && x[3]==y[3] && x[4]==y[4] && x[5]==y[5])
#define IS_ZERO_MAC(x) (x[0]==0 && x[1]==0 && x[2]==0 && x[3]==0 && x[4]==0 && x[5]==0)



void hex_dump(char *str, unsigned char *pSrcBufVA, unsigned int SrcBufLen);
void append_node(struct dllist **phead, struct dllist **ptail, struct dllist *lnode) ;
void insert_node( struct dllist **ptail, struct dllist *lnode, struct dllist *after);
void remove_node(struct dllist **phead, struct dllist **ptail, struct dllist *lnode);
void insert_before_node(struct dllist **phead, struct dllist *lnode, struct dllist *before);
struct dllist *	get_greater_than_signal_node(struct dllist *head, struct dllist *tail,int signal);
struct dllist * find_node(struct dllist *head, struct dllist *tail, char *mac);

int link_test(struct dllist **phead, struct dllist **ptail);

int	SetMacAddress( unsigned char *CurrentAddress, char *arg);
unsigned short csum (unsigned short *buf, int nwords) ;
void display_node_info(struct dllist *head);
int parser_rssi(char *mac, char *buf);


#endif
