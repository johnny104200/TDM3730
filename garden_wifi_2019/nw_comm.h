#ifndef _NCTU_COMM_H_
#define _NCTU_COMM_H_


#define NCTU_SIGNAL_SEARCH                   1  // Enable new state machine for neighbor node search


#define NCTU_WAVE                            1  // Enable new state machine for neighbor node search

///--#define IP_HELF_HEADER   
#define UART_SUPPORT           1

#define TDMA_DEBUUG_LOG        1
#define MAX_TEST_NUMBER       2000

#define PRESYNC_TX_PKTS        60

#define DOUBLE_SLOT_SIZE       0

#define DYNAMIC_SLOT_ASSIGN    1

#define JUST_TDMA              1

#define DUPLICATED_PKT         0

#define TDMA_DEBUUG_LEAST_SYNC_NUM  3

#define MULTI_PKYS_PER_SLOT        0
#define MULTI_PKYS_PER_SLOT_NUM    1

////////////////////////////////////////////////////////////
//   message type
////////////////////////////////////////////////////////////

#define NT_MSG_HELLO                         1
#define NT_MSG_TABLE_CHANGE                  2
#define NT_MSG_ACK                           3
#define NT_MSG_TDMA_ASSIGN                   4
#define NT_MSG_TDMA_ASSIGN_ACK               5

#if NCTU_SIGNAL_SEARCH == 1
#define NT_MSG_MASTER_ASSIGN                 6
#define NT_MSG_MASTER_ASSIGN_ACK             7
#define NT_MSG_TO_EDGE_FORWARD               8
#define NT_MSG_RECEIVE_FORWARD_ACK           9
#endif
#define NT_MSG_MASTER_FINISH                10
#define NT_MSG_MASTER_FINISH_ACK            11


#define NT_MSG_TDMA_CONFIRMM                12
#define NT_MSG_TDMA_CONFIRMM_ACK            13

#define NT_MSG_DATA                         14

////  sub type ////////////

#define    NT_MSG_SUBTYPE_TDMA               1
#if NCTU_SIGNAL_SEARCH == 1
#define    NT_MSG_SUBTYPE_SEARCH_DONE        2
#endif

#define    NT_MSG_SUBTYPE_TEST_COUNT        10






////////////////////////////////////////////////////////////
//  system state
////////////////////////////////////////////////////////////


#define NTBR_STATE_INIT                      0
#define NTBR_STATE_HELO                      1
#define NTBR_STATE_IFNO_CHANGE               2
#define NTBR_STATE_STEADY                    3
#define NTBR_STATE_TDMA                      4
#if NCTU_SIGNAL_SEARCH == 1
#define NTBR_STATE_ASSIGN_MASTER             5
#endif
#define NTBR_STATE_TDMA_INIT                 6

#define NTBR_STATE_DEBUG                     10
//////////////////////////////
//  system sub-state
//////////////////////////////


#define    NTBR_SUBSTATE_READY_FORWARD     1
#define    NTBR_SUBSTATE_RECEIVED_FORWARD  2
#define    NTBR_SUBSTATE_FORWARD_DONE      3



////////////////////////////////////////////////////////////
//  data type define
////////////////////////////////////////////////////////////


#define TIME_HELLO             10 // 10 seconds
#define TIME_INFO_CHANGE       10

#define NCTU_COMM_VER          1

#define ETH_MAC_LEN     6


#ifdef IP_HELF_HEADER
#define LOCATION_COMM_PROTO  0x0800
#else
#define LOCATION_COMM_PROTO  0x8888
#endif

struct location_commu_encaps_hdr {
	struct ethhdr ethhdr;
#ifdef IP_HELF_HEADER
	char   ip[20];
#endif
	unsigned short dst_addr;
	unsigned short hop_addr;
	unsigned short src_addr;
	/* CSM_ENCAPS HEADER */
	unsigned char  version;
	unsigned char  flag;
	unsigned short op_code;
	unsigned short seq_num;
	short          txpower;
	unsigned int   time_stamp;	
	unsigned int   tdma_count;
	int            diff_pmmc;
	int            diff_tick;
	unsigned int   info;
	unsigned int   frames;
	unsigned short len;
	unsigned short crc;
} __attribute__((packed));  // 38 + 4 + 14 = 52

