#ifndef _NCTUBR_H_
#define _NCTUBR_H_


#define TIME_SLOT_ARRAY_LEN         10

#define BUFFER_SIZE  8192
///--#define MLME_TASK_EXEC_INTV         2000  // 400ms
#define MLME_TASK_EXEC_INTV         1000  // 1S


#if NCTU_SIGNAL_SEARCH == 1
///--#define BROADCAST_DURATION          1000  // HZ has been changed .... 200ms, 200us ??
#define BROADCAST_DURATION          300  // 300ms after modify kernel after Frank
#else
///--#define HELLO_TIMEOUT               25000 // 5 second
//#define HELLO_TIMEOUT               2500000 // 500 second for test
//#define HELLO_TIMEOUT               70000 // 14 second for test
#define HELLO_TIMEOUT               20000000 // 4000 second for test
#define EDGE_HELLO_TIMEOUT          50000 // 10 second for test
#define BROADCAST_DURATION          5000  // HZ has been changed .... 1 second
#endif

#if NCTU_SIGNAL_SEARCH == 1
#define INFO_CHANGE_TIME            3000000  // 600 seconds
#else
#define INFO_CHANGE_TIME            10000  // 2 second
#endif

#define EDGE_INFO_CHANGE_TIME          INFO_CHANGE_TIME + 10000  // 2 second added


#define TDMA_DELAY_TICK                              15000  // 15000 * 4.1ms = 61.8 s
#define TDMA_MAINTAIN_TIME                          900000 // 180 seconds

#if NCTU_SIGNAL_SEARCH == 1
#define BROADCAST_MASTER_TRANSIT_TIME               1000  // 200ms
#define BROADCAST_MASTER_MONITOR_TIME            20000000 // 4000 second for test
///--#define NEIGHBOR_TABLE_SENDBACK_TIME               10000  // 2 second
#define NEIGHBOR_TABLE_SENDBACK_TIME               400



#define NEIGHBOR_TABLE_FORWARD_DONE_WAIT_TDMA_TIME  900000  // 180 seconds

#if JUST_TDMA ==1
#define START_SIGNAL_PERCENTAGE     100
#else
#define START_SIGNAL_PERCENTAGE     5
#endif

#define SIGNAL_STEP_VALUE          25
#define SIGNAL_CHANGE_TIME          1

#define BROADCAST_RETRY_TIME        2

#endif

#define RECORD_SENSOR_DATA    5

typedef struct _FORWARD_ADDR_INFO {
    char in_mac[ETH_MAC_LEN];
	char out_mac[ETH_MAC_LEN];
	int len;
	char buffer[ETH_FRAME_LEN];
	unsigned int seq_num;
}FORWARD_ADDR_INFO;

typedef struct _TDMA_INFO {
	unsigned int tdms_start_delay_ticks;
	unsigned int tick_count;
	unsigned int tick_pmmc_value;
	unsigned int id;
	int timeslot[TIME_SLOT_ARRAY_LEN];
	int total_slot;
}TDMA_INFO;

typedef struct _TDMA_LOG {
	int diff_tick_count;
	int diff_tick_pmmc_value;
}TDMA_LOG;

typedef struct _BUFF_DATA {
	int len;
	unsigned int sequence;
	char data[ETH_FRAME_LEN*4];
}BUFF_DATA;


typedef struct _TDMA_DIFF_DATA {
	int ticks;
	int pmmc;	
}TDMA_DIFF_DATA;

typedef struct _TDMA_STATUS {
	TDMA_DIFF_DATA diff_min;
	TDMA_DIFF_DATA diff_max;
	TDMA_DIFF_DATA diff_avg;
	unsigned int total_rx_count;
	unsigned int total_rx_out_count;
	unsigned int total_tx_count;
	unsigned int avg_rx_min;
	unsigned int avg_tx_min;	
    unsigned int avg_rx_sec;
	unsigned int avg_tx_sec;	
	unsigned int mlme_ticks;	
}TDMA_STATUS;


typedef struct _TDMA_STATUS_TEMP {	
	
	unsigned int ticks;
	unsigned int pmmc;
	
	unsigned int current_hz;
	unsigned int current_tx_num;
	unsigned int current_rx_num;	
	unsigned int current_tick;
}TDMA_STATUS_TEMP;



typedef struct _SENSOR_DATA {	
	struct _msg_data_header header;
	struct _wsn_data_format data[RECORD_SENSOR_DATA];
}SENSOR_DATA;

#if TDMA_DEBUUG_LOG==1

typedef struct _TDMA_DATA_RX {	
	unsigned int  count;
	unsigned char data[ MAX_TEST_NUMBER];
	
}TDMA_DATA_RX;


#endif

typedef struct _DEV_MAIN_DISCRIPTION {
  int   dev_state;
  void *wifi;
  void *powerline;
  struct dllist *lnode;  
  unsigned char src_mac[ETH_MAC_LEN];

  unsigned int flag;
  struct sockaddr_ll socket_address;
  //time_t   sys_timer;
  unsigned int sys_timer;
  unsigned short sequence;
  unsigned int signal_level;

  struct task_struct *thread;
  struct task_struct *uart_thread;
  struct socket *sock;
  struct sockaddr_in addr;
  int running;
#if NCTU_SIGNAL_SEARCH == 1
  int retry;
  int scan_step;
  unsigned int master;
#endif
  LIST_HEADER  RscTimerCreateList; /* timers list */
  BR_TIMER_STRUCT     PeriodicTimer;
  BR_TIMER_STRUCT     TimeoutTimer;  
  	
  spinlock_t list_lock;
  spinlock_t buffer_lock;
  struct dllist *head; 
  struct dllist *tail;
  struct dllist *forward_node;
  struct dllist *backward_node;
  
  int sub_state;

  FORWARD_ADDR_INFO forward_addr;
  unsigned char left_mac[ETH_MAC_LEN];

  
  TDMA_INFO  tdma_info;
  TDMA_INFO  tdma_old;
  TDMA_LOG   pro_delay;
  BUFF_DATA  buffer;

  unsigned int     total_tick_number;
  TDMA_STATUS      tdma_status;
  TDMA_STATUS_TEMP tdma_count;
  unsigned int re_sync_count;

  unsigned int cycle;  
#if MULTI_PKYS_PER_SLOT==1
  unsigned int frames;  
#endif
  SENSOR_DATA wsn_data;
  unsigned int get_sync_count;
#if TDMA_DEBUUG_LOG == 1
  unsigned int test_count;
  TDMA_DATA_RX  rx_check[10];  // the maximum numbers for packet lost test
  TDMA_DATA_RX  duplicate[10];
   TDMA_DATA_RX  rx_out[10];
  unsigned int start_test;
#endif  	
#if JUST_TDMA ==1
   unsigned int previous_mac_set;
   unsigned int next_mac_set;
#endif
}DEV_MAIN_DISCRIPTION;

#if DUPLICATED_PKT==1
#if JUST_TDMA ==1
#define RE_SYNC_CYCLY_COUNT         15 // double size if duplicated packets is 3
#else
#define RE_SYNC_CYCLY_COUNT         60 
#endif
#else
#if JUST_TDMA ==1
#define RE_SYNC_CYCLY_COUNT         5 // 10 * timeslot numbers per frame
#else
#define RE_SYNC_CYCLY_COUNT         20 // 10 * timeslot numbers per frame
#endif
#endif

#define TICK_COUNT_DOWN_INIT_VALUE  19500


#define MODULE_NAME "nctu_neighbor"


#define TDMA_FUNCTION_START          1

#define IOBANK_MAJOR				239
#define IOBANK_MINOR				10


#endif
