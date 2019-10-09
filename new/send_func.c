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

int fill_tdma_slot_payload(DEV_MAIN_DISCRIPTION *pDesc, unsigned char *buf, int maximum_len );
int ksocket_send(struct socket *sock, struct sockaddr_ll *addr, unsigned char *buf, int len);
const char *STATE_STR( int dev_state);
int Uplayer_layer_Set_Power(char *arg); // wireless driver 
int build_neighbor_list_data(DEV_MAIN_DISCRIPTION *pDesc, unsigned char *buf, int maximum_len );


int send_cmm_broad_packet(DEV_MAIN_DISCRIPTION *pDesc , unsigned char *dest_mac, char *buffer, int length)
{
    int send_result = 0;
	struct ethhdr *eh= (struct ethhdr *)buffer;
	struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;
	unsigned char *dest_addr;
	unsigned char b_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	if (dest_mac)
		dest_addr = dest_mac;
	else
		dest_addr = b_mac;

	/*set the frame header*/
	memcpy((void*)buffer, (void*)dest_addr, ETH_MAC_LEN);
	memcpy((void*)(buffer+ETH_MAC_LEN), (void*)pDesc->src_mac, ETH_MAC_LEN);
	eh->h_proto = htons(LOCATION_COMM_PROTO);
		
	/*fill the frame with some data*/
	hdr->version = NCTU_COMM_VER;
	hdr->op_code = htons(NT_MSG_HELLO);				
	hdr->seq_num = pDesc->sequence++; 
	hdr->flag = GET_ROLE(pDesc->flag);
	//hdr->seq_num = 1;
	//hdr->flag =2;
	hdr->len = sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr);
	//hdr->crc = 0;
	hdr->crc = csum((unsigned short *)(buffer + sizeof(struct ethhdr)), hdr->len );
	
	/*send the packet*/
	send_result = ksocket_send(pDesc->sock,&(pDesc->socket_address), buffer, length);
	if (send_result == -1) {
		DBGPRINT(DEBUG_ERROR, ("%s:send error, state=%s\n", __FUNCTION__, STATE_STR(pDesc->dev_state)));
	}
				

}			


int send_cmm_trigger_packet(DEV_MAIN_DISCRIPTION *pDesc , unsigned char *dest_mac, char *buffer, int length)
{
    int send_result = 0;
	struct ethhdr *eh= (struct ethhdr *)buffer;
	struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;
	unsigned char *dest_addr;
	char tmp[10];
	unsigned char b_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	if(dest_mac)
		dest_addr = dest_mac;
	else
		dest_addr = b_mac;
	
	
	// set driver tx signal level
	sprintf( tmp,"%d",pDesc->signal_level);
	printk("power=%s\n", tmp );
	Uplayer_layer_Set_Power(tmp);


	/*set the frame header*/
	memcpy((void*)buffer, (void*)dest_addr, ETH_MAC_LEN);
	memcpy((void*)(buffer+ETH_MAC_LEN), (void*)pDesc->src_mac, ETH_MAC_LEN);
	eh->h_proto = htons(LOCATION_COMM_PROTO);
		
	/*fill the frame with some data*/
	hdr->version = NCTU_COMM_VER;
	hdr->op_code = htons(NT_MSG_HELLO);				
	hdr->seq_num = pDesc->sequence++; 
	hdr->flag = GET_ROLE(pDesc->flag);
	hdr->txpower = htons((short)pDesc->signal_level);
	hdr->len = sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr);
	hdr->crc = csum((unsigned short *)(buffer + sizeof(struct ethhdr)), hdr->len );

	///--printk("send_cmm_trigger_packet out...\n");
	
	/*send the packet*/
	send_result = ksocket_send(pDesc->sock,&(pDesc->socket_address), buffer, length);
	if (send_result == -1) { 
		DBGPRINT(DEBUG_ERROR, ("%s:send error, state=%s\n", __FUNCTION__, STATE_STR(pDesc->dev_state)));
	}							
	return send_result;
}

