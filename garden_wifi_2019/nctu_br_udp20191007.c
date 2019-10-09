/******************************************************************************
** HISTORY
** $Date     $Author         $Comment
**  2012     Kevin Yeh       first version	 for internal debug only
**  2012                          add neighbor discovery program
**  2013                          add TDMA function
**  2013    August            add UART function
**  2019

*******************************************************************************/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>	
#include <linux/timer.h>

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/smp_lock.h>
	
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/in.h>

#include <linux/signal.h> 

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ethtool.h>
#include <linux/ip.h>
#include <linux/wireless.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/if_arp.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/wireless.h>
#include <net/iw_handler.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/smp.h>

#include "nw_comm.h"
#include "util.h"
#include "irq_timer.h"
#include "nctubr.h"
#include "send_func.h"

#if UART_SUPPORT==1
//#define TDMA_NODE_NUMBERS  3 
//#define TDMA_NODE_NUMBERS  238  // 1 second has 238 4.2ms period
#define TDMA_NODE_NUMBERS  119  // half second
#elif DUPLICATED_PKT==1 // duplicate pkt test		
#define TDMA_NODE_NUMBERS  40  // 330ms
#else
//#define TDMA_NODE_NUMBERS  10  // half second
//#define TDMA_NODE_NUMBERS  50  // half second
///--#define TDMA_NODE_NUMBERS  30  // 250ms

#if MULTI_PKYS_PER_SLOT ==1
#define TDMA_NODE_NUMBERS  15  // 60ms for 4ms test
#else
#define TDMA_NODE_NUMBERS  10
#endif


#endif


#define PRINTK_BUF        1
#define _NO_KMALLOC

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("kernel thread listening on a UDP socket (code example)");
MODULE_AUTHOR("Toni Garcia-Navarro <topi@phreaker.net>");


static unsigned int iobank_major		= IOBANK_MAJOR;
static char*	devName		= "iobank";

int register_iobank_drv (unsigned int major, char* dev_name);
void unregister_iobank_drv (unsigned int major, char* dev_name);


///--#define TDMA_PMMC_DEF   0xFFFECF4F;                  // 19500 * 4
#if DOUBLE_SLOT_SIZE == 1
#define TDMA_PMMC_DEF  0xFFFF67A7   // after disable schedule, 19500 * 2
#define ABOUT_ONE_SECOND_NUM   125
#else
#define TDMA_PMMC_DEF  0xFFFFB3D3   // after disable schedule
#define ABOUT_ONE_SECOND_NUM   250
#endif

unsigned char timeout_buf[ETH_FRAME_LEN];
unsigned char process_buf[ETH_FRAME_LEN];


static char *mymac   = "00:00:00:00:00:00";
static char *nextmac = "00:00:00:00:00:00";
static char *premac  = "00:00:00:00:00:00";

int nodemode = 0;
static int mlme_test = 0;
int test_count = MAX_TEST_NUMBER;
int fixed_id = 0;
int time_slot_mode=0;

int uart_flag=0;


unsigned char src_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
unsigned char pre_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
unsigned char next_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

module_param(mymac, charp, 0);
module_param(nextmac, charp, 0);
module_param(premac, charp, 0);
module_param (nodemode, int, 0);
module_param (test_count, int, 0);
module_param (fixed_id, int, 0);
module_param (time_slot_mode, int, 0);



MODULE_PARM_DESC(mymac,   "My MAC address");
MODULE_PARM_DESC(nextmac, "Next node MAC address");
MODULE_PARM_DESC(premac,  "Previous node MAC address");
MODULE_PARM_DESC(nodemode, "Node number");
MODULE_PARM_DESC(test_count, "Test count for controlling how many packets would be sent");
MODULE_PARM_DESC(fixed_id, "Test node's id number");
MODULE_PARM_DESC(time_slot_mode, "Test time slot mode");



/////////// Wireless Driver Control API /////////////////

int Uplayer_layer_Set_PeerN_Proc(char *arg);
int	Uplayer_Show_MacTable_Proc(char *extra);
int Uplayer_layer_Remove_PeerN_Proc(char *arg);


int serail_get_char_directly(char *ch_ret, int dbg);
void set_serial_func_ptr( void *func);

int (*get_wlan_driver_table)(void *);
EXPORT_SYMBOL(get_wlan_driver_table);

typedef spinlock_t			NDIS_SPIN_LOCK;

NDIS_SPIN_LOCK TimerSemLock;
unsigned long	DebugLevel=DEBUG_ALL;
//unsigned long	DebugLevel=DEBUG_TDMA;





DEV_MAIN_DISCRIPTION *gState = NULL;

#if JUST_TDMA==1
struct dllist forward_lnode;
struct dllist backward_lnode;
#endif
 
unsigned long diff_pmmc_acumulation = 0;
unsigned long diff_pmmc_num = 0;

 
int ksocket_receive(struct socket* sock, struct sockaddr_ll *addr, unsigned char* buf, int len); 
int ksocket_send(struct socket *sock, struct sockaddr_ll *addr, unsigned char *buf, int len);

int packet_protocol_check(char *buffer);
int build_neighbor_list_data(DEV_MAIN_DISCRIPTION *pDesc, unsigned char *buf, int maximum_len );
int make_neighbor_table_page_by_mac( DEV_MAIN_DISCRIPTION *pDesc, unsigned char *src_mac, unsigned char *neighbor_mac, short rssi_level) ;
int make_neighbor_table_page( DEV_MAIN_DISCRIPTION *pDesc,  unsigned char *src_mac, unsigned char *table, int len );
int update_neighbor_list_rssi(DEV_MAIN_DISCRIPTION *pDesc );
int decide_routing_list(DEV_MAIN_DISCRIPTION *pDesc);

#if NCTU_SIGNAL_SEARCH == 1
int choose_node_near_left_edge(DEV_MAIN_DISCRIPTION *pDesc);
int decide_next_master_node(DEV_MAIN_DISCRIPTION *pDesc);
#endif



void OS_TimerListAdd( void *pAd, void	*pRsc);
void OS__TimerListRelease( void *	 pAd);

#define DECLARE_TIMER_FUNCTION(_func)			\
		void os_timer_##_func(unsigned long data)

#define GET_TIMER_FUNCTION(_func)				\
	(void *)os_timer_##_func

	

DECLARE_TIMER_FUNCTION(MlmePeriodicExec);
DECLARE_TIMER_FUNCTION(TimeoutExec);


/////////////////////////////////////////////////////////////////



static inline void initList(PLIST_HEADER pList)
{
	pList->pHead = pList->pTail = NULL;
	pList->size = 0;
	return;
}

static inline void insertTailList(PLIST_HEADER pList, PLIST_ENTRY pEntry)
{
	pEntry->pNext = NULL;
	if (pList->pTail)
		pList->pTail->pNext = pEntry;
	else
		pList->pHead = pEntry;
	pList->pTail = pEntry;
	pList->size++;

	return;
}

static inline PLIST_ENTRY removeHeadList(
	 PLIST_HEADER pList)
{
	PLIST_ENTRY pNext;
	PLIST_ENTRY pEntry;

	pEntry = pList->pHead;
	if (pList->pHead != NULL)
	{
		pNext = pList->pHead->pNext;
		pList->pHead = pNext;
		if (pNext == NULL)
			pList->pTail = NULL;
		pList->size--;
	}
	return pEntry;
}

static inline int getListSize(
	 PLIST_HEADER pList)
{
	return pList->size;
}

static inline PLIST_ENTRY delEntryList(
	 PLIST_HEADER pList,
	 PLIST_ENTRY pEntry)
{
	PLIST_ENTRY pCurEntry;
	PLIST_ENTRY pPrvEntry;

	if(pList->pHead == NULL)
		return NULL;

	if(pEntry == pList->pHead)
	{
		pCurEntry = pList->pHead;
		pList->pHead = pCurEntry->pNext;

		if(pList->pHead == NULL)
			pList->pTail = NULL;

		pList->size--;
		return pCurEntry;
	}

	pPrvEntry = pList->pHead;
	pCurEntry = pPrvEntry->pNext;
	while(pCurEntry != NULL)
	{
		if (pEntry == pCurEntry)
		{
			pPrvEntry->pNext = pCurEntry->pNext;

			if(pEntry == pList->pTail)
				pList->pTail = pPrvEntry;

			pList->size--;
			break;
		}
		pPrvEntry = pCurEntry;
		pCurEntry = pPrvEntry->pNext;
	}

	return pCurEntry;
}




/* timeout -- ms */
void OS_SetPeriodicTimer(
		NDIS_MINIPORT_TIMER *pTimer, 
		unsigned long timeout)
{
	timeout = ((timeout*HZ) / 1000);
	pTimer->expires = jiffies + timeout;
	add_timer(pTimer);
}

/* convert NdisMInitializeTimer --> OS_Init_Timer */
void OS_Init_Timer(
		void * pAd,
		NDIS_MINIPORT_TIMER *pTimer, 
		TIMER_FUNCTION function,
		void * data)
{
	if (!timer_pending(pTimer))
	{
		init_timer(pTimer);
		pTimer->data = (unsigned long)data;
		pTimer->function = function;		
	}
}


void OS_Add_Timer(
		NDIS_MINIPORT_TIMER		*pTimer,
		unsigned long timeout)
{
	if (timer_pending(pTimer))
		return;

	timeout = ((timeout*HZ) / 1000);
	pTimer->expires = jiffies + timeout;
	add_timer(pTimer);
}

void OS_Mod_Timer(
		NDIS_MINIPORT_TIMER		*pTimer,
		unsigned long timeout)
{
	timeout = ((timeout*HZ) / 1000);
	mod_timer(pTimer, jiffies + timeout);
}

void OS_Del_Timer(
		NDIS_MINIPORT_TIMER		*pTimer,
		unsigned char			*pCancelled)
{
	if (timer_pending(pTimer))
	{	
		*pCancelled = del_timer_sync(pTimer);	
	}
	else
	{
		*pCancelled = TRUE;
	}
	
}
void OS_Release_Timer(
	 NDIS_MINIPORT_TIMER	*pTimerOrg)
{
}


	
// Unify all delay routine by using udelay
void UsecDelay(
	unsigned long usec)
{
	unsigned long	i;
	for (i = 0; i < (usec / 50); i++)
		udelay(50);
	if (usec % 50)
		udelay(usec % 50);
}

void GetCurrentSystemTime(LARGE_INTEGER *time)
{
	time->u.LowPart = jiffies;
}


unsigned char os_alloc_mem(
	void  *pAd,
	unsigned char **mem,
	unsigned long  size)
{	
	*mem = (unsigned char*) kmalloc(size, GFP_ATOMIC);
	if (*mem)
	{
		return 1;
	}
	else
		return 0;
}

// pAd MUST allow to be NULL
unsigned char os_free_mem(
	void  *pAd,
    void  * mem)
{
	ASSERT(mem);
	kfree(mem);
	return 1;
}


void	OSInitTimer(
		void *			      pAd,
		PBR_TIMER_STRUCT      pTimer,
		void *				  pTimerFunc,
		void *				  pData,
		unsigned char		  Repeat)
{
	OS_SEM_LOCK(&TimerSemLock);

	OS_TimerListAdd(pAd, pTimer);

	//
	// Set Valid to TRUE for later used.
	// It will crash if we cancel a timer or set a timer 
	// that we haven't initialize before.
	// 
	pTimer->Valid      = TRUE;
	
	pTimer->PeriodicType = Repeat;
	pTimer->State      = FALSE;
	pTimer->cookie = (unsigned long) pData;
	pTimer->pAd = pAd;

	OS_Init_Timer(pAd, &pTimer->TimerObj,	pTimerFunc, (void *) pTimer);	

	OS_SEM_UNLOCK(&TimerSemLock);
}

void	OSSetTimer(
		PBR_TIMER_STRUCT	pTimer,
		unsigned long			Value)
{
	if (pTimer->Valid)
	{
		OS_SEM_LOCK(&TimerSemLock);
		
		void *pAd;
		
		pAd = pTimer->pAd;
		if (0)
		{
			DBGPRINT_ERR(("OSSetTimer failed, Halt in Progress!\n"));
			OS_SEM_UNLOCK(&TimerSemLock);
			return;
		}
		
		pTimer->TimerValue = Value;
		pTimer->State      = FALSE;
		if (pTimer->PeriodicType == TRUE)
		{
			pTimer->Repeat = TRUE;
			OS_SetPeriodicTimer(&pTimer->TimerObj, Value);
		}
		else
		{
			pTimer->Repeat = FALSE;
			OS_Add_Timer(&pTimer->TimerObj, Value);
		}
	}
	else
	{
		DBGPRINT_ERR(("RTMPSetTimer failed, Timer hasn't been initialize!\n"));
	}
	OS_SEM_UNLOCK(&TimerSemLock);
}


void	OSCancelTimer(
		PBR_TIMER_STRUCT	pTimer,
		unsigned char			*pCancelled)
{
	OS_SEM_LOCK(&TimerSemLock);
	
	if (pTimer->Valid)
	{
		if (pTimer->State == FALSE)
			pTimer->Repeat = FALSE;
		
		OS_Del_Timer(&pTimer->TimerObj, pCancelled);
		
		if (*pCancelled == TRUE)
			pTimer->State = TRUE;
	}
	else
	{
		DBGPRINT_ERR(("RTMPCancelTimer failed, Timer hasn't been initialize!\n"));
	}
	
	OS_SEM_UNLOCK(&TimerSemLock);
}

void	OSModTimer(
		PBR_TIMER_STRUCT	pTimer,
		unsigned long			Value)
{
	unsigned char Cancel;

	OS_SEM_LOCK(&TimerSemLock);
	
	if (pTimer->Valid)
	{
		pTimer->TimerValue = Value;
		pTimer->State      = FALSE;
		if (pTimer->PeriodicType == TRUE)
		{
			OS_SEM_UNLOCK(&TimerSemLock);
			OSCancelTimer(pTimer, &Cancel);
			OSSetTimer(pTimer, Value);
		}
		else
		{
			OS_Mod_Timer(&pTimer->TimerObj, Value);
			OS_SEM_UNLOCK(&TimerSemLock);
		}
	}
	else
	{
		DBGPRINT_ERR(("RTMPModTimer failed, Timer hasn't been initialize!\n"));
		OS_SEM_UNLOCK(&TimerSemLock);
	}
}



void	OSReleaseTimer(
		PBR_TIMER_STRUCT	pTimer,
		unsigned char			*pCancelled)
{
	OS_SEM_LOCK(&TimerSemLock);

	if (pTimer->Valid)
	{
		if (pTimer->State == FALSE)
			pTimer->Repeat = FALSE;
		
		OS_Del_Timer(&pTimer->TimerObj, pCancelled);
		
		if (*pCancelled == TRUE)
			pTimer->State = TRUE;


		/* release timer */
		OS_Release_Timer(&pTimer->TimerObj);

		pTimer->Valid = FALSE;

		DBGPRINT(DEBUG_ALL,("%s: %lx\n",__FUNCTION__, (unsigned long)pTimer));
	}
	else
	{
		DBGPRINT(DEBUG_ALL,("OSReleaseTimer, Timer hasn't been initialize!\n"));
	}

	OS_SEM_UNLOCK(&TimerSemLock);
}


void	OS_TimerListAdd(
		void 		*pAd,
		void		*pRsc)
{
	LIST_HEADER *pRscList = &((DEV_MAIN_DISCRIPTION *)pAd)->RscTimerCreateList;
	LIST_RESOURCE_OBJ_ENTRY *pObj;


	/* try to find old entry */
	pObj = (LIST_RESOURCE_OBJ_ENTRY *)(pRscList->pHead);
	while(1)
	{
		if (pObj == NULL)
			break;
		if ((unsigned long)(pObj->pRscObj) == (unsigned long)pRsc)
			return; /* exists */
		pObj = pObj->pNext;
	}

	/* allocate a timer record entry */
	os_alloc_mem(NULL, (unsigned char **)&(pObj), sizeof(LIST_RESOURCE_OBJ_ENTRY));
	if (pObj == NULL)
	{
		DBGPRINT(DEBUG_ERROR, ("%s: alloc timer obj fail!\n", __FUNCTION__));
		return;
	}
	else
	{
		pObj->pRscObj = pRsc;
		insertTailList(pRscList, (LIST_ENTRY *)pObj);
		DBGPRINT(DEBUG_ERROR, ("%s: add timer obj %lx!\n", __FUNCTION__, (unsigned long)pRsc));
	}
}


void	OS_TimerListRelease(
	void *			pAd)
{
	LIST_HEADER *pRscList = &((DEV_MAIN_DISCRIPTION *)pAd)->RscTimerCreateList;
	LIST_RESOURCE_OBJ_ENTRY *pObj, *pObjOld;
	unsigned char Cancel;


	/* try to find old entry */
	pObj = (LIST_RESOURCE_OBJ_ENTRY *)(pRscList->pHead);
	while(1)
	{
		if (pObj == NULL)
			break;
		DBGPRINT(DEBUG_ERROR, ("%s: Cancel timer obj %lx!\n", __FUNCTION__, (unsigned long)(pObj->pRscObj)));
		OSReleaseTimer(pObj->pRscObj, &Cancel);
		pObjOld = pObj;
		pObj = pObj->pNext;
		os_free_mem(NULL, pObjOld);
	}

	/* reset TimerList */
	initList(pRscList);
}

#define BUILD_TIMER_FUNCTION(_func)										\
void os_timer_##_func(unsigned long data)										\
{																			\
	PBR_TIMER_STRUCT	pTimer = (PBR_TIMER_STRUCT) data;				\
																			\
	_func(NULL, (void *) pTimer->cookie, NULL, pTimer); 							\
	if (pTimer->Repeat)														\
		OS_Add_Timer(&pTimer->TimerObj, pTimer->TimerValue);			\
}



int show_neighbor_mac_rssi(DEV_MAIN_DISCRIPTION *pDesc )
{
	char *buf;
	buf = kmalloc(4096, GFP_KERNEL);
	if(buf){
		Uplayer_Show_MacTable_Proc(buf);
		printk("%s",buf);
	}
}


///////////////////////////////////////////////////////////////////////////////
// TDMA timer related functions
///////////////////////////////////////////////////////////////////////////////

#if PRINTK_BUF ==1
#define PRINTK_DBG_BUF_MAX    1400
char printk_msg_buffer[PRINTK_DBG_BUF_MAX];
int printk_msg_buffer_idx;
#endif


#if TDMA_FUNCTION_START==1

unsigned int send_count;
static unsigned int cnt_en[CNTMAX];
unsigned int full_count;
static struct tasklet_struct my_tasklet;

int irq_request_interrupts(int irqs);
static irqreturn_t armv7_pmnc_interrupt(int irq, void *arg);

#if UART_SUPPORT==1

#define  UART_DBG_PRINT  1

#define  UART_PKT_SIZE   1000
#define  USRT_BUFF_SIZE  10240

int  test_idx_head=0;
int  test_idx_tail=0;
int  uart_number = 0;
char test_buffer[USRT_BUFF_SIZE];


#if UART_DBG_PRINT==1
#define DBG_BUF_MAX    1400
char msg_buffer[USRT_BUFF_SIZE];
int msg_buffer_idx;
#endif

int send_char(char word);
int recv_char(char *word);
void linux_uart_init(void);
void linux_uart_close(void);
int tty_read( int timeout, unsigned char *string) ;
void uart_rx(void);
void uart_rx_char(char c);

int uart_recv_packet(unsigned char *p, int buffer_len, int *p_ret_len );
void uart_send_packet(unsigned char *p, int len);