#define ETH_NAME "eth6"

#define NORMAL_NODE      0
#define LEFT_EDGE_NODE   1
#define RIGHT_EDGE_NODE  2

#define GET_ROLE(x)  (x & 0x03)

#define GET_SYNC_BIT(x)    ((x & 0x04)>>2)
#define SET_SYNC_BIT(x,v)  ( x | ((v<<2)&0x04))

#define GET_RSYNC_REQ(x)    ((x & 0x08)>>3)
#define SET_RSYNC_REQ(x,v)  ( x | ((v<<3)&0x08))



#define MAX_CMD_LEN  128

#define ETH_FRAME_LEN 32


#define CURRENT_MIX_DATA        0
#define FORWARDING_DATA_ONY     1
#define SPECAIL_CMD_DATA        2

struct _msg_data_header {
    unsigned short type;
	unsigned short len;
    unsigned long node_id;
	unsigned long timestamp;
	unsigned long sequence;
} __attribute__((packed));


#define WSN_DATA_LENGTH  24
// << node id | wsn id  | timestamp |  data >>
struct _wsn_data_format {	
	unsigned long wsn_id;	
	char data[WSN_DATA_LENGTH];	
} __attribute__((packed));

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define DBG

#define DEBUG_OFF		0
#define DEBUG_ERROR		1
#define DEBUG_WARN		2
#define DEBUG_ALL		3
#define DEBUG_TDMA		4



#ifdef DBG

extern unsigned long	DebugLevel;

#define DBGPRINT_RAW(Level, Fmt)    \
do{                                   \
    if (Level <= DebugLevel)      \
    {                               \
        printk Fmt;               \
    }                               \
}while(0)

#define DBGPRINT(Level, Fmt)    DBGPRINT_RAW(Level, Fmt)


#define DBGPRINT_ERR(Fmt)           \
{                                   \
    printk("ERROR!!! ");          \
    printk Fmt;                  \
}

#define DBGPRINT_S(Status, Fmt)		\
{									\
	printk Fmt;					\
}
#else
#define DBGPRINT(Level, Fmt)
#define DBGPRINT_RAW(Level, Fmt)
#define DBGPRINT_S(Status, Fmt)
#define DBGPRINT_ERR(Fmt)
#endif

#undef  ASSERT
#ifdef DBG
#define ASSERT(x)                                                               \
{                                                                               \
    if (!(x))                                                                   \
    {                                                                           \
        printk(KERN_WARNING __FILE__ ":%d assert " #x "failed\n", __LINE__);    \
    }                                                                           \
}
#else
#define ASSERT(x)
#endif // DBG //


#define FAILED  -1
#define SCUESS   0

#define MAX_TICK_COUNT_NUM   0x10000
#if DOUBLE_SLOT_SIZE == 1
#define DELAY_TICK_COUNT     3000
#else
#define DELAY_TICK_COUNT     6000     // about 25seconds
#endif

//#define INC_TICK_COUNT(x)   x->tdma_info.tick_count=(x->tdma_info.tick_count+1)&(MAX_TICK_COUNT_NUM-1)
#define INC_TICK_COUNT(x)   x->tdma_info.tick_count++

#define INIT_TICK_COUNT(x)  x->tdma_info.tick_count=0

#define GET_TICK_COUNT(x)   x->tdma_info.tick_count

#define SET_TDMA_START_DELAY_TICK(x,value)   x->tdma_info.tdms_start_delay_ticks = value
#define GET_TDMA_START_DELAY_TICK(x)         x->tdma_info.tdms_start_delay_ticks

#define SYNC_DONE     0
#define RE_SYNC_REQ   1
#define RE_SYNC_SET   2


#endif