int send_cmm_ack_packet(DEV_MAIN_DISCRIPTION *pDesc , unsigned char *dest_mac, char *buffer, int length, int type, unsigned int seq)
{
    int send_result = 0;
	struct ethhdr *eh= (struct ethhdr *)buffer;
	struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;
	unsigned char *dest_addr;
	char tmp[10];
	unsigned char b_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	if (dest_mac)
		dest_addr = dest_mac;
	else
		dest_addr = b_mac;


	/*set the frame header*/
	memcpy((void*)buffer, (void*)dest_addr, ETH_MAC_LEN);
	memcpy((void*)(buffer+ETH_MAC_LEN), (void*)pDesc->src_mac, ETH_MAC_LEN);
	eh->h_proto = htons(LOCATION_COMM_PROTO);
		
#if JUST_TDMA==1
	hdr->src_addr = pDesc->tdma_info.id;
    hdr->hop_addr = 0;  /// we use the field to store sending mode for test ...
	hdr->dst_addr = pDesc->tdma_info.id - 1;
#endif	
		
	/*fill the frame with some data*/
	hdr->version = NCTU_COMM_VER;
	hdr->op_code = htons(type);				
	hdr->seq_num = seq; 
	hdr->flag = GET_ROLE(pDesc->flag);
	hdr->txpower = 0;
	if(type == NT_MSG_TDMA_ASSIGN_ACK) 
		hdr->len = length - sizeof(struct ethhdr); // use maximum size to count round trip delay
	else
	    hdr->len = sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr);
	hdr->crc = csum((unsigned short *)(buffer + sizeof(struct ethhdr)), hdr->len );
	
	/*send the packet*/
	send_result = ksocket_send(pDesc->sock,&(pDesc->socket_address), buffer, length);
	if (send_result == -1) { 
		DBGPRINT(DEBUG_ERROR, ("%s:send error, state=%s\n", __FUNCTION__, STATE_STR(pDesc->dev_state)));
	}							
	return send_result;
}


int send_cmm_neighbor_info_packet(DEV_MAIN_DISCRIPTION *pDesc , unsigned char *dest_mac, char *buffer, int length, char *append, int app_length)
{
    int send_result = 0;
	struct ethhdr *eh= (struct ethhdr *)buffer;
	struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;
	unsigned char *dest_addr;
	unsigned char b_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	if (dest_mac)
		dest_addr = dest_mac;
	else
		dest_addr = b_mac;
	    
	int data_len;
    // make a neighbor sorting list packets
    memcpy((void*)buffer, (void*)dest_addr, ETH_MAC_LEN);
	memcpy((void*)(buffer+ETH_MAC_LEN), (void*)pDesc->src_mac, ETH_MAC_LEN);
	eh->h_proto = htons(LOCATION_COMM_PROTO);
	/*fill the frame with some data*/
	hdr->version = NCTU_COMM_VER;
	hdr->op_code = htons(NT_MSG_TABLE_CHANGE);
	hdr->seq_num = pDesc->sequence++; 
	pDesc->forward_addr.seq_num = hdr->seq_num;
	
	data_len =	build_neighbor_list_data( pDesc, buffer + sizeof(struct location_commu_encaps_hdr), length- sizeof(struct location_commu_encaps_hdr));

	if(append && (app_length + data_len + sizeof(struct location_commu_encaps_hdr) < length) ) {
		memcpy(buffer+sizeof(struct location_commu_encaps_hdr) + data_len, append, app_length);
		data_len += app_length;
	}
	hdr->len = sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr) + data_len ;
	hdr->crc = csum((unsigned short *)(buffer + sizeof(struct ethhdr)), hdr->len );
	printk("NTBR_STATE_IFNO_CHANGE, len=%d\n", hdr->len );
	/*send the packet*/
	send_result = ksocket_send(pDesc->sock,&(pDesc->socket_address), buffer, length);
	if (send_result == -1) { 
	  DBGPRINT(DEBUG_ERROR, ("%s:send error, state=%s\n", __FUNCTION__, STATE_STR(pDesc->dev_state)));
	}				 
	return send_result;
}


int send_cmm_assign_broadcast_node_packet(DEV_MAIN_DISCRIPTION *pDesc , unsigned char *dest_mac, char *buffer, int length)
{
    int send_result = 0;
	struct ethhdr *eh= (struct ethhdr *)buffer;
	struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;
	unsigned char *dest_addr;
	unsigned char b_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	if (dest_mac)
		dest_addr = dest_mac;
	else
		dest_addr = b_mac;
	    
	int data_len;
    // make a neighbor sorting list packets
    memcpy((void*)buffer, (void*)dest_addr, ETH_MAC_LEN);
	memcpy((void*)(buffer+ETH_MAC_LEN), (void*)pDesc->src_mac, ETH_MAC_LEN);
	eh->h_proto = htons(LOCATION_COMM_PROTO);
	/*fill the frame with some data*/
	hdr->version = NCTU_COMM_VER;
	hdr->op_code = htons(NT_MSG_MASTER_ASSIGN);
	hdr->seq_num = pDesc->sequence++; 

	hdr->len = sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr);
	hdr->crc = csum((unsigned short *)(buffer + sizeof(struct ethhdr)), hdr->len );
	printk("send_cmm_assign_broadcast_node_packet, len=%d\n", hdr->len );
	/*send the packet*/
	send_result = ksocket_send(pDesc->sock,&(pDesc->socket_address), buffer, length);
	if (send_result == -1) { 
	  DBGPRINT(DEBUG_ERROR, ("%s:send error, state=%s\n", __FUNCTION__, STATE_STR(pDesc->dev_state)));
	}
	return send_result;
}