char uart_send_buff[ETH_FRAME_LEN];
char uart_receive_buff[ETH_FRAME_LEN];

#define MAX_SEND_LENGTH  UART_PKT_SIZE

spinlock_t uartLock;

//DEFINE_SPINLOCK(uartLock);



void init_uart_send_buf(void)
{
  printk("init_uart_send_buf\n");
  int i, count;
  for(i=0,count=0;i<MAX_SEND_LENGTH; i++) {
  	uart_send_buff[i] = count;
	count++;
	if(count>255)
		count=0;
  }
}

#if UART_DBG_PRINT==1

int dbg_thread(void *s)
{
    int i;
	int timeout; 
	wait_queue_head_t timeout_wq; 
	init_waitqueue_head(&timeout_wq); 		
	msg_buffer_idx =0;
	printk("HZ = %d\n", HZ); 
	
    while(1) {
        if(gState->dev_state != NTBR_STATE_TDMA) {
			if(msg_buffer_idx) {
				uart_send_packet(msg_buffer,msg_buffer_idx);
				msg_buffer_idx =0;
			}
        }	
#if 0		
        set_current_state(TASK_INTERRUPTIBLE);
        schedule();
#else
		//sleep_on_timeout(&timeout_wq, HZ);						 //(1) 
		timeout = interruptible_sleep_on_timeout(&timeout_wq, HZ);//(2) 
		printk("timeout = %d\n", timeout); 
		if (!timeout) 
			printk("timeout\n"); 
#endif
    }
}
#endif
#endif
unsigned char check_id_range( int current_id, int my_id )// Ex: the fourth node, my_id is 3, 0 ~5, 6~9
{                                                        // Ex: the second node, my_id is 1, 0, 1-2
    int before_id, id_num;
    if( time_slot_mode ==0 ||  time_slot_mode ==1) {
#if DUPLICATED_PKT==1 // duplicate pkt test		
    //if (current_id == my_id + 10 || current_id == my_id + 20  || current_id == my_id )
#else
    	if(current_id == my_id)
#endif
			return 1;
		else
        	return 0;
    }else if(time_slot_mode==3) {
        if(current_id <(my_id +1)*4 && current_id >= my_id*4)
            return 1;
        else
            return 0;
    }
    if(my_id == 0) {
        if(current_id ==0) 
            return 1;
        else
            return 0;
    }
    before_id = my_id;
    id_num = (before_id * ( before_id + 1) )>>1; // total * (an + a1) / 2
    if((current_id > id_num -1)  &&  (current_id <= my_id + id_num ))
        return 1;
    return 0;
}
static void my_softirq(unsigned long data)
{
	DEV_MAIN_DISCRIPTION *pDesc;
	void *buf_send;
	unsigned int tick;
	pDesc = (DEV_MAIN_DISCRIPTION *)data;
	static int exec_count =0;
	int resync = 0;
	printk("my_softirq, state=%d\n",pDesc->dev_state);
	if(exec_count++==ABOUT_ONE_SECOND_NUM) {
		printk("S");				   
		exec_count=0;
	}
	if(pDesc->dev_state != NTBR_STATE_STEADY && pDesc->dev_state != NTBR_STATE_TDMA_INIT && pDesc->dev_state != NTBR_STATE_TDMA) {
		printk("return\n");
		return;
	}
	if (pDesc->dev_state == NTBR_STATE_STEADY) {
		printk("NTBR_STATE_STEADY\n");
		if (GET_TICK_COUNT(pDesc) < pDesc->tdma_info.tdms_start_delay_ticks) {
			return; 
		}
		switch( pDesc->sub_state) {
			case NTBR_SUBSTATE_READY_FORWARD:
			case NTBR_SUBSTATE_RECEIVED_FORWARD:
				DBGPRINT(DEBUG_ERROR,("TDMA start timeout but in NTBR_STATE_STEADY, NTBR_SUBSTATE_RECEIVED_FORWARD\n"));	
			default:
				break;
			case NTBR_SUBSTATE_FORWARD_DONE:
			{
				memset(&(pDesc->buffer), 0, sizeof(BUFF_DATA));
				memset(&(pDesc->tdma_status), 0, sizeof(TDMA_STATUS));
				memset(&(pDesc->tdma_count), 0, sizeof(TDMA_STATUS_TEMP));
#if MULTI_PKYS_PER_SLOT==1
				pDesc->frames = 0;
#endif				
				pDesc->sequence = 0;
				pDesc->dev_state = NTBR_STATE_TDMA;
				pDesc->sub_state = NTBR_SUBSTATE_FORWARD_DONE;
				pDesc->tdma_count.current_hz = HZ;
				pDesc->total_tick_number = 0;
				pDesc->buffer.len = 0;
				INIT_TICK_COUNT(pDesc);
				// if my time slot, send out the data
				tick = GET_TICK_COUNT(pDesc) % pDesc->tdma_info.total_slot; 
#if DYNAMIC_SLOT_ASSIGN==1
                if(check_id_range(tick, pDesc->tdma_info.id)) {
#else
				//if( tick  == pDesc->tdma_info.timeslot[0] ){
#endif
#ifdef _NO_KMALLOC
						buf_send = timeout_buf;
#else						  
						buf_send = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
						if (!buf_send) {
							DBGPRINT(DEBUG_ERROR, ("%s: %d, malloc fail\n", __FUNCTION__, __LINE__));
							return FAILED;
						}
#endif					   
						memset(buf_send, 0,ETH_FRAME_LEN);					   
						if(pDesc->forward_node)
						    send_cmm_data( pDesc, 0, NULL, ((struct dllist *)(pDesc->forward_node))->id, buf_send, ETH_FRAME_LEN, SYNC_DONE );
#ifndef _NO_KMALLOC						
					   kfree(buf_send);
#endif
				}
				break;
			}
		}
	}
	else if (pDesc->dev_state == NTBR_STATE_TDMA_INIT ) {
		printk("NTBR_STATE_TDMA_INIT\n");
		
		if (GET_TICK_COUNT(pDesc) < pDesc->tdma_info.tdms_start_delay_ticks ) {
			return; 
		}
		// clean forwarding buffer
		memset(&(pDesc->buffer), 0, sizeof(BUFF_DATA));
		memset(&(pDesc->tdma_status), 0, sizeof(TDMA_STATUS));
		memset(&(pDesc->tdma_count), 0, sizeof(TDMA_STATUS_TEMP));
#if MULTI_PKYS_PER_SLOT==1
		pDesc->frames = 0;
#endif				
		pDesc->sequence = 0;
		// send data in my time slot
		pDesc->dev_state = NTBR_STATE_TDMA;
		pDesc->sub_state = NTBR_SUBSTATE_FORWARD_DONE;
		//DBGPRINT(DEBUG_ALL,("Start TDMA state...\n"));
		pDesc->tdma_count.current_hz = HZ;
		pDesc->total_tick_number = 0;
		pDesc->buffer.len = 0;
		INIT_TICK_COUNT(pDesc);
		tick = GET_TICK_COUNT(pDesc) % pDesc->tdma_info.total_slot; 
#if DYNAMIC_SLOT_ASSIGN==1
        if( check_id_range(tick, pDesc->tdma_info.id ) ) {
#else
		//if( tick == pDesc->tdma_info.timeslot[0] ){
#endif
#ifdef _NO_KMALLOC
		   buf_send = timeout_buf;
#else						  		
		   buf_send = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
		   if (!buf_send) {
				DBGPRINT(DEBUG_ERROR, ("%s: %d, malloc fail\n", __FUNCTION__, __LINE__));
				return FAILED;
		   }
#endif		   
		   memset(buf_send, 0,ETH_FRAME_LEN);			   
		   // send data 
		   if(pDesc->forward_node)
			  send_cmm_data( pDesc, 0, NULL, ((struct dllist *)(pDesc->forward_node))->id, buf_send, ETH_FRAME_LEN, SYNC_DONE);
#ifndef _NO_KMALLOC			   
		   kfree(buf_send);
#endif
	    }

	}else if(pDesc->dev_state == NTBR_STATE_TDMA) {
		printk("NTBR_STATE_TDMA\n");
		tick = GET_TICK_COUNT(pDesc) % pDesc->tdma_info.total_slot ; 
#if DYNAMIC_SLOT_ASSIGN==1
		printk("tick=%d, tdma_info.id=%d\n",tick,pDesc->tdma_info.id);
        if(check_id_range(tick, pDesc->tdma_info.id)) {
#else
		if(tick == pDesc->tdma_info.timeslot[0]) {
#endif
#if NCTU_WAVE==1
            resync = SYNC_DONE;
#else
			pDesc->re_sync_count++;
			if(pDesc->re_sync_count >= RE_SYNC_CYCLY_COUNT) {
				pDesc->re_sync_count = 0;
				pDesc->sub_state = NTBR_SUBSTATE_READY_FORWARD;
				resync = RE_SYNC_REQ;
		    }else if(pDesc->sub_state == NTBR_SUBSTATE_RECEIVED_FORWARD) {
				pDesc->sub_state = NTBR_SUBSTATE_FORWARD_DONE;
                resync = RE_SYNC_SET;
            }else {
				resync = SYNC_DONE;
            }
#endif
#ifdef _NO_KMALLOC
		    buf_send = timeout_buf;
#else						  		
		    buf_send = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
		    if (!buf_send) {
				DBGPRINT(DEBUG_ERROR, ("%s: %d, malloc fail\n", __FUNCTION__, __LINE__));
				return FAILED;
			}
#endif		   
			memset(buf_send, 'A',ETH_FRAME_LEN);
			buf_send = uart_send_buff;
#if 1 // append uplayer data
//            | type | total len |<< node id | wsn id  | timestamp |  data >>| 
//               2        2          2         2          4          16
#endif
			// send data
			if(pDesc->forward_node){
			    printk("forward_node_mac=%02X,%02X,%02X,%02X,%02X,%02X  id=%d\n",pDesc->forward_node->mac[0],pDesc->forward_node->mac[1],pDesc->forward_node->mac[2],pDesc->forward_node->mac[3],pDesc->forward_node->mac[4],pDesc->forward_node->mac[5],pDesc->forward_node->id);
			    //send_cmm_data( pDesc, 0, NULL,((struct dllist *)(pDesc->forward_node))->id, buf_send, ETH_FRAME_LEN, resync);
			    send_cmm_data( pDesc, 0,  ((struct dllist *)(pDesc->forward_node))->mac,((struct dllist *)(pDesc->forward_node))->id, buf_send, ETH_FRAME_LEN, resync);
#if MULTI_PKYS_PER_SLOT==1
				send_cmm_data( pDesc, 0,  ((struct dllist *)(pDesc->forward_node))->mac,((struct dllist *)(pDesc->forward_node))->id, buf_send, ETH_FRAME_LEN, SYNC_DONE);
				send_cmm_data( pDesc, 0,  ((struct dllist *)(pDesc->forward_node))->mac,((struct dllist *)(pDesc->forward_node))->id, buf_send, ETH_FRAME_LEN, SYNC_DONE);
#endif
			    printk("k");
			}
			else if(pDesc->flag == NORMAL_NODE && pDesc->backward_node ) {					
				send_cmm_data( pDesc, 0, ((struct dllist *)(pDesc->backward_node))->mac, ((struct dllist *)(pDesc->backward_node))->id, buf_send, ETH_FRAME_LEN, SYNC_DONE ); // just for TDMA sending test					
#if MULTI_PKYS_PER_SLOT==1
				send_cmm_data( pDesc, 0, ((struct dllist *)(pDesc->backward_node))->mac, ((struct dllist *)(pDesc->backward_node))->id, buf_send, ETH_FRAME_LEN, SYNC_DONE ); // just for TDMA sending test					
				send_cmm_data( pDesc, 0, ((struct dllist *)(pDesc->backward_node))->mac, ((struct dllist *)(pDesc->backward_node))->id, buf_send, ETH_FRAME_LEN, SYNC_DONE ); // just for TDMA sending test													
#endif
				//send_cmm_data( pDesc, 0, NULL, ((struct dllist *)(pDesc->backward_node))->id, buf_send, ETH_FRAME_LEN, SYNC_DONE ); // just for TDMA sending test	
				printk("g");
			}
			else{
				send_cmm_data( pDesc, 0, NULL, 0, buf_send, ETH_FRAME_LEN, SYNC_DONE ); // just for TDMA sending test
#if MULTI_PKYS_PER_SLOT==1
                send_cmm_data( pDesc, 0, NULL, 0, buf_send, ETH_FRAME_LEN, SYNC_DONE ); // just for TDMA sending test
				send_cmm_data( pDesc, 0, NULL, 0, buf_send, ETH_FRAME_LEN, SYNC_DONE ); // just for TDMA sending test
#endif
				printk("y");
			}
#ifndef _NO_KMALLOC	
			kfree(buf_send);
#endif			
		}else {
#if UART_SUPPORT==1		
		    //uart_rx();
#endif
        }
	}
}
/*Performance Monitor Control Register*/

static inline void armv7_pmnc_write(u32 val)
{
	val &= PMNC_MASK;
	asm volatile("mcr p15, 0, %0, c9, c12, 0" : : "r" (val));
}

static inline u32 armv7_pmnc_read(void)
{
	unsigned int val;

	asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r" (val));
	return val;
}

static inline void armv7_start_pmnc(void)
{
	armv7_pmnc_write(armv7_pmnc_read() | PMNC_E);
}

static inline void armv7_stop_pmnc(void)
{
	armv7_pmnc_write(armv7_pmnc_read() & ~PMNC_E);
}

static int armv7_pmnc_start(void)
{
	armv7_start_pmnc();
	return 0;
}

static int armv7_request_irq(void)
{
	int ret;
	ret = irq_request_interrupts(INT_34XX_BENCH_MPU_EMUL);
	return ret;
}

static inline u32 armv7_pmnc_disable_counter(unsigned int cnt)
{
	u32 val;

	if (cnt >= CNTMAX) {
		printk(KERN_ERR "oprofile: CPU%u disabling wrong PMNC counter"
			" %d\n", smp_processor_id(), cnt);
		return -1;
	}

	if (cnt == CCNT)
		val = CNTENC_C;
	else
		val = (1 << (cnt - CNT0));

	val &= CNTENC_MASK;
	asm volatile("mcr p15, 0, %0, c9, c12, 2" : : "r" (val));

	return cnt;
}

static inline u32 armv7_pmnc_enable_counter(unsigned int cnt)
{
	u32 val;

	if (cnt >= CNTMAX) {
		printk(KERN_ERR "oprofile: CPU%u enabling wrong PMNC counter"
			" %d\n", smp_processor_id(), cnt);
		return -1;
	}

	if (cnt == CCNT)
		val = CNTENS_C;
	else
		val = (1 << (cnt - CNT0));

	val &= CNTENS_MASK;
	asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r" (val));

	return cnt;
}

static inline u32 armv7_pmnc_enable_intens(unsigned int cnt)
{
	u32 val;

	if (cnt >= CNTMAX) {
		printk(KERN_ERR "oprofile: CPU%u enabling wrong PMNC counter"
			" interrupt enable %d\n", smp_processor_id(), cnt);
		return -1;
	}

	if (cnt == CCNT)
		val = INTENS_C;

	else
		val = (1 << (cnt - CNT0));

	val &= INTENS_MASK;
	asm volatile("mcr p15, 0, %0, c9, c14, 1" : : "r" (val));

	return cnt;
}

static inline int armv7_pmnc_select_counter(unsigned int cnt)
{
	u32 val;

	if ((cnt == CCNT) || (cnt >= CNTMAX)) {
		printk(KERN_ERR "oprofile: CPU%u selecting wrong PMNC counteri"
			" %d\n", smp_processor_id(), cnt);
		return -1;
	}

	val = (cnt - CNT0) & SELECT_MASK;
	asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r" (val)); ///-- performance counter flags

	return cnt;
}

static inline u32 armv7_pmnc_getreset_flags(void)
{
	u32 val;

	/* Read */
	asm volatile("mrc p15, 0, %0, c9, c12, 3" : "=r" (val));  ///-- over flow flags

	/* Write to clear flags */
	val &= FLAG_MASK;
	asm volatile("mcr p15, 0, %0, c9, c12, 3" : : "r" (val));

	return val;
}



static void armv7_pmnc_reset_counter(unsigned int cnt, u32 preset_val)
{

///--	u32 val=0xFFFFB9AF;                           // 18000 
///	u32 val=0xFFFFB3D3;                                 // 19500
	u32 val=TDMA_PMMC_DEF;//0xFFFECF4F;                  // 19500 * 4

    if(preset_val)
		val = preset_val;

    // my tasklet is exectued is B9C0 ~ B9
   
	// total = number  * ( 1/300M)  *  64 , if number is 19500, the total is 4.16ms

	switch (cnt) {
	case CCNT:
		armv7_pmnc_disable_counter(cnt);

		asm volatile("mcr p15, 0, %0, c9, c13, 0" : : "r" (val));
        //if(val != TDMA_PMMC_DEF)
		//	printk("\r\n God !!!!!!!!!!!!, val=0x%x\r\n", val );

		if (cnt_en[cnt] != 0)
		    armv7_pmnc_enable_counter(cnt);

		break;

	case CNT0:
	case CNT1:
	case CNT2:
	case CNT3:
		armv7_pmnc_disable_counter(cnt);

		if (armv7_pmnc_select_counter(cnt) == cnt)
		    asm volatile("mcr p15, 0, %0, c9, c13, 2" : : "r" (val));

		if (cnt_en[cnt] != 0)
		    armv7_pmnc_enable_counter(cnt);

		break;

	default:
		printk(KERN_ERR "oprofile: CPU%u resetting wrong PMNC counter"
			" %d\n", smp_processor_id(), cnt);
		break;
	}
}

static int armv7_setup_pmnc(unsigned int preset_count )
{

	if (armv7_pmnc_read() & PMNC_E) {
		printk(KERN_ERR "oprofile: CPU%u PMNC still enabled when setup"
			" new event counter.\n", smp_processor_id());
		return -EBUSY;
	}
	/*
	 * Initialize & Reset PMNC: C bit, D bit and P bit.
	 *  Note: Using a slower count for CCNT (D bit: divide by 64) results
	 *   in a more stable system
	 */
	armv7_pmnc_write(PMNC_P | PMNC_C | PMNC_D);

	/*
	 * Disable counter
	 */
	armv7_pmnc_disable_counter(CCNT);
	cnt_en[CCNT] = 0;

	//if (!counter_config[cpu_cnt].enabled)
	//	continue;

	/*
	 * Enable interrupt for this counter
	 */
	armv7_pmnc_enable_intens(CCNT);

	/*
	 * Reset counter
	 */
	armv7_pmnc_reset_counter(CCNT, preset_count);

	/*
	 * Enable counter
	 */
	armv7_pmnc_enable_counter(CCNT);
	cnt_en[CCNT] = 1;

	return 0;
}

int irq_request_interrupts(int irqs)
{
	int ret = 0;

	ret = request_irq(irqs, armv7_pmnc_interrupt, IRQF_DISABLED, "CP15 PMNC", NULL);
	if (ret != 0) {
		printk(KERN_ERR "irq_timer: unable to request IRQ%u for ARMv7\n", irqs);
		free_irq(irqs, NULL);
	}
	return ret;
}
// iwpriv ra0 set FixedTxMode=OFDM
// iwpriv ra0 set HtMcs=6


/*
 * CPU counters' IRQ handler (one IRQ per CPU)
 */



//           18500                                 1500
//  |<--------------------->|<----------->|
//
//                                             |<-x->|
//                                                x processing delay
// 
//                                                1500 is busy waiting, it's about 320us
//
/*interrupt handler*/
static irqreturn_t armv7_pmnc_interrupt(int irq, void *arg)
{
    static int isr_count=0;
#if 0
	/*
	 * Stop IRQ generation
	 */
	asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (full_count));	
	while(full_count<=1500) {
		ndelay(10);
		asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (full_count));	
		//printk("\nwhy=0x%x\n",full_count);
		
	}
