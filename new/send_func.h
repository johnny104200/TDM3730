#ifndef __SEND_FUNC_H__
#define __SEND_FUNC_H__

int send_cmm_packet(DEV_MAIN_DISCRIPTION *pDesc , unsigned char *dest_mac, char *buffer, int length);
int send_cmm_packet_ack(DEV_MAIN_DISCRIPTION *pDesc ,unsigned int seq, unsigned char *dest_addr, char *buffer, int length);
int send_cmm_broad_packet(DEV_MAIN_DISCRIPTION *pDesc , unsigned char *dest_mac, char *buffer, int length);
int send_cmm_packet_tdmaslot(DEV_MAIN_DISCRIPTION *pDesc ,unsigned int seq, unsigned char *dest_addr, char *buffer, int length);
int send_cmm_packet_tdma_start(DEV_MAIN_DISCRIPTION *pDesc ,unsigned int seq, unsigned char *dest_addr, char *buffer, int length);
int send_cmm_data(DEV_MAIN_DISCRIPTION *pDesc ,unsigned int seq, unsigned char *dest_addr, unsigned short dest_id, char *buffer, int length, int resync);
#if NCTU_SIGNAL_SEARCH == 1
int send_cmm_assign_broadcast_node_packet(DEV_MAIN_DISCRIPTION *pDesc , unsigned char *dest_mac, char *buffer, int length);
int send_cmm_trigger_packet(DEV_MAIN_DISCRIPTION *pDesc , unsigned char *dest_mac, char *buffer, int length);
int send_cmm_packet_hello(DEV_MAIN_DISCRIPTION *pDesc , unsigned char *dest_mac, char *buffer, int length);
int send_cmm_ack_packet(DEV_MAIN_DISCRIPTION *pDesc , unsigned char *dest_mac, char *buffer, int length, int type,unsigned int seq);
int send_cmm_neighbor_info_packet(DEV_MAIN_DISCRIPTION *pDesc , unsigned char *dest_mac, char *buffer, int length, char *append, int app_length);
#endif

#endif