int send_cmm_packet_hello(DEV_MAIN_DISCRIPTION *pDesc , unsigned char *dest_mac, char *buffer, int length)
{
    int send_result = 0;
	struct ethhdr *eh= (struct ethhdr *)buffer;
	struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;
	unsigned char *dest_addr;
	unsigned char b_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	if (dest_mac)
		dest_addr = dest_mac;
	else
		dest_addr = b_mac;
	    
	int data_len;
    // make a neighbor sorting list packets
    memcpy((void*)buffer, (void*)dest_addr, ETH_MAC_LEN);
	memcpy((void*)(buffer+ETH_MAC_LEN), (void*)pDesc->src_mac, ETH_MAC_LEN);
	eh->h_proto = htons(LOCATION_COMM_PROTO);
	/*fill the frame with some data*/
	hdr->version = NCTU_COMM_VER;
	hdr->op_code = htons(NT_MSG_HELLO);
	hdr->seq_num = pDesc->sequence++; 

	hdr->len = sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr);
	hdr->crc = csum((unsigned short *)(buffer + sizeof(struct ethhdr)), hdr->len );
	printk("send_cmm_packet_hello, len=%d\n", hdr->len );
	/*send the packet*/
	send_result = ksocket_send(pDesc->sock,&(pDesc->socket_address), buffer, length);
	if (send_result == -1) { 
	  DBGPRINT(DEBUG_ERROR, ("%s:send error, state=%s\n", __FUNCTION__, STATE_STR(pDesc->dev_state)));
	}
	return send_result;

}



int send_cmm_packet(DEV_MAIN_DISCRIPTION *pDesc , unsigned char *dest_mac, char *buffer, int length)
{
    int send_result = 0;
	struct ethhdr *eh= (struct ethhdr *)buffer;
	struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;
	unsigned char *dest_addr;
	unsigned char b_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	if (dest_mac)
		dest_addr = dest_mac;
	else
		dest_addr = b_mac;
	    

    switch(pDesc->dev_state) {
		case NTBR_STATE_INIT:
			{
				/*set the frame header*/
				memcpy((void*)buffer, (void*)dest_addr, ETH_MAC_LEN);
				memcpy((void*)(buffer+ETH_MAC_LEN), (void*)pDesc->src_mac, ETH_MAC_LEN);
				eh->h_proto = htons(LOCATION_COMM_PROTO);
              #ifdef IP_HELF_HEADER
                ip->version = 4;
				ip->ihl = 20;
                ip->tot_len = length - sizeof(struct ethhdr );
              #endif				
					
				/*fill the frame with some data*/
				hdr->version = NCTU_COMM_VER;
				hdr->op_code = htons(NT_MSG_HELLO);				
				hdr->seq_num = pDesc->sequence++; 
				hdr->flag = GET_ROLE(pDesc->flag);
				//hdr->seq_num = 1;
				//hdr->flag =2;
				hdr->len = sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr);
				//hdr->crc = 0;
				hdr->crc = csum((unsigned short *)(buffer + sizeof(struct ethhdr)), hdr->len );
				
				/*send the packet*/
				send_result = ksocket_send(pDesc->sock,&(pDesc->socket_address), buffer, length);
				if (send_result == -1) { 
					DBGPRINT(DEBUG_ERROR, ("%s:send error, state=%s\n", __FUNCTION__, STATE_STR(pDesc->dev_state)));
				}
				
			}
			break;
		case NTBR_STATE_HELO:
			{
				memcpy((void*)buffer, (void*)dest_addr, ETH_MAC_LEN);
				memcpy((void*)(buffer+ETH_MAC_LEN), (void*)pDesc->src_mac, ETH_MAC_LEN);
				eh->h_proto = htons(LOCATION_COMM_PROTO);

            #ifdef IP_HELF_HEADER
                ip->version = 4;
				ip->ihl = 20;
				ip->tot_len = length - sizeof(struct ethhdr );
            #endif
				
				/*fill the frame with some data*/
				hdr->version = NCTU_COMM_VER;
				hdr->op_code = htons(NT_MSG_HELLO);
				hdr->seq_num = pDesc->sequence++; 
				hdr->flag = GET_ROLE(pDesc->flag);
				//hdr->seq_num = 1;
				//hdr->flag =2;
				
				hdr->len = sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr);
				//hdr->crc = 0;
				hdr->crc = csum((unsigned short *)(buffer + sizeof(struct ethhdr)),  hdr->len );
				printk("NTBR_STATE_HELO, len=%d\n", hdr->len );
				/*send the packet*/
				send_result = ksocket_send(pDesc->sock,&(pDesc->socket_address), buffer, length);
				if (send_result == -1) { 
				   DBGPRINT(DEBUG_ERROR, ("%s:send error, state=%s\n", __FUNCTION__, STATE_STR(pDesc->dev_state)));
				}
				
			}
			break;
		case NTBR_STATE_IFNO_CHANGE:
		    {
				int data_len;
                // make a neighbor sorting list packets
                memcpy((void*)buffer, (void*)dest_addr, ETH_MAC_LEN);
				memcpy((void*)(buffer+ETH_MAC_LEN), (void*)pDesc->src_mac, ETH_MAC_LEN);
				eh->h_proto = htons(LOCATION_COMM_PROTO);
				/*fill the frame with some data*/
				hdr->version = NCTU_COMM_VER;
				hdr->op_code = htons(NT_MSG_TABLE_CHANGE);
				hdr->seq_num = pDesc->sequence++; 

				data_len =  build_neighbor_list_data( pDesc, buffer + sizeof(struct location_commu_encaps_hdr), length- sizeof(struct location_commu_encaps_hdr));
				hdr->len = sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr) + data_len ;
				hdr->crc = csum((unsigned short *)(buffer + sizeof(struct ethhdr)), hdr->len );
				printk("NTBR_STATE_IFNO_CHANGE, len=%d\n", hdr->len );
				/*send the packet*/
				send_result = ksocket_send(pDesc->sock,&(pDesc->socket_address), buffer, length);
				if (send_result == -1) { 
				  DBGPRINT(DEBUG_ERROR, ("%s:send error, state=%s\n", __FUNCTION__, STATE_STR(pDesc->dev_state)));
				}
                
			}		
			break;

    }
	return send_result;
}