#endif	
	armv7_stop_pmnc();
	armv7_pmnc_disable_counter(CCNT);
	armv7_pmnc_reset_counter(CCNT,0);
	//asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (full_count));
	armv7_pmnc_getreset_flags();
	/*
	 * Allow IRQ generation
	 */
	armv7_start_pmnc();	
	isr_count++;
	gState->total_tick_number++;
	INC_TICK_COUNT(gState);
	if ( gState->dev_state == NTBR_STATE_TDMA ) {
		//gState->sequence++;
		if(GET_TICK_COUNT(gState)  >= gState->tdma_info.total_slot) {
			///--INIT_TICK_COUNT(gState); 
#if MULTI_PKYS_PER_SLOT==1			
			gState->frames++;
#else
            gState->sequence++;
#endif
		}
		if(isr_count>= gState->tdma_info.total_slot) {
			isr_count = 0;
			gState->cycle++;
		}
	}
	tasklet_hi_schedule(&my_tasklet);
	
	return IRQ_HANDLED;
}


#endif

const char *STATE_STR( int dev_state)
{
    const char *msg;
    switch(dev_state) {
		case NTBR_STATE_INIT:
			msg="NTBR_STATE_INIT";
			break;
		case NTBR_STATE_HELO:
			msg="NTBR_STATE_HELO";
			break;
		case NTBR_STATE_IFNO_CHANGE:
			msg="NTBR_STATE_IFNO_CHANGE";
			break;
		case NTBR_STATE_STEADY:
			msg="NTBR_STATE_STEADY";
			break;
		case NTBR_STATE_TDMA:
			msg="NTBR_STATE_TDMA";
			break;
		case NTBR_STATE_ASSIGN_MASTER:
			msg="NTBR_STATE_ASSIGN_MASTER";
			break;
		case NTBR_STATE_TDMA_INIT:
			msg="NTBR_STATE_TDMA_INIT";
			break;
		default:
			msg="UN-KNOWN";
			break;
    }
	return msg;
}

void init_dev_desc(DEV_MAIN_DISCRIPTION *dev_desc)
{
	int fd;
	int ret =0;
	int i;
	struct ifreq ifr;
	typedef struct net_device	* PNET_DEV;

	memset(dev_desc, 0, sizeof(DEV_MAIN_DISCRIPTION));
	// try to get wireless device mac address
	int retval = SetMacAddress(dev_desc->src_mac, mymac);
	if (retval)
		printk ("My MacAddress = %02x:%02x:%02x:%02x:%02x:%02x\n",PRINT_MAC(dev_desc->src_mac));
	
	/*prepare sockaddr_ll*/
	/*RAW communication*/
	dev_desc->socket_address.sll_family	= PF_PACKET;   
	/*we don't use a protocoll above ethernet layer->just use anything here*/
	dev_desc->socket_address.sll_protocol = htons(ETH_P_IP);
	/*index of the network device see full code later how to retrieve it*/
	dev_desc->socket_address.sll_ifindex = 5;
	//gState->socket_address.sll_ifindex	= if_nametoindex(ETH_NAME);
	//printk("the name to index is %d\n",if_nametoindex(ETH_NAME));
	/*ARP hardware identifier is ethernet*/
	dev_desc->socket_address.sll_hatype	 = ARPHRD_ETHER;  
	/*target is another host*/
	dev_desc->socket_address.sll_pkttype  = PACKET_OTHERHOST;
	/*address length*/
	dev_desc->socket_address.sll_halen	 = ETH_ALEN;	   
	/*MAC - begin*/
	dev_desc->socket_address.sll_addr[0]	= 0xff; 	 
	dev_desc->socket_address.sll_addr[1]	= 0xff; 	 
	dev_desc->socket_address.sll_addr[2]	= 0xff;
	dev_desc->socket_address.sll_addr[3]	= 0xff;
	dev_desc->socket_address.sll_addr[4]	= 0xff;
	dev_desc->socket_address.sll_addr[5]	= 0xff;
	/*MAC - end*/
	dev_desc->socket_address.sll_addr[6]  = 0x00;/*not used*/
	dev_desc->socket_address.sll_addr[7]  = 0x00;/*not used*/		

#if TDMA_DEBUUG_LOG == 1
    dev_desc->test_count = test_count;
#endif
	dev_desc->dev_state = NTBR_STATE_INIT;
	dev_desc->sys_timer = jiffies;
	for(i=0;i<TIME_SLOT_ARRAY_LEN;i++) {
		dev_desc->tdma_info.timeslot[i] =-1;
	}
	dev_desc->total_tick_number = 0;
}

void MlmePeriodicExec(
	void * SystemSpecific1, 
	void * FunctionContext, 
	void * SystemSpecific2, 
	void * SystemSpecific3) 
{
	static int count = 0;
	void* new_buffer;
	switch (gState->dev_state )
	{
		case NTBR_STATE_INIT:  // Timeout
		{
#if UART_SUPPORT==1 && UART_DBG_PRINT==1
			if(msg_buffer_idx<DBG_BUF_MAX-10) {
				msg_buffer_idx += sprintf(msg_buffer + msg_buffer_idx, "INIT state");
				//uart_send_packet(msg_buffer,strlen(msg_buffer));
			}		 
#endif      
			if(count++>5) {
				printk("NTBR_STATE_INIT, sub=%d\r\n", gState->sub_state);
				count =0;
			}
			break;
		}
		case NTBR_STATE_HELO:
	  	{
#if NCTU_SIGNAL_SEARCH == 1
#if UART_SUPPORT==1 && UART_DBG_PRINT==1
			if(msg_buffer_idx<DBG_BUF_MAX-10) {
				msg_buffer_idx += sprintf(msg_buffer + msg_buffer_idx, "HELLO state");
				//uart_send_packet(msg_buffer,strlen(msg_buffer));
			}   
#endif
#else	  	
			new_buffer = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
			if (!new_buffer) {
				printk("malloc fail!!\n");
				return;
			}
			memset(new_buffer, 0,ETH_FRAME_LEN);
			//memcpy(new_buffer, eh->h_source, ETH_MAC_LEN);
			send_cmm_broad_packet(gState, NULL, new_buffer, ETH_FRAME_LEN); // broadcast 
			kfree(new_buffer);
#endif		 
			if(count++>5) {
				printk("NTBR_STATE_HELO, sub=%d\r\n", gState->sub_state);
				count =0;
			}
			break;
		}
		case NTBR_STATE_IFNO_CHANGE:
		{
#if UART_SUPPORT==1 && UART_DBG_PRINT==1
			if(msg_buffer_idx<DBG_BUF_MAX-10) {
				msg_buffer_idx += sprintf(msg_buffer + msg_buffer_idx, "STATE_IFNO_CHANGE state");
				//uart_send_packet(msg_buffer,strlen(msg_buffer));
			}
#endif
			if(count++>5) {
				printk("NTBR_STATE_IFNO_CHANGE, sub=%d\r\n", gState->sub_state);
				count =0;
			}
			break;
		}
		case NTBR_STATE_ASSIGN_MASTER:
		{
#if UART_SUPPORT==1 && UART_DBG_PRINT==1
			if(msg_buffer_idx<DBG_BUF_MAX-10) {
				msg_buffer_idx += sprintf(msg_buffer + msg_buffer_idx, "STATE_ASSIGN_MASTER state");
				//uart_send_packet(msg_buffer,strlen(msg_buffer));
			}
#endif
			if(count++>5) {
				printk("NTBR_STATE_ASSIGN_MASTER, sub=%d\r\n", gState->sub_state);
				count =0;
			}
			break;
		}
		case NTBR_STATE_STEADY:
		{
			if(count++>5) {
				printk("NTBR_STATE_STEADY, sub=%d\r\n", gState->sub_state);
				count =0;
			}
			break;
		}
		case NTBR_STATE_TDMA:
	  	{			
			static unsigned long test_show_count = 0;
			static unsigned long test_old_count = 0;
			//printk("tx=%d\r\n", gState->tdma_status.total_tx_count);
			if(gState->tdma_status.total_rx_count ) {
				if( test_old_count == gState->tdma_status.total_rx_count) {
	                 test_show_count++;
				}else{
				     test_show_count = 0;
				}
				if(test_show_count>= 10 ){
					gState->dev_state  = NTBR_STATE_DEBUG;
					return 0;		
				}
	            test_old_count = gState->tdma_status.total_rx_count;
			}
			break;
	  	}	
		case NTBR_STATE_DEBUG:
	 	{		  	  
			if(count++>6) {
				int i,j;
				printk("========================================\r\n");			  	
				printk("Packet Size, Tx numbers=%d\r\n", gState->tdma_status.total_tx_count);
				printk("Packet Size, Rx out numbers=%d\r\n", gState->tdma_status.total_rx_out_count);
				printk("Packet Size, Rx numbers=%d\r\n", gState->tdma_status.total_rx_count);
				if(diff_pmmc_num)
					printk("AVG RRT=0x%x, sample number=%d\r\n", diff_pmmc_acumulation/diff_pmmc_num, diff_pmmc_num );
				else
					printk("sample number=0\r\n");
				count =0;
				printk("-----------------------------------------\r\n");
				for(i=0; i< gState->tdma_info.id; i++) {
					printk("Node %d, Rx numbers=%d\r\n", i, gState->rx_check[i].count );
					for(j=0;j<gState->test_count + 10;j++) {
						if(gState->rx_check[i].data[j] != 'I') {
							printk("%d ",j);  	
						}
					}
					printk("--------------------\r\n");
					printk("Node %d, Out of band Rx numbers=%d\r\n", i, gState->rx_out[i].count );
					for(j=0;j<gState->test_count + 10;j++) {
						if(gState->rx_out[i].data[j] == 'I') {
							printk("%d ",j);  	
						}
					}
					printk("--------------------\r\n");
					printk("Node %d, duplicate Rx numbers=%d\r\n", i, gState->duplicate[i].count );
					for(j=0;j<gState->test_count + 10;j++) {
						if(gState->duplicate[i].data[j] != 0) {
							printk("%d-%d ",j,gState->duplicate[i].data[j] );  	
						}
					}
					printk("--------------------\r\n");
				}
				  
			}
     	}
	 	break;
	  	
    }
}

void TimeoutExec(
	void * SystemSpecific1, 
	void * FunctionContext, 
	void * SystemSpecific2, 
	void * SystemSpecific3) 
{
	void* buf_send;
	int ret,i, uart_receive_len;
	char ctmp[500] = "N";
	char* uart_receive_buf;
	unsigned char cancel;
   
	switch (gState->dev_state ) {
		case NTBR_STATE_HELO:
		{
			unsigned char transfer = 0;
#if JUST_TDMA==1
			if( gState->retry++ >= 20 ) {
				gState->retry = 0;
				if (gState->flag == LEFT_EDGE_NODE) {
					OSCancelTimer( &gState->TimeoutTimer, &cancel);//cancel TimeoutTimer
					// clean forwarding buffer
					memset(&(gState->buffer), 0, sizeof(BUFF_DATA));
					memset(&(gState->tdma_status), 0, sizeof(TDMA_STATUS));
					memset(&(gState->tdma_count), 0, sizeof(TDMA_STATUS_TEMP));
					gState->sequence = 0;
#if MULTI_PKYS_PER_SLOT==1			
					gState->frames = 0;
#endif					  
					// send data in my time slot
					gState->dev_state = NTBR_STATE_TDMA;
					gState->sub_state = NTBR_SUBSTATE_FORWARD_DONE;
					//DBGPRINT(DEBUG_ALL,("Start TDMA state...\n"));
					gState->tdma_count.current_hz = HZ;
					gState->total_tick_number = 0;
					gState->buffer.len = 0;
					// init TDMA tick counter
					INIT_TICK_COUNT(gState);
					// set TDMA delay tick numbers
				    SET_TDMA_START_DELAY_TICK(gState,2);
					/*setup cycle count timer*/ 
					armv7_setup_pmnc(0);
					/*request irq and start timer*/
					printk("timer start\n");
					armv7_request_irq();
					armv7_pmnc_start();
					// TBD
					// set edge node's time slot
					gState->tdma_info.id = 0;
					gState->tdma_info.timeslot[0] = 0;
					gState->tdma_info.timeslot[1] = -1;
					gState->tdma_info.total_slot = TDMA_NODE_NUMBERS;	 
					break;
				}
            }
			
			/*uart_receive_len = recv_char(ctmp);
			//printk("uart_receive_len=%d\n", uart_receive_len);
			if(uart_receive_len > 0)
				printk("uart_receive=%s\n", ctmp);*/
			
			if(uart_flag) {
				uart_flag = 0;
				for(i=0;i<ETH_FRAME_LEN-2*ETH_MAC_LEN;i++) {
					timeout_buf[i+2*ETH_MAC_LEN] = 'A'+(i%26);
				}
			} else {
				for(i=0;i<ETH_FRAME_LEN-2*ETH_MAC_LEN;i++) {
					timeout_buf[i+2*ETH_MAC_LEN] = i;
				}
			}
			buf_send = timeout_buf;
			gState->signal_level += SIGNAL_STEP_VALUE;
			if(gState->signal_level >100) {				  
				gState->signal_level = START_SIGNAL_PERCENTAGE;
			}
			//show_neighbor_mac_rssi(gState );		
			send_cmm_packet(gState, NULL, buf_send, ETH_FRAME_LEN); // send boradcast out at initialization			
			//send_cmm_trigger_packet(gState , NULL ,buf_send, ETH_FRAME_LEN); // send boradcast out at initialization
			OSCancelTimer(&gState->TimeoutTimer, &cancel);
			//OSInitTimer(gState, &gState->TimeoutTimer,	GET_TIMER_FUNCTION(TimeoutExec), gState, FALSE);	// restart timeout timer
			OSModTimer(&gState->TimeoutTimer, BROADCAST_DURATION);
            break;
#endif
			// Increase power level and resend again
			if( gState->retry++ >= BROADCAST_RETRY_TIME ) {
				gState->signal_level += SIGNAL_STEP_VALUE;
				if(gState->signal_level >100) {				  
					gState->signal_level = START_SIGNAL_PERCENTAGE;
					gState->scan_step++;
				}
				gState->retry = 0;
			}
			if(gState->scan_step >= SIGNAL_CHANGE_TIME) { // Send the broadcast right to next node  
				gState->scan_step = 0;
				update_neighbor_list_rssi(gState ); 		  // update RSSI signal and sort neighbor list
				ret = decide_next_master_node(gState);
				if(ret) {					
				  	DBGPRINT(DEBUG_ALL,("Transfer search job to next node, NTBR_STATE_ASSIGN_MASTER\n"));
					OSCancelTimer( &gState->TimeoutTimer, &cancel);
					OSSetTimer(&gState->TimeoutTimer, BROADCAST_MASTER_TRANSIT_TIME);		  
					gState->dev_state = NTBR_STATE_ASSIGN_MASTER;
#ifdef _NO_KMALLOC
					buf_send = timeout_buf;
#else						  
					buf_send = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
					if (!buf_send) {
						DBGPRINT(DEBUG_ERROR, ("%s: %d, malloc fail\n", __FUNCTION__, __LINE__));
						return FAILED;
					}
#endif					  
					memset(buf_send, 0,ETH_FRAME_LEN);
					// send transfer job indication			  
					send_cmm_assign_broadcast_node_packet(gState , ((struct dllist *)(gState->forward_node))->mac, buf_send, ETH_FRAME_LEN);
#ifndef _NO_KMALLOC					  
					kfree(buf_send);
#endif
					break;
				}else {
					if (gState->flag == LEFT_EDGE_NODE) {
						OSCancelTimer( &gState->TimeoutTimer, &cancel);
						OSSetTimer(&gState->TimeoutTimer, BROADCAST_MASTER_TRANSIT_TIME); 						  
					}else if ( choose_node_near_left_edge(gState)) {
						gState->dev_state = NTBR_STATE_IFNO_CHANGE;
						gState->sub_state = NTBR_SUBSTATE_RECEIVED_FORWARD;
						memset(&(gState->forward_addr), 0, sizeof(FORWARD_ADDR_INFO));
						OSCancelTimer( &gState->TimeoutTimer, &cancel);
						OSSetTimer(&gState->TimeoutTimer, NEIGHBOR_TABLE_SENDBACK_TIME);						
#ifdef _NO_KMALLOC
					    buf_send = timeout_buf;
#else	
						buf_send = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
						if (!buf_send) {
							DBGPRINT(DEBUG_ERROR, ("%s: %d, malloc fail\n", __FUNCTION__, __LINE__));
							return FAILED;
						}
#endif						  
						memset(buf_send, 0,ETH_FRAME_LEN);
						// Start sending neighbor table back to the left edge node
						send_cmm_neighbor_info_packet(gState , ((struct dllist *)(gState->backward_node))->mac, buf_send, ETH_FRAME_LEN, NULL, 0);
#ifndef _NO_KMALLOC						   
						kfree(buf_send);
#endif
					}else {
						DBGPRINT(DEBUG_ERROR, ("%s: %d, NTBR_SUBSTATE_READY_FORWARD fail\n", __FUNCTION__, __LINE__));
						gState->dev_state = NTBR_STATE_INIT; // back to init ???
					}
					break;
					
				}
			}
#ifdef _NO_KMALLOC
 			buf_send = timeout_buf;
#else				  
			buf_send = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);		  
			if (!buf_send) {
				DBGPRINT(DEBUG_ALL,("malloc fail!!\n"));
				return;
			}
#endif			  
			send_cmm_trigger_packet(gState , NULL ,buf_send, ETH_FRAME_LEN); // send boradcast out at initialization
#ifndef _NO_KMALLOC						   			  
			kfree(buf_send);
#endif
			OSCancelTimer(&gState->TimeoutTimer, &cancel);
			OSModTimer(&gState->TimeoutTimer, BROADCAST_DURATION); 
			break;
		}	  
	    case NTBR_STATE_INIT:
		{
			OSCancelTimer(&gState->TimeoutTimer, &cancel);
			OSModTimer(&gState->TimeoutTimer, BROADCAST_DURATION);
#if JUST_TDMA==1
			if( gState->retry++ >= 10 ) {
				gState->retry = 0;
				gState->dev_state = NTBR_STATE_HELO;
			}
#endif		  
			break;
		}
	    case NTBR_STATE_ASSIGN_MASTER:
		{
			if(gState->retry++<=1000){
				OSCancelTimer( &gState->TimeoutTimer, &cancel);
				OSSetTimer(&gState->TimeoutTimer, BROADCAST_MASTER_TRANSIT_TIME);
#ifdef _NO_KMALLOC
				buf_send = timeout_buf;
#else				
				buf_send = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
				if (!buf_send) {
					DBGPRINT(DEBUG_ERROR, ("%s: %d, malloc fail\n", __FUNCTION__, __LINE__));
					return FAILED;
				}
#endif			  
				memset(buf_send, 0,ETH_FRAME_LEN);
				DBGPRINT(DEBUG_ALL,("Resend assign master packet to next node\n"));			
				// send transfer job indication 			
				send_cmm_assign_broadcast_node_packet(gState , ((struct dllist *)(gState->forward_node))->mac, buf_send, ETH_FRAME_LEN);	
#ifndef _NO_KMALLOC			  
				kfree(buf_send);	  
#endif
				break;
			}		
			DBGPRINT(DEBUG_ERROR,("Transfer search job to next node buf failed, why ??\n"));
			gState->retry = 0;
			gState->dev_state = NTBR_STATE_IFNO_CHANGE;
			OSCancelTimer( &gState->TimeoutTimer, &cancel);
			OSSetTimer(&gState->TimeoutTimer, BROADCAST_MASTER_MONITOR_TIME); 		
			break;
		} 
		case NTBR_STATE_IFNO_CHANGE:
		{		
			switch(gState->sub_state) {
				case NTBR_SUBSTATE_READY_FORWARD:
				{
					DBGPRINT(DEBUG_ALL,("NTBR_SUBSTATE_READY_FORWARD, enter init state!!, jiffies=0x%x\n", jiffies ));
					gState->dev_state = NTBR_STATE_INIT;
					break;
				}
				case NTBR_SUBSTATE_RECEIVED_FORWARD:
				{
					if(gState->retry++ >4){
						// retry but doesn't receive ack, back to the ready forwarding state
						gState->retry = 0;
						gState->sub_state = NTBR_SUBSTATE_READY_FORWARD;					
						OSCancelTimer( &gState->TimeoutTimer, &cancel);
						OSSetTimer(&gState->TimeoutTimer, INFO_CHANGE_TIME);
						break;
					}
					// resend neighbor list again and waiting for ack packet
					OSCancelTimer( &gState->TimeoutTimer, &cancel);
					OSSetTimer(&gState->TimeoutTimer, NEIGHBOR_TABLE_SENDBACK_TIME);
#ifdef _NO_KMALLOC
					buf_send = timeout_buf;
#else								
					buf_send = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
					if (!buf_send) {
						DBGPRINT(DEBUG_ERROR, ("%s: %d, malloc fail\n", __FUNCTION__, __LINE__));
						return FAILED;
					}
#endif				
					memset(buf_send, 0,ETH_FRAME_LEN);
				
					// Start sending neighbor table back to the left edge node
					if (gState->forward_addr.len > 0){
						send_cmm_neighbor_info_packet(gState , ((struct dllist *)(gState->backward_node))->mac, buf_send, ETH_FRAME_LEN, gState->forward_addr.buffer,	gState->forward_addr.len );
					}else {
						send_cmm_neighbor_info_packet(gState , ((struct dllist *)(gState->backward_node))->mac, buf_send, ETH_FRAME_LEN, NULL, 0);
					}
#ifndef _NO_KMALLOC				
					kfree(buf_send);
#endif
					break;
				}
				case NTBR_SUBSTATE_FORWARD_DONE: // waitng for TDMA command
				{
					gState->sub_state = NTBR_SUBSTATE_READY_FORWARD;
					OSCancelTimer( &gState->TimeoutTimer, &cancel);
					OSSetTimer(&gState->TimeoutTimer, INFO_CHANGE_TIME);						
					break;
				}
				defualt:
					break;

			}
			break;
		} 	  
		case NTBR_STATE_STEADY:
	 	{
			switch(gState->sub_state) {
				case NTBR_SUBSTATE_READY_FORWARD:
					// do not send tdma assign ack because it might cause loop time counting error, if ack has been lost, just waiting for tdma assign command
					break;
				case NTBR_SUBSTATE_RECEIVED_FORWARD:
					break;
				case NTBR_SUBSTATE_FORWARD_DONE: // resend TDMA info to next node again
				{
					OSCancelTimer( &gState->TimeoutTimer, &cancel);
					OSSetTimer(&gState->TimeoutTimer, NEIGHBOR_TABLE_SENDBACK_TIME);
#ifdef _NO_KMALLOC
		 			buf_send = timeout_buf;
#else				  
					buf_send = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
					if (!buf_send) {
						DBGPRINT(DEBUG_ERROR, ("%s: %d, malloc fail\n", __FUNCTION__, __LINE__));
						return FAILED;
					}
#endif				  
					memset(buf_send, 0,ETH_FRAME_LEN);				  
					send_cmm_packet_tdmaslot(gState , gState->sequence, ((struct dllist *)(gState->forward_node))->mac, buf_send, ETH_FRAME_LEN); 			  
#ifndef _NO_KMALLOC								  
					kfree(buf_send);
#endif
					break;
				}
				default:
					break;		
			}
			break;			
	 	} 	
		case NTBR_STATE_TDMA_INIT:
	    {
			switch(gState->sub_state) {
				case NTBR_SUBSTATE_READY_FORWARD: // resend TDMA info to next node again
				{
					OSCancelTimer( &gState->TimeoutTimer, &cancel);
					OSSetTimer(&gState->TimeoutTimer, NEIGHBOR_TABLE_SENDBACK_TIME);
#ifdef _NO_KMALLOC
					buf_send = timeout_buf;
#else				 
					buf_send = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
					if (!buf_send) {
						DBGPRINT(DEBUG_ERROR, ("%s: %d, malloc fail\n", __FUNCTION__, __LINE__));
						return FAILED;
					}
#endif				 
					memset(buf_send, 0,ETH_FRAME_LEN); 				 
					send_cmm_packet_tdma_start(gState , gState->sequence, ((struct dllist *)(gState->forward_node))->mac, buf_send, ETH_FRAME_LEN);				 
#ifndef _NO_KMALLOC								 
					kfree(buf_send);			   
#endif
					break;
				}
				case NTBR_SUBSTATE_FORWARD_DONE:
				case NTBR_SUBSTATE_RECEIVED_FORWARD:
				default:
					break;
		   
			}
			break;
		}    
	}
}