int send_cmm_packet_ack(DEV_MAIN_DISCRIPTION *pDesc ,unsigned int seq, unsigned char *dest_mac, char *buffer, int length)
{
    int send_result = 0;
	struct ethhdr *eh= (struct ethhdr *)buffer;
	struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;

	unsigned char *dest_addr;
	unsigned char b_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	if (dest_mac)
		dest_addr = dest_mac;
	else
		dest_addr = b_mac;
	

	/*set the frame header*/
	memcpy((void*)buffer, (void*)dest_addr, ETH_MAC_LEN);
	memcpy((void*)(buffer+ETH_MAC_LEN), (void*)pDesc->src_mac, ETH_MAC_LEN);
	eh->h_proto = htons(LOCATION_COMM_PROTO);
		
	/*fill the frame with some data*/
	hdr->version = NCTU_COMM_VER;
	hdr->op_code = htons(NT_MSG_ACK);				
	hdr->seq_num = seq; 
	hdr->flag = GET_ROLE(pDesc->flag);
	hdr->len = sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr);
	hdr->crc = csum((unsigned short *)(buffer + sizeof(struct ethhdr)), hdr->len );
	
	/*send the packet*/
	send_result = ksocket_send(pDesc->sock,&(pDesc->socket_address), buffer, length);
	if (send_result == -1) { 
	   DBGPRINT(DEBUG_ERROR,("%s: send ack error\n", STATE_STR(pDesc->dev_state)));
	}				
	return send_result;
}


int send_cmm_packet_tdmaslot(DEV_MAIN_DISCRIPTION *pDesc ,unsigned int seq, unsigned char *dest_mac, char *buffer, int length)
{
    int send_result = 0;
	int payload_len;
	unsigned int pmmc_count;
	struct ethhdr *eh= (struct ethhdr *)buffer;
	struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;

	
	unsigned char *dest_addr;
	unsigned char b_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	if (dest_mac)
		dest_addr = dest_mac;
	else
		dest_addr = b_mac;


	/*set the frame header*/
	memcpy((void*)buffer, (void*)dest_addr, ETH_MAC_LEN);
	memcpy((void*)(buffer+ETH_MAC_LEN), (void*)pDesc->src_mac, ETH_MAC_LEN);
	eh->h_proto = htons(LOCATION_COMM_PROTO);
		
	/*fill the frame with some data*/
	hdr->version = NCTU_COMM_VER;
	hdr->op_code = htons(NT_MSG_TDMA_ASSIGN);				
	hdr->seq_num = seq;                             // current tdma slot number
	hdr->flag = GET_ROLE(pDesc->flag);


    asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (pmmc_count));	

	hdr->tdma_count = htonl(pmmc_count);
	hdr->time_stamp = htonl(GET_TICK_COUNT(pDesc));
	hdr->info       = htonl( GET_TDMA_START_DELAY_TICK(pDesc));

    // backup old information
	pDesc->tdma_old.tick_pmmc_value = pmmc_count;
	pDesc->tdma_old.tick_count = GET_TICK_COUNT(pDesc);

    if (pDesc->flag == LEFT_EDGE_NODE ) {
	    payload_len = fill_tdma_slot_payload( pDesc, buffer + sizeof(struct location_commu_encaps_hdr ), length - sizeof(struct location_commu_encaps_hdr ) );
	    if(!payload_len){
		    DBGPRINT(DEBUG_ERROR,("No neighbor node for time slot assigned\n"));
		    return FALSE;
	    }
    }
	else{
		memcpy(buffer + sizeof(struct location_commu_encaps_hdr ), pDesc->buffer.data, pDesc->buffer.len );
		payload_len = pDesc->buffer.len;
    }
	hdr->len = sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr) + payload_len ;   
	hdr->crc = csum((unsigned short *)(buffer + sizeof(struct ethhdr)), hdr->len );

	//printk("send tdma slot, len=%d, payload_len=%d\n", hdr->len, payload_len);
	/*send the packet*/
	send_result = ksocket_send(pDesc->sock,&(pDesc->socket_address), buffer, length);
	if (send_result == -1) { 
	    DBGPRINT(DEBUG_ERROR,("%s: send tdma slot error\n", STATE_STR(pDesc->dev_state)));
	}
	
    asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (pmmc_count));		
	//printk("send tdma slot, len=%d, payload_len=%d, diff pmmc=%x, diff tick=%x\n", hdr->len, payload_len, pmmc_count - pDesc->tdma_old.tick_pmmc_value, GET_TICK_COUNT(pDesc) - pDesc->tdma_old.tick_count );

	
	return send_result;
}