BUILD_TIMER_FUNCTION(MlmePeriodicExec);
BUILD_TIMER_FUNCTION(TimeoutExec);

struct dllist * make_dev_list(DEV_MAIN_DISCRIPTION *pDesc, unsigned char *source_mac, int level, int flag)
{
	struct dllist *lnode;
	struct dllist *prev;
	int ret;
	lnode = (struct dllist *) kmalloc(sizeof(struct dllist), GFP_KERNEL);
	if (!lnode){
		DBGPRINT(DEBUG_ERROR, ("%s: malloc fail\n", __FUNCTION__));
		return NULL;
	}
    lnode->number = 0;
	lnode->signal = level;
	lnode->table_len =0;
	lnode->table = NULL;
	memcpy( lnode->mac, source_mac,ETH_MAC_LEN);

	OS_SEM_LOCK(&pDesc->list_lock);

    if (flag == LEFT_EDGE_NODE){
		lnode->role = LEFT_EDGE_NODE;
		if ( !pDesc->head ) {
			append_node(&(pDesc->head), &(pDesc->tail),lnode);
		}
		else{
			 insert_before_node(&(pDesc->head), lnode, pDesc->head);
		}
    }
	else if (flag == RIGHT_EDGE_NODE) {
		lnode->role = RIGHT_EDGE_NODE;
		append_node(&(pDesc->head), &(pDesc->tail),lnode);
	}
	else if(level!=0){
		lnode->role = NORMAL_NODE;
#if 0		// TBD
		int signal_level=0;
		unsigned char sys_cmd[MAX_CMD_LEN];
		snprintf(sys_cmd, MAX_CMD_LEN, "ifconfig %s > /dev/null 2>&1", "ra0");
		ret = system(sys_cmd);
		if (!ret) { // ifno is exist
			char *buf_tmp;
			buf_tmp = run_command("iwpriv ra0 adhocEntry");
			if (!buf_tmp) {
				signal_level = 0;
			}
			//parser signal level
			signal_level = parser_rssi(source_mac,buf_tmp);
		}				
		prev = get_greater_than_signal_node(signal_level);
		if (!prev)
	        append_node(lnode);
		else
	        insert_node(lnode, prev);
#else
	append_node(&(pDesc->head), &(pDesc->tail),lnode); // compare the neighbor list after table changed
#endif		
	}else {
		lnode->role = NORMAL_NODE;
        append_node(&(pDesc->head), &(pDesc->tail),lnode); // compare the neighbor list after table changed
	}
	OS_SEM_UNLOCK(&pDesc->list_lock);
    return lnode;
}

int build_neighbor_list_data(DEV_MAIN_DISCRIPTION *pDesc, unsigned char *buf, int maximum_len )
{
    struct dllist *lnode;
	char *p;
	short *u16P;
	p = buf;
#if NCTU_SIGNAL_SEARCH == 1
    memcpy(buf,pDesc->src_mac, ETH_MAC_LEN);
    p = buf + ETH_MAC_LEN + sizeof(unsigned short);
#endif	
	for ( lnode = pDesc->head; lnode; lnode = lnode->next ) {
		memcpy(p,lnode->mac, ETH_MAC_LEN);
		p += ETH_MAC_LEN;
		u16P = (short *)p;
		*u16P = ntohs((short)lnode->signal);
		p+=sizeof(short);		
	}
#if NCTU_SIGNAL_SEARCH == 1
	u16P = (short *)(buf + ETH_MAC_LEN);
	*u16P = ntohs((short)((unsigned int )p - (unsigned int )buf - 8 ));
#endif	
	
	return ((unsigned int )p - (unsigned int )buf);
}

#if NCTU_SIGNAL_SEARCH == 1

int show_neighbor_list_data( char *buf_in, int total_len )
{
	char *p,*buf;
	short *u16P;
	int   len, tmp_len;
	short tmp_signal;

	for (buf=buf_in; (unsigned int)buf - (unsigned int)buf_in < total_len; ) {	

		u16P =buf+ETH_MAC_LEN ;
		len = ntohs(*u16P);

		printk("=========== show_neighbor_list_data ===================\n");
		printk("=========== MAC=%02x:%02x:%02x:%02x:%02x:%02x, len=%d\n", PRINT_MAC(buf), len );
		printk("================ Information =========================\n");

	    for ( p = buf + ETH_MAC_LEN + sizeof(short), tmp_len=0; tmp_len < len ; tmp_len += ETH_MAC_LEN + sizeof(short) ) {
			u16P =p+ETH_MAC_LEN ;
			tmp_signal = ntohs(*u16P);
			printk("=========== MAC=%02x:%02x:%02x:%02x:%02x:%02x, signal=%d\n", PRINT_MAC(p), tmp_signal );
			p += ETH_MAC_LEN + sizeof(short);
	    }
		buf += ETH_MAC_LEN + sizeof(short) + len;
	}
}

int set_neighbor_list_table( DEV_MAIN_DISCRIPTION *pDesc, char *buf_in, int total_len )
{
	char *p,*buf;
	short *u16P;
	int   len, tmp_len;
	short tmp_signal;
	struct dllist *lnode;

	for (buf=buf_in; (unsigned int)buf - (unsigned int)buf_in < total_len; ) {	

		u16P =buf+ETH_MAC_LEN ;
		len = ntohs(*u16P);

		lnode = find_node(pDesc->head, NULL, buf );		
		if (!lnode){
			lnode = make_dev_list(pDesc, buf, 0, NORMAL_NODE );
			if(!lnode){
				DBGPRINT(DEBUG_ERROR,("make_dev_list error!\n"));
				return FAILED;
			}												
			lnode->txpower = 0; // the smallest power
		}
		lnode->timestamp = jiffies;
		lnode->left = 0;
		make_neighbor_table_page(pDesc, buf, buf + ETH_MAC_LEN + sizeof(short), len);
		buf += ETH_MAC_LEN + sizeof(short) + len;
	}
	return SCUESS;
}

#endif
void swap_list_node(struct dllist *lnode1, struct dllist *lnode2) 
{
	struct dllist tmp;

	tmp.next = lnode1->next;
	tmp.prev = lnode1->prev;
	if(lnode1->prev)
      ((struct dllist *)lnode1->prev)->next = lnode1->next;
	if(lnode2->next)
	  ((struct dllist *)lnode2->next)->prev = lnode2->prev;
	lnode1->prev = lnode1->next;
	lnode1->next = lnode2->next;
	lnode2->next = lnode2->prev;
	lnode2->prev = tmp.prev;
}	

int fill_tdma_slot_payload(DEV_MAIN_DISCRIPTION *pDesc, unsigned char *buf, int maximum_len )
{
	struct dllist *lnode;
	char *p;
	short *u16P;
	unsigned int *u32P;
	unsigned char idx;
    /*
         |type | len   |        payload                             |    type |  len   |   payload                            |     
                             mac (6) | id |total slot | slot                                   mac (6) | id | slot | slot


                 if ten nodes, each has three slots, the total lengths are 14*10 = 140 bytes

    */
#if TDMA_DEBUUG_LOG == 1
    u16P = (short *)buf; 
	*u16P = ntohs((short)NT_MSG_SUBTYPE_TEST_COUNT);	// type
	u16P++;
	*u16P = ntohs((short)8);	 // len 
	u16P++; 
	u32P = u16P;
	*u32P = ntohl(pDesc->test_count);
	p = buf + 8;
#else
    p = buf;
#endif
	for (idx=1, lnode = pDesc->head; lnode; lnode = lnode->next, idx++ ) {
		u16P = (short *)p;
	    *u16P = ntohs((short)NT_MSG_SUBTYPE_TDMA);  // type
		u16P++;
        *u16P = ntohs((short)10);   // len 
		u16P++;		
		memcpy(u16P, lnode->mac, ETH_MAC_LEN);  // MAC address
		p += 10;
		*p = idx; // node ID, from 1,2,3 for this example
		*(++p) = TDMA_NODE_NUMBERS;  // total time slot number is 3 in this example
		*(++p) = idx;  // time slot for this node.
		*(++p) = -1;   // no next time slot
		p++;
		if ( ((unsigned int )p - (unsigned int )buf) >= maximum_len ) {
			DBGPRINT(DEBUG_ERROR,("fill_tdma_slot_payload: payload length is too long!!\n"));
            break;
		}
	}
	return ((unsigned int )p - (unsigned int )buf);	
}

int update_neighbor_list_rssi(DEV_MAIN_DISCRIPTION *pDesc )
{
    struct dllist *lnode;
	char *buf;
	int i,j,first_num;

	buf = kmalloc(4096, GFP_KERNEL);
	if(buf){
		Uplayer_Show_MacTable_Proc(buf);
		printk("%s",buf);
	}
	OS_SEM_LOCK(&pDesc->list_lock);	
#if 1	
	
	for ( lnode = pDesc->head; lnode; lnode = lnode->next ) {

		lnode->signal = parser_rssi(lnode->mac, buf);
		printk("###parser_rssi: [%d] MAC=%02x:%02x:%02x:%02x:%02x:%02x, signal=%d\n", i++, PRINT_MAC(lnode->mac), lnode->signal );
			
	}	

    for ( lnode = pDesc->head, first_num=0; lnode; lnode = lnode->next ) {
		if(lnode->next) {
			if( lnode->signal < ((struct dllist *)(lnode->next))->signal ) {
				if(lnode == pDesc->head)
					pDesc->head = lnode->next;
				swap_list_node(lnode, lnode->next) ;
			}
 
		}else {
		    pDesc->tail = lnode; // update last tail node
		}
		first_num++;
    }
	for ( j=first_num - 1 ; j > 1; j--) {
	    for ( lnode = pDesc->head, i=0; lnode && i < j; lnode = lnode->next, i++ ) {
			if(lnode->next) {
				if( lnode->signal < ((struct dllist *)(lnode->next))->signal ) {
					if(lnode == pDesc->head)
						pDesc->head = lnode->next;
					swap_list_node(lnode, lnode->next) ;
				}
	 
			}
	    }
	}    
#endif
	OS_SEM_UNLOCK(&pDesc->list_lock);
    kfree(buf);
	return 0;
}

int make_neighbor_table_page_by_mac( DEV_MAIN_DISCRIPTION *pDesc, unsigned char *src_mac, unsigned char *neighbor_mac, short rssi_level)
{
    // TBD
    struct dllist *lnode;
	for ( lnode = pDesc->head; lnode; lnode = lnode->next ) {
		if (!memcmp(lnode->mac,src_mac, ETH_MAC_LEN)) {
			if(lnode->table !=NULL) {
			}
			else{
				
			}
			break;
			
		}
  
	}
}	

int refreash_tdma_table( DEV_MAIN_DISCRIPTION *pDesc, unsigned char *src_mac, unsigned char *buffer, int len, unsigned int seq_num, unsigned int time_stamp )
{
    struct dllist *lnode;
    char *p,*buf;
    short *u16P;
    unsigned int *u32P;
    unsigned char idx;
    int total_len;
    buf = buffer;
    /*
         |type | len   |        payload                             |    type |  len   |   payload                            |     
							mac (6) | id |total slot | slot | slot						 mac (6) | id | total slot | slot | slot

                 if ten nodes, each has three slots, the total lengths are 14*10 = 140 bytes

    */
    // backup TDMA forwarding information
    //printk("refreash_tdma_table: rx len=%d\r\n", len);
    memcpy(pDesc->buffer.data, buf, len );
    pDesc->buffer.len = len;
	while(1) {
	    u16P = (short *)buf;
#if TDMA_DEBUUG_LOG == 1
	    if( NT_MSG_SUBTYPE_TEST_COUNT == ntohs(*u16P)) {  // type
			u32P = (buf+4);
		    pDesc->test_count = ntohl(*u32P);		   
		    buf+=8;
		    continue;
	    }else
#endif   
	    if ( NT_MSG_SUBTYPE_TDMA == ntohs(*u16P)) {  // type
			u16P++;
		    total_len = ntohs(*u16P);   // len 
		    u16P++;
	    }	   
	    for ( p = u16P, lnode = pDesc->head; lnode; lnode = lnode->next ) {
		    if( SAME_MAC( p, pDesc->src_mac)) { 
				p += 6;
			    pDesc->tdma_info.id = *p;
			    p++;
	            pDesc->tdma_info.total_slot  = *p;
			    p ++;			   
			    pDesc->tdma_info.timeslot[0] = *p;
			    p++;
			    pDesc->tdma_info.timeslot[1] = *p;
			    pDesc->tdma_info.timeslot[2] = -1;
			    break;
		    }else if ( pDesc->forward_node && SAME_MAC(((struct dllist *)(pDesc->forward_node))->mac, p) ) {
		   	    p += 6;
			    ((struct dllist *)(pDesc->forward_node))->id = *p;
			    //printk("GD:%d ",  ((struct dllist *)(pDesc->forward_node))->id);
			    break;
		    }else if ( pDesc->backward_node && SAME_MAC(((struct dllist *)(pDesc->backward_node))->mac, p) ) { 
				p += 6;
			    ((struct dllist *)(pDesc->backward_node))->id = *p;
			    //printk("GDB:%d ",  ((struct dllist *)(pDesc->backward_node))->id);
			    break;		   
		    }	   
	    }
	   //printk("refreash_tdma_table: rx total_len=%d\r\n", total_len);
	   buf = buf + sizeof(unsigned short) + sizeof(unsigned short) + total_len;
	   if ( ((unsigned int )buf - (unsigned int )buffer) >= len ) {
		   break;
	   }
    }
    //printk("my slot:%d, total=%d\r\n", pDesc->tdma_info.timeslot[0], pDesc->tdma_info.total_slot);
    return SCUESS;
}