int send_cmm_packet_tdma_start(DEV_MAIN_DISCRIPTION *pDesc ,unsigned int seq, unsigned char *dest_mac, char *buffer, int length)
{
    int send_result = 0;
	int payload_len;
	unsigned int pmmc_count;
	struct ethhdr *eh= (struct ethhdr *)buffer;
	struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;

#if 0
	if(GET_ROLE(pDesc->flag) != LEFT_EDGE_NODE ) {
		DBGPRINT(DEBUG_ERROR,("It's not the control edge node(%d) \n", GET_ROLE(pDesc->flag)));
		return FALSE;
	}
#endif
	
	unsigned char *dest_addr;
	unsigned char b_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	if (dest_mac)
		dest_addr = dest_mac;
	else
		dest_addr = b_mac;


	/*set the frame header*/
	memcpy((void*)buffer, (void*)dest_addr, ETH_MAC_LEN);
	memcpy((void*)(buffer+ETH_MAC_LEN), (void*)pDesc->src_mac, ETH_MAC_LEN);
	eh->h_proto = htons(LOCATION_COMM_PROTO);
		
	/*fill the frame with some data*/
	hdr->version = NCTU_COMM_VER;
	hdr->op_code = htons(NT_MSG_TDMA_CONFIRMM);				
	hdr->seq_num = seq;                             // current tdma slot number
	hdr->flag = GET_ROLE(pDesc->flag);


    asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (pmmc_count));	

      
	hdr->tdma_count = htonl(pmmc_count);	
	hdr->time_stamp = htonl(GET_TICK_COUNT(pDesc));	
	hdr->info       = htonl(GET_TDMA_START_DELAY_TICK(pDesc));	
	hdr->diff_pmmc  = htonl(pDesc->pro_delay.diff_tick_pmmc_value);	
	hdr->diff_tick  = htonl(pDesc->pro_delay.diff_tick_count);	

#if 0	
    payload_len = fill_tdma_slot_payload( pDesc, buffer + sizeof(struct location_commu_encaps_hdr ), length - sizeof(struct location_commu_encaps_hdr ) );
    if(!payload_len){
	    DBGPRINT(DEBUG_ERROR,("No neighbor node for time slot assigned\n"));
	    return FALSE;
    }
#else
    payload_len = 0;
#endif

	hdr->len = sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr) + payload_len ;   
	hdr->crc = csum((unsigned short *)(buffer + sizeof(struct ethhdr)), hdr->len );
	
	
	/*send the packet*/
	send_result = ksocket_send(pDesc->sock,&(pDesc->socket_address), buffer, length);
	if (send_result == -1) { 
	    DBGPRINT(DEBUG_ERROR,("%s: send tdma slot error\n", STATE_STR(pDesc->dev_state)));
	}
	//printk("S:Pmmc=%x,Tstamp=%x,dPmmc=%x,dTick=%x\r\n", pmmc_count, hdr->time_stamp, pDesc->pro_delay.diff_tick_pmmc_value, pDesc->pro_delay.diff_tick_count);
	DBGPRINT(DEBUG_TDMA,("dPmmc=%x,dTick=%x\r\n", pDesc->pro_delay.diff_tick_pmmc_value, pDesc->pro_delay.diff_tick_count));
	return send_result;
	
}