#define MAX_NEIGHBOR_SIZE   (8*101)
int make_neighbor_table_page( DEV_MAIN_DISCRIPTION *pDesc,  unsigned char *src_mac, unsigned char *table, int len )
{
    struct dllist *lnode;
	for ( lnode = pDesc->head; lnode; lnode = lnode->next ) {
		if (!memcmp(lnode->mac,src_mac, ETH_MAC_LEN )) {
			if(lnode->table ==NULL) {
				lnode->table = kmalloc(MAX_NEIGHBOR_SIZE, GFP_KERNEL);
				if(!lnode->table) {
					DBGPRINT(DEBUG_ERROR, ("%s: malloc fail\n", __FUNCTION__));
				}
			}
			memset(lnode->table,0, MAX_NEIGHBOR_SIZE);
			memcpy(lnode->table, table,len);
			lnode->table_len = len;
			break;			
		}  
	}
	return 0;
}

void Display_System_neighbor_Table( DEV_MAIN_DISCRIPTION *pDesc )
{
    struct dllist *lnode;
	int len;

	printk("\r\n********* neighbor table list, Prev Mac = %02x:%02x:%02x:%02x:%02x:%02x **********\r\n", PRINT_MAC(pDesc->left_mac));
	for ( lnode = pDesc->head; lnode; lnode = lnode->next ) {
		if(lnode->role == NORMAL_NODE )
			printk(" %02x:%02x:%02x:%02x:%02x:%02x  RSSI=%d, %s", PRINT_MAC(lnode->mac), lnode->signal, "Normal Node" );
		else if ( lnode->role == LEFT_EDGE_NODE )
			printk(" %02x:%02x:%02x:%02x:%02x:%02x  RSSI=%d, %s", PRINT_MAC(lnode->mac), lnode->signal, "Left Node" );
		else
			printk(" %02x:%02x:%02x:%02x:%02x:%02x  RSSI=%d, %s", PRINT_MAC(lnode->mac), lnode->signal, "Right Node" );

        if(lnode->table_len !=0) {
			for( len=0;len<lnode->table_len; len +=8) {
				short signal = (int)ntohs(*((short *)(lnode->table+6)));
				printk("    >> %02x:%02x:%02x:%02x:%02x:%02x,   %x-%x ,signal=%x, RSSI=%d", PRINT_MAC((lnode->table+len)), lnode->table[6],lnode->table[7],signal, signal );			
			}
		}
	}
}

void clean_driver_macs( DEV_MAIN_DISCRIPTION *pDesc )
{
    struct dllist *lnode;
	int len;
	char tmp[30];
	
	printk("\r\n********* clean driver macs **********\r\n");
	for ( lnode = pDesc->head; lnode; lnode = lnode->next ) {
		
		sprintf(tmp,"%02x:%02x:%02x:%02x:%02x:%02x", lnode->mac[0],lnode->mac[1],lnode->mac[2], lnode->mac[3], lnode->mac[4], lnode->mac[5]);
		printk("call Uplayer_layer_Remove_PeerN_Proc..........%s\n",tmp );
		Uplayer_layer_Remove_PeerN_Proc(tmp);
	}
}
int decide_routing_list(DEV_MAIN_DISCRIPTION *pDesc)
{
    struct dllist *lnode;
	int len;
	int find =0;

    if (pDesc->flag==LEFT_EDGE_NODE) {
		pDesc->forward_node = pDesc->head;
		printk("##%s: Find next MAC :%02x:%02x:%02x:%02x:%02x:%02x\r\n", __FUNCTION__, PRINT_MAC((struct dllist *)(pDesc->forward_node)->mac));		
    }	
	else {
		// Depends on left edge node's neighbor table to decide next node.
		for ( lnode = pDesc->head; lnode; lnode = lnode->next ) {
	        if ( lnode->role == LEFT_EDGE_NODE ) {			
				if(lnode->table_len !=0 && lnode->table )
				for( len=0;len<lnode->table_len; len +=8) {
					 if( SAME_MAC((lnode->table+len),pDesc->src_mac)){ 
					 	len +=8;
					 	if(len<lnode->table_len)
					 	    find =1;
						else
						    find =0;
						break;
					 }
				}
				break;
	        }
		}
		if(find) {
		    pDesc->forward_node = find_node(pDesc->head, NULL,lnode->table+len);
			printk("##%s: Find next MAC :%02x:%02x:%02x:%02x:%02x:%02x\r\n", __FUNCTION__, PRINT_MAC((struct dllist *)(pDesc->forward_node)->mac));
		}
		else {
			pDesc->forward_node =NULL;
		}
	}
	if(pDesc->forward_node)
		return 1;
	else
		return 0;
}
#if NCTU_SIGNAL_SEARCH == 1
int decide_next_master_node(DEV_MAIN_DISCRIPTION *pDesc) // find the right site node which includes maximum power level
{
    struct dllist *lnode;
	pDesc->forward_node;
	int len;
	int find =0;
	pDesc->forward_node = NULL;

    if (pDesc->flag==LEFT_EDGE_NODE) {
		pDesc->forward_node = pDesc->head;		
		if(pDesc->forward_node)
		((struct dllist *)(pDesc->forward_node))->id = 1;
    }	
	else {
		for ( lnode = pDesc->head; lnode; lnode = lnode->next ) {
	        if ( !lnode->left ) {
				if(!SAME_MAC(pDesc->left_mac,lnode->mac)) {
					if(pDesc->forward_node) {
						if (((struct dllist *)(pDesc->forward_node))->signal < lnode->signal)
							pDesc->forward_node = lnode;
					}else {
					    pDesc->forward_node = lnode;
					}				
				}
	        } 
		}
	}
	if(pDesc->forward_node) {
		printk("##%s: Find next MAC :%02x:%02x:%02x:%02x:%02x:%02x\r\n", __FUNCTION__, PRINT_MAC((struct dllist *)(pDesc->forward_node)->mac));
		return 1;
	}
	else
		return 0;	
}

int choose_node_near_left_edge(DEV_MAIN_DISCRIPTION *pDesc) // find a left site node which near to left edge node
{
	struct dllist *lnode;
	struct dllist *nearest;
	int len;
	int find =0;
	pDesc->backward_node = NULL;
	// Depends on left edge node's neighbor table to decide next node.
	for ( lnode = pDesc->head; lnode; lnode = lnode->next ) {	
#if 0	
		if(lnode->left) {  // choose a left node which power is lowerest , it means that it would be near to left edge node
			if(!pDesc->backward_node) {
				pDesc->backward_node = lnode;
			}
			else if ( (struct dllist *)(pDesc->backward_node)->signal < lnode->signal) {
                pDesc->backward_node = lnode;
			}
		}		
#else
        if(SAME_MAC(pDesc->left_mac,lnode->mac)){
			pDesc->backward_node = lnode;
        }
#endif
	}
	if(pDesc->backward_node) {
		printk("##%s: Find previous MAC :%02x:%02x:%02x:%02x:%02x:%02x\r\n", __FUNCTION__, PRINT_MAC((struct dllist *)(pDesc->backward_node)->mac));
		return 1;
	}
	else
		return 0;
}

#endif

void refreash_tdma_timer(DEV_MAIN_DISCRIPTION *pDesc, unsigned int tdma_count, unsigned int time_stamp, unsigned int delay_ticks )
{
	// TBD
	pDesc->tdma_info.tick_count = time_stamp;
	pDesc->tdma_info.tick_pmmc_value = tdma_count;
}


void correct_tdma_timer(DEV_MAIN_DISCRIPTION *pDesc,  int diff_pmmc,  int diff_tick)
{
    unsigned int tmp_tick, tmp_pmmc;
	unsigned tmp;
    unsigned int val = 0xffffffff - TDMA_PMMC_DEF;
#if PRINTK_BUF==1 && 0
    if(printk_msg_buffer_idx<PRINTK_DBG_BUF_MAX-10) {
	   printk_msg_buffer_idx += sprintf(printk_msg_buffer + printk_msg_buffer_idx, "<%x, %x>:%x\r\n" , diff_pmmc, diff_tick , pDesc->cycle);
    }
#endif	
	if(diff_tick==0) { // don't save big value
	    diff_pmmc_acumulation += diff_pmmc;
		diff_pmmc_num++;
	}
	tmp = (diff_pmmc + diff_tick*val)>>1;
	tmp_tick = tmp/val;		
	tmp_pmmc = tmp % val;
	pDesc->tdma_info.tick_count += tmp_tick;
	if( 0xffffffff -  pDesc->tdma_info.tick_pmmc_value < tmp_pmmc ) {
		pDesc->tdma_info.tick_pmmc_value = TDMA_PMMC_DEF +  tmp_pmmc - (0xffffffff - pDesc->tdma_info.tick_pmmc_value);
		pDesc->tdma_info.tick_count++;
	}else	
	    pDesc->tdma_info.tick_pmmc_value += tmp_pmmc;
}	


int process_handler(DEV_MAIN_DISCRIPTION *pDesc ,char *buffer, int length) // copy form usp
{
	struct ethhdr *eh= (struct ethhdr *)buffer;
	char tmp[100];
	struct dllist *pEntry;
	void* new_buffer;
	int first_recv =0;
	int reply =0;
	unsigned char cancel;

    switch(pDesc->dev_state) {
	    case NTBR_STATE_INIT:
		{
			struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;
			if (ntohs(eh->h_proto) == LOCATION_COMM_PROTO && ntohs(hdr->op_code) == NT_MSG_HELLO) {
				if( SAME_MAC(eh->h_dest,pDesc->src_mac)){ // Peer node reply 																
					}else if (IS_BROADCAST(eh->h_dest)){
						DBGPRINT(DEBUG_WARN,("Receive a broadcast hello\n"));
						reply =1;
					}else{
						DBGPRINT(DEBUG_ERROR,("Receive a packet but it's not belong to me\n") ); // go out the function
						break;
					}
#if JUST_TDMA ==1
				sprintf(tmp,"%02x:%02x:%02x:%02x:%02x:%02x,%d,%d", eh->h_source[0],eh->h_source[1],eh->h_source[2], eh->h_source[3], eh->h_source[4], eh->h_source[5], 2,108);
				DBGPRINT(DEBUG_ERROR,("call wireless driver Uplayer_layer_Set_PeerN_Proc, MAC=%s\n",tmp) );
				Uplayer_layer_Set_PeerN_Proc(tmp);
				pDesc->dev_state = NTBR_STATE_HELO;
				break;
#endif								
				pEntry = find_node(pDesc->head, NULL,eh->h_source);
				if (!pEntry){
					pEntry = make_dev_list(pDesc,eh->h_source, 0, hdr->flag ); // defaule signal power level	
					if(!pEntry){
						DBGPRINT(DEBUG_ERROR,("Memory allocation error!\n"));
						return FAILED;
					}												
					sprintf(tmp,"%02x:%02x:%02x:%02x:%02x:%02x,%d,%d", eh->h_source[0],eh->h_source[1],eh->h_source[2], eh->h_source[3], eh->h_source[4], eh->h_source[5], 2,108);
					DBGPRINT(DEBUG_ERROR,("call wireless driver Uplayer_layer_Set_PeerN_Proc, cmd=%s\n",tmp) );
					Uplayer_layer_Set_PeerN_Proc(tmp);
					pEntry->txpower = ntohs(hdr->txpower); // the smallest power
				}
				pEntry->timestamp = jiffies;
				pEntry->left = 1;
					
				if (reply) {						
					void* new_buffer;
#ifdef _NO_KMALLOC
                    new_buffer = process_buf;
#else						
					new_buffer = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
					if (!new_buffer) {
						DBGPRINT(DEBUG_ERROR, ("%s: malloc fail\n", __FUNCTION__));
						return FAILED;
					}
#endif						
					memset(new_buffer, 0,ETH_FRAME_LEN);
					//memcpy(new_buffer, eh->h_source, ETH_MAC_LEN);
					printk("send unicast hello...\n");
					send_cmm_packet_hello(pDesc, eh->h_source, new_buffer, ETH_FRAME_LEN); // unicast 
#ifndef _NO_KMALLOC						
					kfree(new_buffer);
#endif
				}
			}		
			else if (ntohs(eh->h_proto) == LOCATION_COMM_PROTO && ntohs(hdr->op_code) == NT_MSG_MASTER_ASSIGN) {
				void* new_buffer ;
				if(!SAME_MAC(eh->h_dest, pDesc->src_mac)) {
					break;
				}
#ifdef _NO_KMALLOC
				new_buffer = process_buf;
#else
				new_buffer = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
				if (!new_buffer) {
					DBGPRINT(DEBUG_ERROR, ("%s: malloc fail\n", __FUNCTION__));
					return FAILED;
				}
#endif					
				memset(new_buffer, 0,ETH_FRAME_LEN);
				DBGPRINT(DEBUG_ALL, ("%s: send NT_MSG_MASTER_ASSIGN_ACK back,  %02x:%02x:%02x:%02x:%02x:%02x\r\n", __FUNCTION__, PRINT_MAC(eh->h_source)));
				send_cmm_ack_packet(pDesc , eh->h_source, new_buffer, ETH_FRAME_LEN, NT_MSG_MASTER_ASSIGN_ACK, hdr->seq_num);
				memcpy(pDesc->left_mac, eh->h_source, ETH_FRAME_LEN );
#ifndef _NO_KMALLOC					
				kfree(new_buffer);		
#endif
				pDesc->dev_state = NTBR_STATE_HELO;
				pDesc->signal_level = START_SIGNAL_PERCENTAGE;
				pDesc->retry     = 0;
				pDesc->scan_step = 0;
				pDesc->sequence  = 0;					
#if MULTI_PKYS_PER_SLOT==1			
				pDesc->frames = 0;
#endif
#ifdef _NO_KMALLOC
				new_buffer = process_buf;
#else
				new_buffer = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
				if (!new_buffer) {
					DBGPRINT(DEBUG_ERROR, ("%s: malloc fail\n", __FUNCTION__));
					return FAILED;
				}
#endif
				memset(new_buffer, 0,ETH_FRAME_LEN);
				DBGPRINT(DEBUG_ALL, ("%s: send trigger and enter hello state\n", __FUNCTION__));					
				send_cmm_trigger_packet(pDesc , NULL ,new_buffer, ETH_FRAME_LEN); // send boradcast out at initialization
#ifndef _NO_KMALLOC
				kfree(new_buffer);
#endif
				OSCancelTimer( &pDesc->TimeoutTimer, &cancel);
				OSSetTimer(&pDesc->TimeoutTimer, BROADCAST_DURATION);
			}
			break;
		}
		case NTBR_STATE_HELO:
		{
			struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;
			void* new_buffer ;
			if ( ntohs(eh->h_proto) == LOCATION_COMM_PROTO && ntohs(hdr->op_code) == NT_MSG_HELLO) {
				if(SAME_MAC(eh->h_dest,pDesc->src_mac)){ // Peer node reply 
				}else if (IS_BROADCAST(eh->h_dest)){
					DBGPRINT(DEBUG_WARN,("Receive a broadcast hello packet ???\n"));
				}else{
					DBGPRINT(DEBUG_ERROR,("Receive a packet but it's not belong to me\n") ); // go out the function
					break;
				}
#if JUST_TDMA ==1
                sprintf(tmp,"%02x:%02x:%02x:%02x:%02x:%02x,%d,%d", eh->h_source[0],eh->h_source[1],eh->h_source[2], eh->h_source[3], eh->h_source[4], eh->h_source[5], 2,108);
                DBGPRINT(DEBUG_ERROR,("call wireless driver Uplayer_layer_Set_PeerN_Proc, MAC=%s\n",tmp) );
						
                Uplayer_layer_Set_PeerN_Proc(tmp);
				break;
#endif
				pEntry = find_node(pDesc->head, NULL,eh->h_source);
				if ( !pEntry ) {
					pEntry = make_dev_list(pDesc,eh->h_source, 0, hdr->flag ); // defaule signal power level	
					if(!pEntry){
						DBGPRINT(DEBUG_ERROR,("Memory allocation error!\n"));
						return FAILED;
					}												
					sprintf(tmp,"%02x:%02x:%02x:%02x:%02x:%02x,%d,%d", eh->h_source[0],eh->h_source[1],eh->h_source[2], eh->h_source[3], eh->h_source[4], eh->h_source[5], 2,108);
					DBGPRINT(DEBUG_ERROR,("call wireless driver Uplayer_layer_Set_PeerN_Proc, cmd=%s\n",tmp) );
					Uplayer_layer_Set_PeerN_Proc(tmp);
                    pEntry->txpower = pDesc->signal_level;
				}
				pEntry-> timestamp = jiffies;
				pEntry->left =0;
#if JUST_TDMA ==1					
            }else if (ntohs(eh->h_proto) == LOCATION_COMM_PROTO && ntohs(hdr->op_code) == NT_MSG_DATA) {
				// check the address
                if( hdr->dst_addr != 0xffff && hdr->dst_addr != pDesc->tdma_info.id ) { 
				    DBGPRINT(DEBUG_ERROR,("Not me.....pkt-dst=%d, tdma-info id=%d.....???\r\n", hdr->dst_addr, pDesc->tdma_info.id ));
					break;
				}
				OSCancelTimer( &pDesc->TimeoutTimer, &cancel);
				//OSSetTimer(&gState->TimeoutTimer, BROADCAST_MASTER_TRANSIT_TIME);
                if ( pDesc->flag != LEFT_EDGE_NODE ) {
                    time_slot_mode = hdr->hop_addr;
                }                 
				sprintf(tmp,"%02x:%02x:%02x:%02x:%02x:%02x,%d,%d", eh->h_source[0],eh->h_source[1],eh->h_source[2], eh->h_source[3], eh->h_source[4], eh->h_source[5], 2,108);
				//DBGPRINT(DEBUG_ERROR,("call wireless driver Uplayer_layer_Set_PeerN_Proc, MAC=%s\n",tmp) );
				Uplayer_layer_Set_PeerN_Proc(tmp);

                if(gState->forward_node) {
					sprintf(tmp,"%02x:%02x:%02x:%02x:%02x:%02x,%d,%d", ((struct dllist *)(gState->forward_node))->mac[0],((struct dllist *)(gState->forward_node))->mac[1],((struct dllist *)(gState->forward_node))->mac[2], ((struct dllist *)(gState->forward_node))->mac[3], ((struct dllist *)(gState->forward_node))->mac[4], ((struct dllist *)(gState->forward_node))->mac[5], 2,108);
					//DBGPRINT(DEBUG_ERROR,("call wireless driver Uplayer_layer_Set_PeerN_Proc, MAC=%s\n",tmp) );
					Uplayer_layer_Set_PeerN_Proc(tmp);
                }

				if( !gState->backward_node && hdr->src_addr == (fixed_id-1)) {												
					gState->backward_node = &backward_lnode;
					memcpy(((struct dllist *)(gState->backward_node))->mac, eh->h_source, 6 );
					gState->backward_node->id = fixed_id - 1;
					sprintf(tmp,"%02x:%02x:%02x:%02x:%02x:%02x,%d,%d", ((struct dllist *)(gState->backward_node))->mac[0],((struct dllist *)(gState->backward_node))->mac[1],((struct dllist *)(gState->backward_node))->mac[2], ((struct dllist *)(gState->backward_node))->mac[3], ((struct dllist *)(gState->backward_node))->mac[4], ((struct dllist *)(gState->backward_node))->mac[5], 2,108);
					//DBGPRINT(DEBUG_ERROR,("call wireless driver Uplayer_layer_Set_PeerN_Proc, MAC=%s\n",tmp) );
					Uplayer_layer_Set_PeerN_Proc(tmp);						
				}
				// clean forwarding buffer
				memset(&(pDesc->buffer), 0, sizeof(BUFF_DATA));
				memset(&(pDesc->tdma_status), 0, sizeof(TDMA_STATUS));
				memset(&(pDesc->tdma_count), 0, sizeof(TDMA_STATUS_TEMP));
				pDesc->sequence = 0;
#if MULTI_PKYS_PER_SLOT==1			
				pDesc->frames = 0;
#endif
				// send data in my time slot
				pDesc->dev_state = NTBR_STATE_TDMA;
				pDesc->sub_state = NTBR_SUBSTATE_FORWARD_DONE;
				//DBGPRINT(DEBUG_ALL,("Start TDMA state...\n"));
				pDesc->tdma_count.current_hz = HZ;
				pDesc->total_tick_number = 0;
				pDesc->buffer.len = 0;					
#if NCTU_WAVE==1
#ifdef _NO_KMALLOC
				new_buffer = process_buf;
#else
				new_buffer = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
				if (!new_buffer) {
					DBGPRINT(DEBUG_ERROR, ("%s: malloc fail\n", __FUNCTION__));
					return FAILED;
				}
#endif		
				if(pDesc->forward_node){
 	                send_cmm_data( pDesc, 0,  ((struct dllist *)(pDesc->forward_node))->mac,((struct dllist *)(pDesc->forward_node))->id, new_buffer, ETH_FRAME_LEN, 0);
                }
#else
                // set default TDMA counter information
                refreash_tdma_timer(pDesc, ntohl(hdr->tdma_count), ntohl(hdr->time_stamp), ntohl(hdr->info) );
                correct_tdma_timer(pDesc, 0x1000, 0);					 
				// set TDMA delay tick numbers
				SET_TDMA_START_DELAY_TICK(pDesc,2);
				/*setup cycle count timer*/ 
				armv7_setup_pmnc(0);
				/*request irq and start timer*/
				armv7_request_irq();
				armv7_pmnc_start();
#endif					
				// TBD
				// set edge node's time slot
				pDesc->tdma_info.id = fixed_id;
				pDesc->tdma_info.timeslot[0] = fixed_id;
				pDesc->tdma_info.timeslot[1] = -1;
				pDesc->tdma_info.total_slot = TDMA_NODE_NUMBERS;
					 
				break; 																								 
					 
                
#endif                
			}else if ( ntohs(eh->h_proto) == LOCATION_COMM_PROTO && ntohs(hdr->op_code) == NT_MSG_MASTER_ASSIGN){
				void* new_buffer ;
				if(!SAME_MAC(eh->h_dest, pDesc->src_mac)) {
					break;
				}
#ifdef _NO_KMALLOC
			new_buffer = process_buf;
#else
			new_buffer = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
			if (!new_buffer) {
				DBGPRINT(DEBUG_ERROR, ("%s: malloc fail\n", __FUNCTION__));
				return FAILED;
			}
#endif					
			memset(new_buffer, 0,ETH_FRAME_LEN);
			DBGPRINT(DEBUG_ALL, ("%s: send NT_MSG_MASTER_ASSIGN_ACK back,  %02x:%02x:%02x:%02x:%02x:%02x\r\n", __FUNCTION__, PRINT_MAC(eh->h_source)));
			send_cmm_ack_packet(pDesc , eh->h_source, new_buffer, ETH_FRAME_LEN, NT_MSG_MASTER_ASSIGN_ACK, hdr->seq_num);
			memcpy(pDesc->left_mac, eh->h_source, ETH_FRAME_LEN );
#ifndef _NO_KMALLOC					
			kfree(new_buffer);		
#endif
			}
			break;
		}
		case NTBR_STATE_ASSIGN_MASTER:
		{				
			struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;
			if ( ntohs(eh->h_proto) == LOCATION_COMM_PROTO && ntohs(hdr->op_code) == NT_MSG_MASTER_ASSIGN_ACK) {
				DBGPRINT(DEBUG_ALL,("Receive assign master ack, enter NTBR_STATE_IFNO_CHANGE\n"));
				pDesc->retry = 0;
				pDesc->dev_state = NTBR_STATE_IFNO_CHANGE;
				pDesc->sub_state = NTBR_SUBSTATE_READY_FORWARD;
				OSCancelTimer( &pDesc->TimeoutTimer, &cancel);
				OSSetTimer(&pDesc->TimeoutTimer, INFO_CHANGE_TIME); 		
			}
			break;
		}
		case NTBR_STATE_IFNO_CHANGE:
		{
			char *buf_send;
			struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;
			if ( ntohs(eh->h_proto) == LOCATION_COMM_PROTO ) {
				if( SAME_MAC(eh->h_dest,pDesc->src_mac)){ // Peer node reply 
					}else if (IS_BROADCAST(eh->h_dest)){
						DBGPRINT(DEBUG_TDMA,("Receive a broadcast packet in NTBR_STATE_IFNO_CHANGE, sub-state=%d, type=%d, jiffies=0x%x\n",  pDesc->sub_state, ntohs(hdr->op_code), jiffies ));
						reply =1;
					}else{
						///--DBGPRINT(DEBUG_TDMA,("Receive a packet but it's not belong to me??\n") ); // go out the function
						break;
					}

					switch(pDesc->sub_state) {
						case NTBR_SUBSTATE_READY_FORWARD:
						{
							if( ntohs(hdr->op_code) == NT_MSG_TABLE_CHANGE) {
								DBGPRINT(DEBUG_WARN,("Receive a NT_MSG_TABLE_CHANGE packet, enter NTBR_SUBSTATE_RECEIVED_FORWARD\n"));
								pDesc->sub_state = NTBR_SUBSTATE_RECEIVED_FORWARD;
								memset(&(pDesc->forward_addr), 0, sizeof(FORWARD_ADDR_INFO));
								memcpy(pDesc->forward_addr.in_mac,eh->h_source,ETH_MAC_LEN);
								if ( hdr->len - sizeof(struct location_commu_encaps_hdr) + sizeof(struct ethhdr) > 0 ) {
								    pDesc->forward_addr.len =  hdr->len - sizeof(struct location_commu_encaps_hdr) + sizeof(struct ethhdr);
									memcpy(pDesc->forward_addr.buffer,(buffer + sizeof(struct location_commu_encaps_hdr)),pDesc->forward_addr.len);
								}
#ifdef _NO_KMALLOC
								buf_send = process_buf;
#else
								buf_send = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
								if (!buf_send) {
									DBGPRINT(DEBUG_ERROR, ("%s: %d, malloc fail\n", __FUNCTION__, __LINE__));
									return FAILED;
								}
#endif								
								memset(buf_send, 0,ETH_FRAME_LEN);
								send_cmm_ack_packet(pDesc, eh->h_source, buf_send, ETH_FRAME_LEN, NT_MSG_RECEIVE_FORWARD_ACK, hdr->seq_num);								
								
								if( choose_node_near_left_edge(pDesc)) {
								
									OSCancelTimer( &pDesc->TimeoutTimer, &cancel);
									OSSetTimer(&pDesc->TimeoutTimer, NEIGHBOR_TABLE_SENDBACK_TIME);						  

									memset(buf_send, 0,ETH_FRAME_LEN);
									// Start sending neighbor table back to the left edge node
									if (pDesc->forward_addr.len > 0) {
										send_cmm_neighbor_info_packet(pDesc , ((struct dllist *)(pDesc->backward_node))->mac, buf_send, ETH_FRAME_LEN, pDesc->forward_addr.buffer, 	pDesc->forward_addr.len );
									}else {
										DBGPRINT(DEBUG_ERROR, ("%s: Not receiving neighbor information\n", __FUNCTION__, __LINE__));
										send_cmm_neighbor_info_packet(pDesc , ((struct dllist *)(pDesc->backward_node))->mac, buf_send, ETH_FRAME_LEN, NULL, 0);
									}									 
								}else if ( pDesc->flag == LEFT_EDGE_NODE ) {
								    DBGPRINT(DEBUG_ALL, ("%s:%d, prepare to enter TDMA state\n", __FUNCTION__, __LINE__));
									printk("total len=%d\n",  hdr->len - sizeof(struct location_commu_encaps_hdr) +  sizeof(struct ethhdr));

									// show all neighbor table
								    show_neighbor_list_data( buffer + sizeof(struct location_commu_encaps_hdr), hdr->len - sizeof(struct location_commu_encaps_hdr) +  sizeof(struct ethhdr));
									// add other nodes information into neighbor list							  
									set_neighbor_list_table(pDesc, buffer + sizeof(struct location_commu_encaps_hdr), hdr->len - sizeof(struct location_commu_encaps_hdr) +  sizeof(struct ethhdr));

								    // prepare to enter TDMA state								     
								    pDesc->dev_state = NTBR_STATE_STEADY;	
									pDesc->sub_state = NTBR_SUBSTATE_FORWARD_DONE;							  
								    // enable resend timer 
									OSCancelTimer( &pDesc->TimeoutTimer, &cancel);
									OSSetTimer(&pDesc->TimeoutTimer, NEIGHBOR_TABLE_SENDBACK_TIME);
#if TDMA_FUNCTION_START==1	
                                    // init TDMA tick counter
									INIT_TICK_COUNT(pDesc);
									// set TDMA delay tick numbers
									SET_TDMA_START_DELAY_TICK(pDesc,DELAY_TICK_COUNT);
									/*setup cycle count timer*/ 
									armv7_setup_pmnc(0);
									/*request irq and start timer*/
									armv7_request_irq();
									armv7_pmnc_start();
                                    // TBD
                                    // set edge node's time slot
                                    pDesc->tdma_info.id = 0;
									pDesc->tdma_info.timeslot[0] = 0;
									pDesc->tdma_info.timeslot[1] = -1;
									pDesc->tdma_info.total_slot = TDMA_NODE_NUMBERS;                                      									 
#endif
									memset(buf_send, 0,ETH_FRAME_LEN);
									// send tdma trigger packet out
									send_cmm_packet_tdmaslot(pDesc , pDesc->sequence, ((struct dllist *)(pDesc->forward_node))->mac, buf_send, ETH_FRAME_LEN); 
									//send_cmm_packet_tdmaslot(pDesc , pDesc->sequence, NULL, buf_send, ETH_FRAME_LEN); 
								}else {
									DBGPRINT(DEBUG_ERROR, ("%s: %d, NTBR_SUBSTATE_READY_FORWARD fail\n", __FUNCTION__, __LINE__));
									pDesc->dev_state = NTBR_STATE_INIT; // back to init ???
									OSCancelTimer( &pDesc->TimeoutTimer, &cancel);
									OSModTimer(&pDesc->TimeoutTimer, BROADCAST_DURATION);	
									
								}
#ifndef _NO_KMALLOC									 
								kfree(buf_send);
#endif
							}		
						}							
						break;
						case NTBR_SUBSTATE_RECEIVED_FORWARD:
						{
							// send ack back
							if( ntohs(hdr->op_code) == NT_MSG_TABLE_CHANGE) {
								if(!memcmp(pDesc->forward_addr.in_mac,eh->h_source, ETH_FRAME_LEN)) {
#ifdef _NO_KMALLOC
									buf_send = process_buf;
#else									
									buf_send = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
									if (!buf_send) {
										 DBGPRINT(DEBUG_ERROR, ("%s: %d, malloc fail\n", __FUNCTION__, __LINE__));
										 return FAILED;
									}
#endif									
									memset(buf_send, 0,ETH_FRAME_LEN);
									DBGPRINT(DEBUG_ALL, ("%s:%d, NTBR_SUBSTATE_RECEIVED_FORWARD, receive NT_MSG_TABLE_CHANGE\n", __FUNCTION__, __LINE__));
									send_cmm_ack_packet(pDesc, eh->h_source, buf_send, ETH_FRAME_LEN, NT_MSG_RECEIVE_FORWARD_ACK, hdr->seq_num);
									
								}else {
									DBGPRINT(DEBUG_ERROR, ("%s: %d, Receive a new node forwarding request!! mac=\n", __FUNCTION__, __LINE__));
								}
							}
							if ( ntohs(hdr->op_code) == NT_MSG_RECEIVE_FORWARD_ACK ) {
								if(hdr->seq_num == pDesc->forward_addr.seq_num ) {
									DBGPRINT(DEBUG_ALL, ("%s:%d, receiving NT_MSG_RECEIVE_FORWARD_ACK\n", __FUNCTION__, __LINE__));
									pDesc->sub_state = NTBR_SUBSTATE_FORWARD_DONE;
									OSCancelTimer( &pDesc->TimeoutTimer, &cancel);
									OSModTimer(&pDesc->TimeoutTimer, NEIGHBOR_TABLE_FORWARD_DONE_WAIT_TDMA_TIME);
								}
							}
						}
						break;
						case NTBR_SUBSTATE_FORWARD_DONE:
						{
							//send ack back
							if( ntohs(hdr->op_code) == NT_MSG_TABLE_CHANGE) {
								if(!memcmp(pDesc->forward_addr.in_mac,eh->h_source, ETH_FRAME_LEN)) { // just resend ack back
#ifdef _NO_KMALLOC
									buf_send = process_buf;
#else																		
									buf_send = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
									if (!buf_send) {
										 DBGPRINT(DEBUG_ERROR, ("%s: %d, malloc fail\n", __FUNCTION__, __LINE__));
										 return FAILED;
									}
#endif									
									memset(buf_send, 0,ETH_FRAME_LEN);
									DBGPRINT(DEBUG_ALL, ("%s:%d, NTBR_SUBSTATE_FORWARD_DONE, receive NT_MSG_TABLE_CHANGE\n", __FUNCTION__, __LINE__));
									send_cmm_ack_packet(pDesc, eh->h_source, buf_send, ETH_FRAME_LEN, NT_MSG_RECEIVE_FORWARD_ACK, hdr->seq_num);	
								}else {
									DBGPRINT(DEBUG_ERROR, ("%s: %d, Receive a new node forwarding request!! mac=\n", __FUNCTION__, __LINE__));
								}
							}
							else if (ntohs(hdr->op_code) == NT_MSG_TDMA_ASSIGN) {
								// compare and merge the neighbor list table
								int i,data_len;
								char *p;
								DBGPRINT(DEBUG_TDMA, ("Receive NT_MSG_TDMA_ASSIGN in NTBR_SUBSTATE_FORWARD_DONE\n"));
					
								data_len = hdr->len - (sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr));
								p = buffer + sizeof(struct location_commu_encaps_hdr);	
								if(data_len>0)
									refreash_tdma_table( pDesc, eh->h_source, p, data_len, hdr->seq_num, hdr->time_stamp );
								else {
									DBGPRINT(DEBUG_ALL, ("Receive a TDMA assigned packet but len < 0 ??\n"));
								}

								// enable resend timer	
								OSCancelTimer( &pDesc->TimeoutTimer, &cancel);
								OSSetTimer(&pDesc->TimeoutTimer, NEIGHBOR_TABLE_SENDBACK_TIME);
								// set TDMA delay tick numbers
								SET_TDMA_START_DELAY_TICK(pDesc,ntohl(hdr->info));
                                // set TDMA counter information
								refreash_tdma_timer(pDesc, ntohl(hdr->tdma_count), ntohl(hdr->time_stamp), ntohl(hdr->info));

#ifdef _NO_KMALLOC
								buf_send = process_buf;
#else																		
								buf_send = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
								if (!buf_send) {
									 DBGPRINT(DEBUG_ERROR, ("%s: %d, malloc fail\n", __FUNCTION__, __LINE__));
									 return FAILED;
								}
#endif		
								memset(buf_send, 0,ETH_FRAME_LEN);
                                send_cmm_ack_packet(pDesc, eh->h_source, buf_send, ETH_FRAME_LEN, NT_MSG_TDMA_ASSIGN_ACK, hdr->seq_num);						
                                //send_cmm_ack_packet(pDesc, NULL, buf_send, ETH_FRAME_LEN, NT_MSG_TDMA_ASSIGN_ACK, hdr->seq_num);

							    pDesc->dev_state = NTBR_STATE_STEADY;
								pDesc->sub_state = NTBR_SUBSTATE_READY_FORWARD;
								//printk("NTBR_STATE_STEADY...\r\n");
							}
							break;
						}						
						default:
							break;
					}
			}
			break;	
		}
		case NTBR_STATE_STEADY:             			
		{
			char *buf_send;
            struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;
			switch(pDesc->sub_state) {
				case NTBR_SUBSTATE_READY_FORWARD: // middle node
				{
					if( ntohs(hdr->op_code) == NT_MSG_TDMA_CONFIRMM ) {						
						pDesc->sub_state = NTBR_SUBSTATE_FORWARD_DONE;						 						
						// enable resend timer 
						OSCancelTimer( &pDesc->TimeoutTimer, &cancel);
						OSSetTimer(&pDesc->TimeoutTimer, NEIGHBOR_TABLE_SENDBACK_TIME);
						// set TDMA delay tick numbers
						SET_TDMA_START_DELAY_TICK(pDesc,DELAY_TICK_COUNT);
						// set TDMA counter information
						refreash_tdma_timer(pDesc,  ntohl(hdr->tdma_count),  ntohl(hdr->time_stamp), ntohl(hdr->info) );
						correct_tdma_timer(pDesc,  ntohl(hdr->diff_pmmc),  ntohl(hdr->diff_tick));
#if TDMA_FUNCTION_START==1
						armv7_stop_pmnc();
						armv7_pmnc_disable_counter(CCNT);
						/*setup cycle count timer*/ 
						armv7_setup_pmnc(pDesc->tdma_info.tick_pmmc_value);
						/*request irq and start timer*/
						armv7_request_irq();
						armv7_pmnc_start();
#endif
						DBGPRINT(DEBUG_TDMA, ( "tick=%d,rx ts=%d\r\n",  pDesc->tdma_info.tick_count, ntohl(hdr->time_stamp) ));
						DBGPRINT(DEBUG_TDMA, ( "pmmc=0x%x\r\n", pDesc->tdma_info.tick_pmmc_value));
#ifdef _NO_KMALLOC
						buf_send = process_buf;
#else																		
						buf_send = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
						if (!buf_send) {
							DBGPRINT(DEBUG_ERROR, ("%s: %d, malloc fail\n", __FUNCTION__, __LINE__));
							return FAILED;
						}
#endif						 
						memset(buf_send, 0,ETH_FRAME_LEN);
						send_cmm_ack_packet(pDesc, eh->h_source, buf_send, ETH_FRAME_LEN, NT_MSG_TDMA_CONFIRMM_ACK, hdr->seq_num);
                        if(pDesc->forward_node) {
							memset(buf_send, 0,ETH_FRAME_LEN);                 
							// send tdma assign packet out
							send_cmm_packet_tdmaslot(pDesc , pDesc->sequence, ((struct dllist *)(pDesc->forward_node))->mac, buf_send, ETH_FRAME_LEN); 
                        }else {
							OSCancelTimer( &pDesc->TimeoutTimer, &cancel);
							pDesc->dev_state = NTBR_STATE_TDMA_INIT;
							pDesc->sub_state = NTBR_SUBSTATE_FORWARD_DONE;
                            DBGPRINT(DEBUG_TDMA, ("TDMA init, last node\n"));
                        }
					}
					break;
				}			
				case NTBR_SUBSTATE_FORWARD_DONE:
				{
				  	if( ntohs(hdr->op_code) == NT_MSG_TDMA_ASSIGN_ACK) {
						unsigned int pmmc_count;
						unsigned int diff_pmmc_count;						
				  	    pDesc->dev_state = NTBR_STATE_TDMA_INIT;
						pDesc->sub_state = NTBR_SUBSTATE_READY_FORWARD;					    
					    // count diff of p-Delay, 
						asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (pmmc_count));	
						pDesc->pro_delay.diff_tick_count = pDesc->tdma_info.tick_count - pDesc->tdma_old.tick_count;					
                        if(pmmc_count > pDesc->tdma_old.tick_pmmc_value)
							pDesc->pro_delay.diff_tick_pmmc_value = pmmc_count - pDesc->tdma_old.tick_pmmc_value;
						else {
							pDesc->tdma_old.tick_pmmc_value - pmmc_count;
							pDesc->pro_delay.diff_tick_count--;
						}						
						// enable resend timer	
						OSCancelTimer( &pDesc->TimeoutTimer, &cancel);
						OSSetTimer(&pDesc->TimeoutTimer, NEIGHBOR_TABLE_SENDBACK_TIME);					
#ifdef _NO_KMALLOC
						buf_send = process_buf;
#else																		
						buf_send = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
						if (!buf_send) {
							 DBGPRINT(DEBUG_ERROR, ("%s: %d, malloc fail\n", __FUNCTION__, __LINE__));
							 return FAILED;
						}
#endif						
						memset(buf_send, 0,ETH_FRAME_LEN);
						// send TDMA start to next node	
						send_cmm_packet_tdma_start(pDesc , pDesc->sequence, ((struct dllist *)(pDesc->forward_node))->mac, buf_send, ETH_FRAME_LEN);    
		            }
					break;
				}									
			default:	
				break;
            }
			break;
		}
		case NTBR_STATE_TDMA_INIT: 					   
		{
			char *buf_send;
			struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;
		    switch(pDesc->sub_state) {
				case NTBR_SUBSTATE_READY_FORWARD:
				{
					if( ntohs(hdr->op_code) == NT_MSG_TDMA_CONFIRMM_ACK && pDesc->forward_node && SAME_MAC(eh->h_source,((struct dllist *)(pDesc->forward_node))->mac) ) {
						unsigned int pmmc_count;
						DBGPRINT(DEBUG_TDMA, ("Receive NT_MSG_TDMA_CONFIRMM_ACK...\n"));
						pDesc->sub_state = NTBR_SUBSTATE_FORWARD_DONE;
						OSCancelTimer( &pDesc->TimeoutTimer, &cancel);
					}
					break;	
				}		
				default:
					break;
			}
			break;
		}
		case NTBR_STATE_TDMA:			
		{
		    char *buf_send;
		    struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;
		    unsigned int tmp_test_seq = 0;
			switch(pDesc->sub_state) {
				case NTBR_SUBSTATE_READY_FORWARD:
				{
						if ( ntohs(eh->h_proto) == LOCATION_COMM_PROTO && ntohs(hdr->op_code) == NT_MSG_TDMA_ASSIGN_ACK &&  (SAME_MAC(eh->h_dest,pDesc->src_mac)||IS_BROADCAST(eh->h_dest) ) ) {
							unsigned int pmmc_count;
							unsigned int diff_pmmc_count;
							if( hdr->dst_addr != 0xffff && hdr->dst_addr != pDesc->tdma_info.id ) // check the address 
								break;
							pDesc->sub_state = 	NTBR_SUBSTATE_RECEIVED_FORWARD;														
							// count diff of p-Delay, 
							asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (pmmc_count)); 							
							pDesc->pro_delay.diff_tick_count = pDesc->tdma_info.tick_count - pDesc->tdma_old.tick_count;
							if(pmmc_count > pDesc->tdma_old.tick_pmmc_value)
								pDesc->pro_delay.diff_tick_pmmc_value = pmmc_count - pDesc->tdma_old.tick_pmmc_value;
							else {
								pDesc->tdma_old.tick_pmmc_value - pmmc_count;
								pDesc->pro_delay.diff_tick_count--;
							}		
							pDesc->get_sync_count++;
						}
					break;
				}
				case NTBR_SUBSTATE_RECEIVED_FORWARD:
					break;
                case NTBR_SUBSTATE_FORWARD_DONE:
					break;
			}
			if (ntohs(eh->h_proto) == LOCATION_COMM_PROTO && ntohs(hdr->op_code) == NT_MSG_DATA) {
				int counted =0;
			    int tick_diff;
			    int pmmc_diff;
				unsigned int pmmc_count;
				if(hdr->dst_addr != 0xffff && hdr->dst_addr != pDesc->tdma_info.id ) // check the address 
					break;
#if NCTU_WAVE==1
#else				   
			    // check destination address is mine
			    // store the packet and next timeslot out
                if(hdr->seq_num != pDesc->sequence) {
				   	printk("%d", pDesc->sequence - hdr->seq_num );
#if UART_SUPPORT==1 && UART_DBG_PRINT==1
					if(msg_buffer_idx<DBG_BUF_MAX-10) {
						msg_buffer_idx += sprintf(msg_buffer + msg_buffer_idx, "S-%d " , pDesc->sequence - hdr->seq_num );
						//uart_send_packet(msg_buffer,strlen(msg_buffer));
					}
#endif					  
#if TDMA_DEBUUG_LOG==1
					if( ((pDesc->forward_node && !SAME_MAC(eh->h_source,((struct dllist *)(pDesc->forward_node))->mac)) ||  !pDesc->forward_node)) {
						if( pDesc->flag != LEFT_EDGE_NODE ){ // not the last node, record the buffer data
							if( pDesc->forward_node && hdr->src_addr == pDesc->forward_node->id ) { // to avoid loop issue
							}else {
								char *bTmp = buffer + sizeof(struct location_commu_encaps_hdr);							  
								int data_len =  hdr->len - (sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr));
								int refill=0;
								// for counting the out of sequence packets and update status, ex: Unicast retry data will enter this entry
								if(pDesc->buffer.sequence != hdr->seq_num) { 
									if(ETH_FRAME_LEN - pDesc->buffer.len >= hdr->len - (sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr)) ) {
										refill=1;
									}
								}
                                //printk("b-seq=%d, h-seq=%d,dlen=%d\n",pDesc->buffer.sequence , hdr->seq_num , data_len);
								for( ;data_len > sizeof(struct _msg_data_header); ) {
									SENSOR_DATA *sen_ptr = bTmp;
                                    if(sen_ptr->header.type == SPECAIL_CMD_DATA) {
                                        if(sen_ptr->header.node_id >= 10 || sen_ptr->header.sequence >= 2000)
										    printk("SPECAIL_CMD_DATA, nid=%d, seq=%d\r\n", sen_ptr->header.node_id, sen_ptr->header.sequence);
										pDesc->rx_out[sen_ptr->header.node_id].data[ sen_ptr->header.sequence] = 'I';
										pDesc->rx_out[sen_ptr->header.node_id].count++;								  
										  	
									}else {
									    break;
									}
									if(refill) {
										if(pDesc->rx_check[sen_ptr->header.node_id].data[ sen_ptr->header.sequence]=='I') {
											pDesc->duplicate[sen_ptr->header.node_id].data[sen_ptr->header.sequence]++;
											pDesc->duplicate[sen_ptr->header.node_id].count++;
										}else {
											pDesc->rx_check[sen_ptr->header.node_id].data[ sen_ptr->header.sequence] = 'I';
											pDesc->rx_check[sen_ptr->header.node_id].count++;
										}
                                    }									 
									data_len -= ( sen_ptr->header.len	+ sizeof(struct _msg_data_header));
									bTmp += ( sen_ptr->header.len +  sizeof(struct _msg_data_header));    
								}
							}
						}				    
					}