extern int time_slot_mode;
int send_cmm_data(DEV_MAIN_DISCRIPTION *pDesc ,unsigned int seq, unsigned char *dest_mac, unsigned short dest_id, char *buffer, int length, int mode)
{
    static int count_test =0;
    int send_result = 0;
	char *ptr;
	unsigned int pmmc_count;
	SENSOR_DATA  *pData, *dbg_wsn;
#if TDMA_DEBUUG_LOG ==1
    static unsigned int test_sequnece =0;
    static unsigned int last_sequnece =0xffffffff;
#endif	
	unsigned long flags;	

    int copy_len =0;
	struct ethhdr *eh= (struct ethhdr *)buffer;
	struct location_commu_encaps_hdr *hdr=(struct location_commu_encaps_hdr *)buffer;
	pData = &(pDesc->wsn_data);
    if (time_slot_mode==0){
        length = (pDesc->tdma_info.id +1)*300;
    }
#if TDMA_DEBUUG_LOG ==1
    ///--if(pDesc->tdma_status.total_tx_count > pDesc->test_count){
    ///--if(pDesc->cycle > pDesc->test_count + (RE_SYNC_CYCLY_COUNT<<1)){
#if NCTU_WAVE==1    
    if (pDesc->flag == LEFT_EDGE_NODE) 
#endif
	if(pDesc->tdma_status.total_tx_count > pDesc->test_count + (PRESYNC_TX_PKTS<<2) || test_sequnece > pDesc->test_count ){

		pDesc->dev_state  = NTBR_STATE_DEBUG;
		return 0;
	}

#endif
	
	unsigned char *dest_addr;
	unsigned char b_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	if (dest_mac)
		dest_addr = dest_mac;
	else
		dest_addr = b_mac;


#if MULTI_PKYS_PER_SLOT ==1
    pDesc->sequence++;
#endif
	/*set the frame header*/
	memcpy((void*)buffer, (void*)dest_addr, ETH_MAC_LEN);
	memcpy((void*)(buffer+ETH_MAC_LEN), (void*)pDesc->src_mac, ETH_MAC_LEN);
	eh->h_proto = htons(LOCATION_COMM_PROTO);
		
	/*fill the frame with some data*/
	hdr->version = NCTU_COMM_VER;
	hdr->op_code = htons(NT_MSG_DATA);				
	hdr->seq_num = pDesc->sequence;
	hdr->flag    = GET_ROLE(pDesc->flag);
#if MULTI_PKYS_PER_SLOT ==1
	hdr->frames  = pDesc->frames;	
#endif
	hdr->src_addr = pDesc->tdma_info.id;
    hdr->hop_addr = time_slot_mode;  /// we use the field to store sending mode for test ...
	hdr->dst_addr = dest_id;
	//printk(KERN_INFO "dest_id=%d ", dest_id );

	asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (pmmc_count));	
	
    if(mode == RE_SYNC_REQ ){
		
	    hdr->flag = SET_RSYNC_REQ(hdr->flag,1);	
		hdr->diff_pmmc = 0;
		hdr->diff_tick = 0;
		
		// backup old information
		pDesc->tdma_old.tick_pmmc_value = pmmc_count;
		pDesc->tdma_old.tick_count = GET_TICK_COUNT(pDesc);
		
    }
    else if( mode == RE_SYNC_SET){
		hdr->flag = SET_SYNC_BIT(hdr->flag,1);
		hdr->diff_pmmc	= htonl(pDesc->pro_delay.diff_tick_pmmc_value); 
		hdr->diff_tick	= htonl(pDesc->pro_delay.diff_tick_count);	
    }
    
	hdr->tdma_count = htonl(pmmc_count);	
	hdr->time_stamp = htonl(GET_TICK_COUNT(pDesc));
	hdr->info       = 0;
	

#if 1

#if TDMA_DEBUUG_LOG ==1
    //if(pDesc->get_sync_count >= TDMA_DEBUUG_LEAST_SYNC_NUM ) {
	//	pData->header.type = CURRENT_MIX_DATA;
	//	test_sequnece++;
    //}
	//else
	//	pData->header.type = SPECAIL_CMD_DATA;

#if NCTU_WAVE==1 
	if( !GET_ROLE(pDesc->flag) ) 
		pDesc->sequence++;
#endif

	if( GET_ROLE(pDesc->flag) ) {
		if(pDesc->tdma_status.total_tx_count > PRESYNC_TX_PKTS && test_sequnece <= pDesc->test_count) {
			pData->header.type = SPECAIL_CMD_DATA;

#if MULTI_PKYS_PER_SLOT==1
            test_sequnece++;
#else ////#if DUPLICATED_PKT==1			
			if(last_sequnece != pDesc->sequence) {
			    test_sequnece++;
				last_sequnece = pDesc->sequence;
			}
#endif			
		}else{
		    pData->header.type = CURRENT_MIX_DATA;
			test_sequnece = 0;
		}
	}else if (pDesc->start_test){
		pData->header.type = SPECAIL_CMD_DATA;
#if MULTI_PKYS_PER_SLOT==1
		test_sequnece++;
#else ////#if DUPLICATED_PKT==1			
		if(last_sequnece != pDesc->sequence) {
			test_sequnece++;
			last_sequnece = pDesc->sequence;
		}
#endif		
	}
#else		
    pData->header.type = CURRENT_MIX_DATA;
#endif

	pData->header.len  = sizeof(struct _wsn_data_format)*2;  // two wsn data
    pData->header.node_id = pDesc->tdma_info.id;  // my node id



#if TDMA_DEBUUG_LOG ==1
#if NCTU_WAVE==1    
	if( !GET_ROLE(pDesc->flag) ) 
        pData->header.sequence = seq;
	else
#endif
    pData->header.sequence = test_sequnece;
#endif	
	pData->header.timestamp = hdr->time_stamp;	 // we use current time to test, it should be sensor capture time



    // fill wsn dummy data
    pData->data[0].wsn_id = htonl(pDesc->tdma_info.id << 4 );
	memset( pData->data[0].data, 1 ,WSN_DATA_LENGTH);
    pData->data[1].wsn_id = htonl(pDesc->tdma_info.id << 4 + 1 );
	memset( pData->data[1].data, 2 ,WSN_DATA_LENGTH);

    if(count_test++ >= 250) {
		count_test=0;
		//printk("%d ", pDesc->buffer.len);

    }

	OS_SEM_LOCK(&pDesc->buffer_lock);	
	
# if MULTI_PKYS_PER_SLOT==1		

	if (pDesc->buffer.len) {
		
		copy_len = sizeof(struct _msg_data_header) + (sizeof(struct _wsn_data_format)<<1);
		if(pDesc->tdma_info.id)
			copy_len = pDesc->tdma_info.id*copy_len;
		if(copy_len> pDesc->buffer.len) {
			copy_len = pDesc->buffer.len;
		    //printk("copy length is bigger than buffer len ? c=%d,b=%d\n",copy_len,pDesc->buffer.len);
		}else if (pDesc->buffer.len > MULTI_PKYS_PER_SLOT_NUM * pDesc->tdma_info.id *( sizeof(struct _msg_data_header) + (sizeof(struct _wsn_data_format)<<1))) { // clean buffer to avoid big packet size
             copy_len = pDesc->buffer.len;
		}

#if 0		
		if( pDesc->tdma_info.id==1) {
			int len = pDesc->buffer.len;
			char *ptr = pDesc->buffer.data;
			for( ;len>0; len -=  sizeof(struct _msg_data_header) + (sizeof(struct _wsn_data_format)<<1), ptr += sizeof(struct _msg_data_header) + (sizeof(struct _wsn_data_format)<<1) ) {
				dbg_wsn = ptr;
				printk("out sseq=%d,buf=%d, len=%d\n", dbg_wsn->header.sequence, pDesc->buffer.len ,len );
			}
		}
#endif		
		
	    memcpy(buffer + sizeof(struct location_commu_encaps_hdr ), pDesc->buffer.data, copy_len );	
		ptr = buffer + sizeof(struct location_commu_encaps_hdr ) + copy_len;
#if 0		
		if( pDesc->tdma_info.id==1 && copy_len > 0) {
			dbg_wsn = buffer + sizeof(struct location_commu_encaps_hdr );
			printk(" sensor seq =%d, copy_len=%d, buf=%d\n", dbg_wsn->header.sequence, copy_len, pDesc->buffer.len  );
		}
#endif		
    }else if(pDesc->buffer.len<0) {
        printk("pDesc->buffer.len <0 =%d\n", pDesc->buffer.len);
	}else{
	    copy_len = 0;
  	    ptr=buffer + sizeof(struct location_commu_encaps_hdr ) ;
	}
    memcpy(ptr, pData, pData->header.len + sizeof(struct _msg_data_header));	
	hdr->len = sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr) + pData->header.len + sizeof(struct _msg_data_header) + copy_len;
	
	//if( pDesc->tdma_info.id==1) 
	//	hex_dump("Send", buffer + sizeof(struct location_commu_encaps_hdr ), hdr->len - (sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr)) );
	
	
    //  protocol header length + my data length + my data header length + carried data total size    
	