#endif
					counted =1;
					pDesc->tdma_status.total_rx_out_count++;
                }				  	
#endif
				// update status
#if TDMA_DEBUUG_LOG==1				   
				///--if( (counted ==0) &&((pDesc->forward_node && !SAME_MAC(eh->h_source,((struct dllist *)(pDesc->forward_node))->mac)) ||  !pDesc->forward_node) && (pDesc->cycle > RE_SYNC_CYCLY_COUNT<<1))
				if( (counted ==0) &&((pDesc->forward_node && !SAME_MAC(eh->h_source,((struct dllist *)(pDesc->forward_node))->mac)) ||  !pDesc->forward_node)) {
					pDesc->tdma_status.total_rx_count++;
					if( pDesc->flag != LEFT_EDGE_NODE ){ // not the last node, record the buffer data
						if( pDesc->forward_node && hdr->src_addr == pDesc->forward_node->id ) { // to avoid loop issue
						}else {
							char *bTmp = buffer + sizeof(struct location_commu_encaps_hdr);                              
							int data_len =  hdr->len - (sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr));							  
							for( ;data_len > sizeof(struct _msg_data_header); ) {
									SENSOR_DATA *sen_ptr = bTmp;
									if(sen_ptr->header.node_id >= 10 || sen_ptr->header.sequence >= 2000)
										printk("SPECAIL_CMD_DATA, nid=%d, seq=%d\r\n", sen_ptr->header.node_id, sen_ptr->header.sequence);
									if(sen_ptr->header.type == SPECAIL_CMD_DATA) {
										if(pDesc->start_test==0){
											pDesc->start_test = 1;
											pDesc->buffer.len = 0; // clean buffer first
										}
#if NCTU_WAVE==1
									if(sen_ptr->header.node_id == 0 ) // left node
										tmp_test_seq = sen_ptr->header.sequence;
#endif											   
									if(pDesc->rx_check[sen_ptr->header.node_id].data[ sen_ptr->header.sequence]=='I') {
										pDesc->duplicate[sen_ptr->header.node_id].data[sen_ptr->header.sequence]++;
										pDesc->duplicate[sen_ptr->header.node_id].count++;
									}else {
										pDesc->rx_check[sen_ptr->header.node_id].data[ sen_ptr->header.sequence] = 'I';
										pDesc->rx_check[sen_ptr->header.node_id].count++;
									}
									}else {
										break;
									}
									data_len -= ( sen_ptr->header.len  + sizeof(struct _msg_data_header));
									bTmp += ( sen_ptr->header.len +  sizeof(struct _msg_data_header)); 	
							};					  
						}
					}					
   					  
				}
#else				   	
                pDesc->tdma_status.total_rx_count++;				

#endif				   	
			    pDesc->tdma_count.pmmc = ntohl(hdr->tdma_count);
				pDesc->tdma_count.ticks = ntohl(hdr->time_stamp);
			    // count status
			    asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (pmmc_count));	  
			    tick_diff = pDesc->total_tick_number -  pDesc->tdma_count.ticks;
			    pmmc_diff = pmmc_count - pDesc->tdma_count.pmmc;
				// log status
				if ( tick_diff > pDesc->tdma_status.diff_max.ticks ) {
					pDesc->tdma_status.diff_max.ticks = tick_diff;
					pDesc->tdma_status.diff_max.pmmc = pmmc_diff;
				}
				if ( tick_diff < pDesc->tdma_status.diff_min.ticks ) {
					pDesc->tdma_status.diff_min.ticks = tick_diff;
					pDesc->tdma_status.diff_min.pmmc = pmmc_diff;
				}
				if(GET_RSYNC_REQ(hdr->flag)){
#ifdef _NO_KMALLOC
					buf_send = process_buf;
#else																		
					buf_send = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
					if (!buf_send) {
						DBGPRINT(DEBUG_ERROR, ("%s: %d, malloc fail\n", __FUNCTION__, __LINE__));
						return FAILED;
					}
#endif		
					memset(buf_send, 0,ETH_FRAME_LEN);												
                    send_cmm_ack_packet(pDesc, NULL, buf_send, ETH_FRAME_LEN, NT_MSG_TDMA_ASSIGN_ACK, hdr->seq_num); // try to use broadcast to test packet lost issue
#if PRINTK_BUF==1
					if(printk_msg_buffer_idx<PRINTK_DBG_BUF_MAX-10) {
						printk_msg_buffer_idx += sprintf(printk_msg_buffer + printk_msg_buffer_idx, "R" );
					}
#else						
					printk("R");
#endif							
				}
				if(GET_SYNC_BIT(hdr->flag)) {
					refreash_tdma_timer(pDesc,	ntohl(hdr->tdma_count),  ntohl(hdr->time_stamp), ntohl(hdr->info) );
					correct_tdma_timer(pDesc,  ntohl(hdr->diff_pmmc),  ntohl(hdr->diff_tick));
#if TDMA_FUNCTION_START==1
					armv7_stop_pmnc();
					armv7_pmnc_disable_counter(CCNT);
					/*setup cycle count timer*/ 
					armv7_setup_pmnc(pDesc->tdma_info.tick_pmmc_value);
					/*request irq and start timer*/
					armv7_pmnc_start();
#endif
#if	MULTI_PKYS_PER_SLOT==1
                    pDesc->frames = hdr->frames; // set sequence number to avoid debug message is printed always

#else
					pDesc->sequence = hdr->seq_num; // set sequence number to avoid debug message is printed always
#endif
					DBGPRINT(DEBUG_TDMA, ( "tick=%d,rx ts=%d\r\n",	pDesc->tdma_info.tick_count, ntohl(hdr->time_stamp) ));
					DBGPRINT(DEBUG_TDMA, ( "pmmc=0x%x\r\n", pDesc->tdma_info.tick_pmmc_value));
#if PRINTK_BUF==1
					if(printk_msg_buffer_idx<PRINTK_DBG_BUF_MAX-10) {
						printk_msg_buffer_idx += sprintf(printk_msg_buffer + printk_msg_buffer_idx, "s" );
					}
#else						
					printk("s ");
#endif							
				}
				if( pDesc->flag != LEFT_EDGE_NODE && pDesc->forward_node){ // not the last node, record the buffer data
					if( hdr->src_addr == pDesc->forward_node->id ) { // to avoid loop issue
					}else {
						if(pDesc->buffer.sequence != hdr->seq_num) {
				            pDesc->buffer.sequence = hdr->seq_num;
							SENSOR_DATA  *dbg_wsn;
							OS_SEM_LOCK(&pDesc->buffer_lock);
							if(ETH_FRAME_LEN - pDesc->buffer.len >= hdr->len - (sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr)) ) {
						        memcpy(((unsigned int)pDesc->buffer.data + pDesc->buffer.len), buffer + sizeof(struct location_commu_encaps_hdr), hdr->len - (sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr)) );
								pDesc->buffer.len += hdr->len - (sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr));
						    }else {
						        DBGPRINT(DEBUG_ERROR, ("O "));
						    }		
						    OS_SEM_UNLOCK(&pDesc->buffer_lock);
						}
				    }
#if NCTU_WAVE==1
#ifdef _NO_KMALLOC
					new_buffer = process_buf;
#else
					new_buffer = kmalloc(ETH_FRAME_LEN, GFP_KERNEL);
					if (!new_buffer) {
						DBGPRINT(DEBUG_ERROR, ("%s: malloc fail\n", __FUNCTION__));
						return FAILED;
					}
#endif							   
					printk("send_forward_node MAC %02x:%02x:%02x:%02x:%02x:%02x\r\n", PRINT_MAC(((struct dllist *)(pDesc->forward_node))->mac));
					send_cmm_data( pDesc, tmp_test_seq,  ((struct dllist *)(pDesc->forward_node))->mac,((struct dllist *)(pDesc->forward_node))->id, new_buffer, ETH_FRAME_LEN, 0);
#endif				   
				}
			}
			break;	
		}	
		default:
			break;
    }
}

#if UART_SUPPORT==1
void uart_rx(void)
{
    int i,j,ret;
	unsigned int tick;
	char *cTmp;
	i=0;
	j=0;
	while(uart_number<USRT_BUFF_SIZE) {
		//ret = serail_get_char_directly(cTmp, 1);
		if(ret>0)
		{
			test_buffer[test_idx_tail] = cTmp;
			printk("test_buffer[0]=%02x\n", test_buffer[0]);
			test_idx_tail = ++test_idx_tail % USRT_BUFF_SIZE ;
			uart_number++;		  
			i++;
			uart_flag = 1;
			if(i>600)
				break;
		}else {
		    break;
		}
	}
}

void uart_rx_char(char c)
{
	unsigned long flags;	
    //if(c!=0){
    if(1) {
		//printk("%c",c);
		spin_lock_irqsave(&uartLock, flags); // save the state, if locked already it is saved in flags
		test_buffer[test_idx_tail] = c;
		test_idx_tail = ++test_idx_tail % USRT_BUFF_SIZE ;
		uart_number++;
		spin_unlock_irqrestore(&uartLock, flags); // return to the formally state specified in flags
    }else {
        send_char('0');
    }
}
int get_buff_char(unsigned char *c)
{
	unsigned long flags;	
    if(uart_number>0) {
		spin_lock_irqsave(&uartLock, flags); // save the state, if locked already it is saved in flags
	    *c = test_buffer[test_idx_head];
	    test_idx_head = ++test_idx_head % USRT_BUFF_SIZE ;
	    uart_number--;
		// Critical section
		spin_unlock_irqrestore(&uartLock, flags); // return to the formally state specified in flags
		return 1;
    }else {
        return -1;
    }
}			   
#endif
static void ksocket_start(void)
{
	//u32 flags;	
	int err,size,i,uart_receive_len;
	unsigned short tmp;
	/*UART setting initial*/
	mm_segment_t oldfs;
	unsigned char *buf;
	char uart_receive_buf[500];
	
	int *uart_len;
	
	buf = kmalloc(BUFFER_SIZE+1, GFP_KERNEL);
	if(!buf) {
		printk("ksocket_start: malloc failed!!\n");
        goto out;
	}
    printk(KERN_INFO "ksocket_start!!\n");
	init_dev_desc(gState); // also init socket address
	initList(&gState->RscTimerCreateList);
	NdisAllocateSpinLock(&TimerSemLock);
	NdisAllocateSpinLock(&gState->list_lock);
	NdisAllocateSpinLock(&gState->buffer_lock);
	link_test(&gState->head, &gState->tail);
	///--preempt_disable();
	gState->signal_level = START_SIGNAL_PERCENTAGE;
	gState->running = 1;
	gState->flag = nodemode;
	/* kernel thread initialization */
	lock_kernel();
	current->flags |= PF_NOFREEZE;	
	/* daemonize (take care with signals, after daemonize() they are disabled) */
	daemonize(MODULE_NAME);
	allow_signal(SIGKILL);
	unlock_kernel();

	printk(KERN_INFO "ksocket_start initialization...\n");

	if( (err = sock_create(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL),&gState->sock)) < 0) {
        printk(KERN_INFO MODULE_NAME": Could not create a raw socket, error = %d\n", -ENXIO);
	    goto out;
    }

	printk(KERN_INFO "After a kernel socket\n");
	
#if PRINTK_BUF ==1
	printk_msg_buffer_idx = 0;
#endif

#if UART_SUPPORT==1
	spin_lock_init((spinlock_t *)(&uartLock)); 	
	init_uart_send_buf();
#endif

	//OSInitTimer(gState, &gState->TimeoutTimer, GET_TIMER_FUNCTION(TimeoutExec), gState, FALSE);	
	//OSSetTimer(&gState->TimeoutTimer, BROADCAST_DURATION);//start timer peroid=BROADCAST_DURATION
/*	
#if TDMA_FUNCTION_START==1	
	tasklet_init(&my_tasklet, my_softirq, (unsigned long)gState );
#endif
	OSInitTimer(gState, &gState->PeriodicTimer,  GET_TIMER_FUNCTION(MlmePeriodicExec), gState, TRUE);	
	OSSetTimer(&gState->PeriodicTimer, MLME_TASK_EXEC_INTV );//start timer peroid=MLME_TASK_EXEC_INTV
	memset(buf, 0,ETH_FRAME_LEN);	
#if JUST_TDMA==1
	msleep(500);
	send_cmm_trigger_packet(gState , NULL ,buf, ETH_FRAME_LEN); // send boradcast out at initialization
	if (gState->flag == LEFT_EDGE_NODE)
        gState->dev_state = NTBR_STATE_HELO;
	else
		gState->dev_state = NTBR_STATE_INIT;	 
	gState->tdma_info.id = fixed_id;
	gState->tdma_info.timeslot[0]= fixed_id;
    if(IS_ZERO_MAC(next_mac))
		gState->forward_node = NULL;
	else {
		gState->forward_node = &forward_lnode;
		memcpy(((struct dllist *)(gState->forward_node))->mac, next_mac, 6);
		gState->forward_node->id = fixed_id + 1;
		printk("forward_node MAC %02x:%02x:%02x:%02x:%02x:%02x\r\n", PRINT_MAC(((struct dllist *)(gState->forward_node))->mac));
		printk(KERN_INFO "gState->forward_node->id=%d\r\n", gState->forward_node->id );
	}
#elif NCTU_SIGNAL_SEARCH ==1
	if(gState->flag == LEFT_EDGE_NODE) {	
		msleep(500);
		send_cmm_trigger_packet(gState , NULL ,buf, ETH_FRAME_LEN); // send boradcast out at initialization
		gState->dev_state = NTBR_STATE_HELO;		
	}
#else
*/
	get_random_bytes(&tmp, sizeof(short));
	tmp %= 10;
	msleep(tmp);
	for(i=0;i<ETH_FRAME_LEN-2*ETH_MAC_LEN;i++) {
		buf[i+2*ETH_MAC_LEN] = i;
	}
	send_cmm_packet(gState, NULL, buf, ETH_FRAME_LEN); // send boradcast out at initialization
//#endif
	for(;;) {
		uart_receive_len = recv_char(uart_receive_buf);
			//printk("uart_receive_len=%d\n", uart_receive_len);
		if(uart_receive_len > 0)
			printk("uart_receive=%s\n", uart_receive_buf);
	    /*if(gState->running ==0)
			break;
		size = ksocket_receive(gState->sock, &(gState->socket_address), buf, ETH_FRAME_LEN);
		///--if(size==ETH_FRAME_LEN)
        if(size>0) {
			printk("mac=%02x:%02x:%02x:%02x:%02x:%02x\n", PRINT_MAC(gState->socket_address.sll_addr));
			hex_dump("receive_buf", buf, size);
			//process_handler(gState ,buf, size);
		}*/
/*
#if PRINTK_BUF ==1
        if (gState->dev_state == NTBR_STATE_TDMA) {
            static unsigned int tick_dbg=0;			
            unsigned int tick = GET_TICK_COUNT(gState) % gState->tdma_info.total_slot ; 
#if DUPLICATED_PKT==1			 
            //if(tick_dbg != gState->cycle && (tick > gState->tdma_info.total_slot - 10 &&  tick < gState->tdma_info.total_slot-1)) {
#else
            if(tick_dbg != gState->cycle && (tick > gState->tdma_info.total_slot/2 &&  tick < gState->tdma_info.total_slot-1)) {
#endif
				tick_dbg = gState->cycle;
				if(printk_msg_buffer_idx){
				   printk_msg_buffer[printk_msg_buffer_idx] = 0;
				   //printk("%s", printk_msg_buffer );
				   printk_msg_buffer_idx = 0;
				}
				
            }
        }
#endif
#if UART_SUPPORT==1
		if (gState->dev_state == NTBR_STATE_TDMA) {
			int ret,i,j,rcv_len;
            #define LOOP_COUNT 100			
			char ctmp[500] = "N";
			unsigned int pmmc_base, pmmc_diff;
			unsigned int tick_base, tick_diff ;			
			unsigned int max_pmmc_count;
			unsigned int max_tick_diff;
			static int count=0;
			unsigned int tick;
			static unsigned int tick_uart=0;			
#if 1			
			tick = GET_TICK_COUNT(gState) % gState->tdma_info.total_slot ; 
            if(tick_uart != gState->cycle && (tick > 50 &&  tick < 90)) {		 	
                tick_uart = gState->cycle;         
				while(uart_number >= UART_PKT_SIZE) {

					i = 0;
					rcv_len =0;
					while(1) {
						ret = uart_recv_packet(uart_receive_buff+i,MAX_SEND_LENGTH-i, &rcv_len);
						if(ret==1) { // receive a packet
							break;
						}else if( ret== 0) {
							if(rcv_len>0 && rcv_len < MAX_SEND_LENGTH) {
								i+= rcv_len;
								if(i>=MAX_SEND_LENGTH){
								break;
								}
							}
						}else { // ret  == -1
							break;
						}
					}
                 
					if(ret==1) {
				       // get a new paclet from UART uplayer 
						if(MAX_SEND_LENGTH == rcv_len) {
							if(!memcmp(uart_send_buff,uart_receive_buff,UART_PKT_SIZE)) {
								printk("RG ");
								uart_send_packet("GOOD\r\n",strlen("GOOD\r\n"));
							}else {
								printk("G%d ", uart_number);
								uart_send_packet("FUCK\r\n",strlen("FUCK\r\n"));
							}
						}else {
							sprintf(ctmp,"WOW:%d\r\n",rcv_len);
							uart_send_packet(ctmp,strlen(ctmp));
						}
					}else if(ret== 0) {
						sprintf(ctmp,"WHY:%d\r\n",rcv_len);
						uart_send_packet(ctmp,strlen(ctmp));
					}			   
				}
#if UART_DBG_PRINT==1
				if(msg_buffer_idx) {
					uart_send_packet(msg_buffer,msg_buffer_idx);
					msg_buffer_idx =0;
				}
#endif          			   
				uart_send_packet(uart_send_buff,UART_PKT_SIZE);

			   
			}
#elif 0 
			tick = GET_TICK_COUNT(gState) % gState->tdma_info.total_slot;
            if(tick_uart != gState->cycle && (tick > 50 &&  tick < 90)) {
				//send_char('G');
				//printk("G");
				tick_uart = gState->cycle;
           
				if(uart_number>0) {			
					//send_char('P');
					//printk("P%c ", test_buffer[test_idx_head]);
					//udelay(100);
					i=0;
					while(uart_number>0){
				    send_char(test_buffer[test_idx_head]);
				    test_idx_head = ++test_idx_head % USRT_BUFF_SIZE ;
				    uart_number--;
				    i++;
				    if(i>600)
					   break;
					}
				}
			}
			
#elif 0
			tick = GET_TICK_COUNT(gState) % gState->tdma_info.total_slot ; 
			uart_rx();

            if(tick_uart != gState->cycle && (tick > 50 &&  tick < 90)) {
				//send_char('G');
                tick_uart = gState->cycle;
           
//////////////////////////////////////   Rx Start ////////////////////////////////////////////
//////////////////////////////////////	 Rx End  ////////////////////////////////////////////
//////////////////////////////////////   Put Start ////////////////////////////////////////////

				if(uart_number>0) {
			   	
					//send_char('P');
					//udelay(100);
					i=0;
					while(uart_number>0){
						send_char(test_buffer[test_idx_head]);
						test_idx_head = ++test_idx_head % USRT_BUFF_SIZE ;
						uart_number--;
						i++;
						if(i>600)
							break;
					}
				}

//////////////////////////////////////	 Put End  ////////////////////////////////////////////
			}
#elif 0
		    while(1) {
				tick = GET_TICK_COUNT(gState) % gState->tdma_info.total_slot ; 
	            if( tick > 50 &&  tick < 80 ){				
#if 1
                    i=0;
					while(uart_number>0){
						send_char(test_buffer[test_idx_head]);
						test_idx_head = ++test_idx_head % USRT_BUFF_SIZE ;
						uart_number--;
						i++;
						if(i>600)
							break;
					}
#endif
					i=0;
                    j=0;
					while(uart_number<USRT_BUFF_SIZE){							
						asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (pmmc_base));	  
						tick_base = gState->total_tick_number;

						//ret = recv_char(ctmp);
						//ret = tty_read(0,ctmp);
						ret = serail_get_char_directly(&ctmp[0], 0);
						//printk("rx:%c, ", ch_ret );

						asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (pmmc_diff));	  
						tick_diff = gState->total_tick_number;	
	                    tick_diff = tick_diff - tick_base;
						pmmc_diff = pmmc_diff - pmmc_base;

						//if( tick_diff > 2 || ret!=-1)
						//    printk("\r\n%c: t=%x", ctmp[0], tick_diff );

						if(ret<0) {
							j++;
							if(j>LOOP_COUNT)
								break;
							else {
								udelay(10);
								continue;
							}
						}
#if 1
						test_buffer[test_idx_tail] = ctmp[0];
						test_idx_tail = ++test_idx_tail % USRT_BUFF_SIZE ;
						uart_number++;					   
#endif
						i++;
						if(i>600)
							break;		 
					}
	            }else {
				  if(uart_number<USRT_BUFF_SIZE){		
					  ret = serail_get_char_directly(&ctmp[0], 0);
					  if(ret<0)
						  break;
                      else{
					      test_buffer[test_idx_tail] = ctmp[0];
					      test_idx_tail = ++test_idx_tail % USRT_BUFF_SIZE ;
					      uart_number++;					  
                      }	  
				  }		  
				  break;
				}
		    }
#endif	
		}
#endif
*/		
	}

out:
	NdisFreeSpinLock(&TimerSemLock);
    gState->thread = NULL;
    gState->running = 0;
}

int packet_protocol_check(char *buffer)
{	
	struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;
    return hdr->op_code;
} 

unsigned char mac_address_check(char *buffer)
{	
	struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;
} 

int ksocket_send(struct socket *sock, struct sockaddr_ll *addr, unsigned char *buf, int len)
{
    struct msghdr msg;
    struct iovec iov;
    mm_segment_t oldfs;
    int size = 0;

    if (sock->sk==NULL)
       return 0;

    iov.iov_base = buf;
    iov.iov_len = len;

    msg.msg_flags = 0;
    msg.msg_name = addr;
    msg.msg_namelen  =sizeof(struct sockaddr_ll);
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;	

    oldfs = get_fs();
    set_fs(KERNEL_DS);
    size = sock_sendmsg(sock,&msg,len);
    set_fs(oldfs);

    return size;
}

int ksocket_receive(struct socket* sock, struct sockaddr_ll *addr, unsigned char* buf, int len)
{
    struct msghdr msg;
    struct iovec iov;
    mm_segment_t oldfs;
    int size = 0;

    if (sock->sk==NULL) return 0;

    iov.iov_base = buf;
    iov.iov_len = len;
	
#if NCTU_WAVE==1
    msg.msg_flags = 0;
#else	
    if(gState->dev_state == NTBR_STATE_INIT ||  gState->dev_state == NTBR_STATE_HELO ) 
        msg.msg_flags = 0;
	else
	    msg.msg_flags =MSG_DONTWAIT;
#endif	
	
    msg.msg_name = addr;
    msg.msg_namelen  =sizeof(struct sockaddr_ll);
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;

    oldfs = get_fs();
    set_fs(KERNEL_DS);
    size = sock_recvmsg(sock,&msg,len,msg.msg_flags);
    set_fs(oldfs);

    return size;
}

static void ksocket_exit(void)
{
    int err;

	gState->running = 0;
#if TDMA_FUNCTION_START==1	
	armv7_stop_pmnc();
	armv7_pmnc_disable_counter(CCNT);
    free_irq(INT_34XX_BENCH_MPU_EMUL, NULL);
#endif
	clean_driver_macs(gState);

    if (gState->thread==NULL)
            printk(KERN_INFO ": no kernel thread to kill\n");
    else 
    {
		err=kthread_stop(gState->thread);

        /* wait for kernel thread to die */
        if (err < 0)
                printk(KERN_INFO ": unknown error %d while trying to terminate kernel thread\n",-err);
        else 
        {
                printk(KERN_INFO ": succesfully killed kernel thread!\n");
        }
    }
	OS_TimerListRelease(gState);
#if UART_SUPPORT==1 && UART_DBG_PRINT==1
	if (gState->uart_thread==NULL){
	}
	else 
	{
		err=kthread_stop(gState->uart_thread);

		/* wait for kernel thread to die */
		if (err < 0)
				printk(KERN_INFO ": unknown error %d while trying to terminate uart kernel thread\n",-err);
		else 
		{
				printk(KERN_INFO ": succesfully killed uart kernel thread!\n");
		}
	}
#endif
	

    kfree(gState);
    gState = NULL;
    printk(KERN_INFO ": kernel socket unloaded\n");
}

static int nctu_neighbor_init(void)
{
    int ret;
	gState = kmalloc(sizeof(DEV_MAIN_DISCRIPTION), GFP_KERNEL);
    memset(gState, 0, sizeof(DEV_MAIN_DISCRIPTION));

	ret = register_iobank_drv(iobank_major, devName);
	if(ret > 0){
		DBGPRINT(DEBUG_ERROR, ("%s:register_iobank_drv fail!!\n", __FUNCTION__));
	}
	
    linux_uart_init();
    /* start kernel thread */
    gState->thread = kthread_run((void *)ksocket_start, NULL, MODULE_NAME);
	
    //gState->uart_thread = kthread_run((void *)dbg_thread, NULL, MODULE_NAME);		
	
    if(IS_ERR(gState->thread)) 
    {
		DBGPRINT(DEBUG_ERROR, ("%s:unable to start kernel thread\n", __FUNCTION__));
        kfree(gState);
        gState = NULL;
    }
	return 0;	
}

static void nctu_neighbor_exit(void)
{
	ksocket_exit();
#if UART_SUPPORT == 1
	linux_uart_close();
#endif	
	unregister_iobank_drv(iobank_major, devName);
}

module_init(nctu_neighbor_init);
module_exit(nctu_neighbor_exit);
	
const char * get_system_state(DEV_MAIN_DISCRIPTION *pDesc)
{
	const char *pRet;

    switch(pDesc->dev_state) {

		case NTBR_STATE_INIT:
			pRet = "STATE_INIT";
			break;
		case NTBR_STATE_HELO:
			pRet = "STATE_HELLO";			
			break;
		case NTBR_STATE_IFNO_CHANGE:
			pRet = "STATE_IFNO_CHANGE";
			break;
		case NTBR_STATE_STEADY:
			pRet = "STATE_STEADY";
			break;
		case NTBR_STATE_ASSIGN_MASTER:
			pRet = "STATE_ASSIGN_MASTER";			
			break;
		case NTBR_STATE_TDMA_INIT:
			pRet = "STATE_TDMA_INIT";
			break;
		case NTBR_STATE_TDMA:
			pRet = "STATE_TDMA";			
			break;
		default:
			pRet = "STATE_UNKNOWN";
			break;

    }
    return pRet;
}

const char * get_system_sub_state(DEV_MAIN_DISCRIPTION *pDesc)
{
	const char *pRet;

    switch(pDesc->sub_state) {

		case NTBR_SUBSTATE_READY_FORWARD:
			pRet = "READY_FORWARD";
			break;
		case NTBR_SUBSTATE_RECEIVED_FORWARD:
			pRet = "RECEIVED_FORWARD";			
			break;
		case NTBR_SUBSTATE_FORWARD_DONE:
			pRet = "FORWARD_DONE";
			break;
		default:
			pRet = "STATE_UNKNOWN";
			break;

    }
    return pRet;
}

static struct semaphore iobank_sem;
static struct file_operations iobank_fops = {0};

void IOBankMutexGet (void)
{
	down(&iobank_sem);
}

void IOBankMutexRelease (void)
{
	up(&iobank_sem);
}	

static int iobank_ioctl(struct inode *inode, struct file *filp, unsigned int nCmd, unsigned long nArg)
{
	int i;
	int ret = 0;
	///--union dray_io_param cmd;
	unsigned int fb;	
	unsigned char *buf;
	unsigned int pmmc_count;

	//printk("iobank_ioctl nCmd = %d, nArg=%d \n", nCmd, nArg );

	
   IOBankMutexGet();
   switch (nCmd)
   {
		case 144:
            switch(nArg) {
				case 0:			
					printk("************ current state = %s, sub state=%s ************\r\n", get_system_state(gState), get_system_sub_state(gState) );
					printk("** timeslot id=%d, slot0=%d, slot1=%d **\r\n", gState->tdma_info.id, gState->tdma_info.timeslot[0],  gState->tdma_info.timeslot[1]);
					printk("** Start delay tick=%d, save tick=%d, save pmmc =0x%x ***\r\n", gState->tdma_info.tdms_start_delay_ticks, gState->tdma_info.tick_count,  gState->tdma_info.tick_pmmc_value);
					asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (pmmc_count));	
					printk("** current pmmc = %x, diff pmmc=%d, diff tick=%d \r\n", pmmc_count, gState->pro_delay.diff_tick_pmmc_value, gState->pro_delay.diff_tick_count );
					printk("** total ticks = %d\r\n", gState->total_tick_number );

                    if(gState->forward_node)
					    printk("** Forwarding Mode MAC %02x:%02x:%02x:%02x:%02x:%02x\r\n", PRINT_MAC(((struct dllist *)(gState->forward_node))->mac));
					else
						printk("** It's the last node\r\n");

					printk("** Rx/s = %d, Tx/s = %d, 400ms ticks=%d\r\n", gState->tdma_status.avg_rx_sec, gState->tdma_status.avg_tx_sec,  gState->tdma_status.mlme_ticks );
					printk("** Total Rx = %d, Total Tx = %d\r\n", gState->tdma_status.total_rx_count, gState->tdma_status.total_tx_count );
					printk("** MAX tick diff = %d, MAX PMMC diff = %d\r\n", gState->tdma_status.diff_max.ticks, gState->tdma_status.diff_max.pmmc);
					printk("** Min tick diff = %d, Min PMMC diff = %d\r\n", gState->tdma_status.diff_min.ticks, gState->tdma_status.diff_min.pmmc);
					break;
				case 1:
					printk("************  display_node_info  ************\n");
					display_node_info(gState->head);
					break;
				case 2:
					printk("************  Driver: Uplayer_Show_MacTable_Proc  **************\n");
					buf = kmalloc(4096, GFP_KERNEL);
					if(buf){
#if 1				
						Uplayer_Show_MacTable_Proc(buf);
						printk("%s",buf);
						kfree(buf);
#endif				
					}					
					break;
				case 3:
					Display_System_neighbor_Table( gState );
					break;
            }
			break;
		default:
		  	/* magic is not known - return error */
		  	printk("unknown %d\r\n", nCmd);
		  	ret = -1;
		  	break;
	}
   IOBankMutexRelease();
   return ret;
}
	
int register_iobank_drv (unsigned int major, char* dev_name)
{
   int ret = 0;
   unsigned int majorNumber = major;

	init_MUTEX(&iobank_sem);
	if(iobank_fops.ioctl == NULL) {
		iobank_fops.owner = NULL;
		iobank_fops.read  = NULL;
		iobank_fops.write = NULL;
		iobank_fops.poll  = NULL;
		iobank_fops.ioctl = iobank_ioctl;
		iobank_fops.open  = NULL;
		iobank_fops.release = NULL;
	}
#if 0
   /* copy registration info from Low level driver */
   majorNumber = pLLDrvCtx->majorNumber;

   /* pointer to static driver name storage (used for driver registration) */
   pRegDrvName = pHLDrvCtx->registeredDrvName;

#endif
   /* limit devNodeName to 8 characters */
   //sprintf (pRegDrvName, "%.8s", dev_name);
   /* Register the character device */
   ret = register_chrdev (majorNumber, dev_name, &iobank_fops);
   if (ret < 0) {
   	  printk("register fail=%d!!!!\n", ret);
      return ret;
   }
   return ret;
}

void unregister_iobank_drv (unsigned int major, char* dev_name)
{
    unregister_chrdev(major, dev_name);
}