# else
	if(pDesc->buffer.len > length - (sizeof(struct location_commu_encaps_hdr ) + pData->header.len + sizeof(struct _msg_data_header)) ) { // size check
	    printk("data size error=%d\r\n", pDesc->buffer.len);
        return 0;
	}

	if (pDesc->buffer.len) {		
		memcpy(buffer + sizeof(struct location_commu_encaps_hdr ), pDesc->buffer.data, pDesc->buffer.len ); 
		ptr = buffer + sizeof(struct location_commu_encaps_hdr ) + pDesc->buffer.len;
	}else{
		ptr=buffer + sizeof(struct location_commu_encaps_hdr ) ;
	}
    memcpy(ptr, pData, pData->header.len + sizeof(struct _msg_data_header));	
	hdr->len = sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr) + pData->header.len + sizeof(struct _msg_data_header) + pDesc->buffer.len;
    //  protocol header length + my data length + my data header length + carried data total size    
	
# endif

    
# if MULTI_PKYS_PER_SLOT==1    

#if 0
    printk("=1===\n");
	if( pDesc->tdma_info.id==1) {
		int len = pDesc->buffer.len;
		char *ptr = pDesc->buffer.data;
		for( ;len>0; len -=  sizeof(struct _msg_data_header) + (sizeof(struct _wsn_data_format)<<1), ptr += sizeof(struct _msg_data_header) + (sizeof(struct _wsn_data_format)<<1) ) {
			dbg_wsn = ptr;
			printk("Sseq=%d,buf=%d, len=%d,ptr=0x%x\n", dbg_wsn->header.sequence, pDesc->buffer.len, len, ptr	);
		}
	}
	
	if( pDesc->tdma_info.id==1) 
		  hex_dump("Buffer1", pDesc->buffer.data, pDesc->buffer.len );
#endif
    if(copy_len>0 && pDesc->buffer.len - copy_len > 0 )
        memcpy(pDesc->buffer.data, pDesc->buffer.data + copy_len, pDesc->buffer.len - copy_len );
    pDesc->buffer.len -= copy_len;
#if 0
    if( pDesc->tdma_info.id==1) 
	  hex_dump("Buffer2", pDesc->buffer.data, pDesc->buffer.len );
#endif
# else
    pDesc->buffer.len = 0; // clean the forwarding buffer
# endif


#else	
	hdr->len = sizeof(struct location_commu_encaps_hdr) - sizeof(struct ethhdr);

#endif

	OS_SEM_UNLOCK(&pDesc->buffer_lock);

	hdr->crc = csum((unsigned short *)(buffer + sizeof(struct ethhdr)), hdr->len );
#if TDMA_DEBUUG_LOG ==1
	///--if(pDesc->cycle > RE_SYNC_CYCLY_COUNT<<1 )
	///--if(pDesc->get_sync_count >= TDMA_DEBUUG_LEAST_SYNC_NUM ) 
#endif		
	    pDesc->tdma_status.total_tx_count++;	

	/*send the packet*/
	/*MAC - begin*/

	 pDesc->socket_address.sll_addr[0]	=  ((struct dllist *)(pDesc->forward_node))->mac[0]; 	 
	 pDesc->socket_address.sll_addr[1]	=  ((struct dllist *)(pDesc->forward_node))->mac[1]; 	 
	 pDesc->socket_address.sll_addr[2]	=  ((struct dllist *)(pDesc->forward_node))->mac[2]; 
	 pDesc->socket_address.sll_addr[3]	=  ((struct dllist *)(pDesc->forward_node))->mac[3]; 
	 pDesc->socket_address.sll_addr[4]	=  ((struct dllist *)(pDesc->forward_node))->mac[4]; 
	 pDesc->socket_address.sll_addr[5]	=  ((struct dllist *)(pDesc->forward_node))->mac[5]; 

	 /*MAC - end*/
	 pDesc->socket_address.sll_addr[6]  = 0x00;/*not used*/
	 pDesc->socket_address.sll_addr[7]  = 0x00;/*not used*/

	printk("dec_mac=%02X,%02X,%02X,%02X,%02X,%02X  len=%d\n",pDesc->socket_address.sll_addr[0],pDesc->socket_address.sll_addr[1],pDesc->socket_address.sll_addr[2],pDesc->socket_address.sll_addr[3],pDesc->socket_address.sll_addr[4],pDesc->socket_address.sll_addr[5],length);
	hex_dump("Send_buf",buffer,length);
	send_result = ksocket_send(pDesc->sock,&(pDesc->socket_address), buffer, length);

#if 0 // duplicate packet	
    if(mode == RE_SYNC_REQ ){		
	    hdr->flag = 0;			
    }
    else if( mode == RE_SYNC_SET){
		hdr->flag = 0;
    }
	hdr->crc = csum((unsigned short *)(buffer + sizeof(struct ethhdr)), hdr->len );
	send_result = ksocket_send(pDesc->sock,&(pDesc->socket_address), buffer, length);
#endif		
	if (send_result == -1) { 
	   DBGPRINT(DEBUG_ERROR,("%s: send data error\n", STATE_STR(pDesc->dev_state)));
	}				
	return send_result;
}

