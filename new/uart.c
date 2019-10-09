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

#include <linux/fs.h>

#include <linux/poll.h>
#include "nw_comm.h"


#define MODEMDEVICE "/dev/ttyS2"


#if UART_SUPPORT==1
struct file *f;

int send_char(char word)
{
	mm_segment_t oldfs;
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	f->f_op->write(f, &word,1,&f->f_pos);
	set_fs(oldfs);
	//in_hex_dump("Receive Packet",kuart->send_buffer ,13);
	return 1;
}


int recv_char(char *word)
{
    int result;	
	mm_segment_t oldfs;
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	result=f->f_op->read(f, word,100,&f->f_pos);
	set_fs(oldfs);
	if( result <=0 )
		return -1;
	else 
		return 1;
}

#define BUFF_SIZE 1500

 int tty_read( int timeout, unsigned char *string) 
{ 
	int result; 
	unsigned char buff[BUFF_SIZE+1]; 
	int length; 

	result = -1; 
	if (!IS_ERR(f)) { 
		mm_segment_t oldfs; 

		oldfs = get_fs(); 
		set_fs(KERNEL_DS); 
		if (f->f_op->poll) { 
			struct poll_wqueues table; 
			struct timeval start, now; 

			do_gettimeofday(&start); 
			poll_initwait(&table); 
			while (1) { 
				long elapsed; 
				int mask; 

				mask = f->f_op->poll(f, &table.pt); 
				if (mask & (POLLRDNORM | POLLRDBAND | POLLIN | POLLHUP | POLLERR)) { 
					break; 
				} 
				do_gettimeofday(&now); 
				elapsed = 
					(1000000 * (now.tv_sec - start.tv_sec) + 
					now.tv_usec - start.tv_usec); 
				if (elapsed > timeout) { 
					//break; 
					poll_freewait(&table); 
					set_fs(oldfs); 
					return result;
				} 
				set_current_state(TASK_INTERRUPTIBLE); 
				schedule_timeout(((timeout - elapsed) * HZ) / 10000); 
			} 
			poll_freewait(&table); 
		} 

		f->f_pos = 0; 
		if ((length = f->f_op->read(f, buff, BUFF_SIZE, &f->f_pos)) > 0) { 
			buff[length-1] = '\0'; 
			strcpy(string, buff); 
			result = 1; 
		} else { 
			//printk("debug: failed\n"); 
		} 
		set_fs(oldfs); 
	} 
	return result; 
} 


void linux_uart_init(void)
{
	/*open UART*/	
	//f=filp_open(MODEMDEVICE,O_RDWR | O_NDELAY|O_NOCTTY | O_NONBLOCK ,0);
	f=filp_open(MODEMDEVICE,O_RDWR | O_NDELAY|O_NOCTTY,0);
}

void linux_uart_close(void)
{
	/*close UART*/	
	filp_close(f,NULL);
}


/* SLIP special character codes
*/


#define END 0300                  /* indicates end of packet *///192
#define ESC 0333                  /* indicates byte stuffing */  // 219
#define ESC_END 0334              /* ESC ESC_END means END data byte */
#define ESC_ESC 0335              /* ESC ESC_ESC means ESC data byte */


/* SEND_PACKET: sends a packet of length "len", starting at
 * location "p".
 */

void uart_send_packet(unsigned char *p, int len)
{

  /* send an initial END character to flush out any data that may
  * have accumulated in the receiver due to line noise
  */

  send_char(END);

  /* for each byte in the packet, send the appropriate character
   * sequence
   */

  while(len--) {

     switch(*p) {

	       /* if it’s the same code as an END character, we send a
	        * special two character code so as not to make the
	        * receiver think we sent an END
	       */

	      case END:
	           send_char(ESC);
	           send_char(ESC_END);
	           break;

	       /* if it’s the same code as an ESC character,
	        * we send a special two character code so as not
	        * to make the receiver think we sent an ESC
	        */

	     case ESC:
	          send_char(ESC); 
	          send_char(ESC_ESC);
	          break;

	     /* otherwise, we just send the character
	     */

	    default:
	        send_char(*p);
	  }
     p++;
   }


   /* tell the receiver that we are done sending the packet
      */
    send_char(END);
}

int get_buff_char(unsigned char *c);


/* RECV_PACKET: receives a packet into the buffer located at "p".
* If more than len bytes are received, the packet will
* be truncated.
* p_ret_len would be the number of bytes stored in the buffer.
* -1 means no data or error, 0 means buffer has data but not end yet, 1 means received a packet.
*/
int uart_recv_packet(unsigned char *p, int buffer_len, int *p_ret_len ) {
	unsigned char c;
	int received = 0;
	int ret;
	/* sit in a loop reading bytes until we put together
	* a whole packet.
	* Make sure not to copy them into the packet if we
	* run out of room.
	*/
	while(1) {
		/* get a character to process
		*/
		ret = get_buff_char(&c);
		if(ret==-1){
			*p_ret_len = received;
			if(received==0)
				return -1;
			else
				return 0;
		}
		/* handle bytestuffing if necessary
		*/

		switch(c) {
		/* if it¡¦s an END character then we¡¦re done with
		* the packet
		*/
		case END:
		/* a minor optimization: if there is no
		* data in the packet, ignore it. This is
		* meant to avoid bothering IP with all
		* the empty packets generated by the
		* duplicate END characters which are in
		* turn sent to try to detect line noise.
		*/
		if(received){
			*p_ret_len = received;			
			return 1;
		}
		else
			break;
		/* if it¡¦s the same code as an ESC character, wait
		* and get another character and then figure out
		* what to store in the packet based on that.
		*/
		case ESC:
			ret = get_buff_char(&c);
			if(ret==-1){
				*p_ret_len = received;
				if(received==0)
					return -1;
				else
					return 0;
			}
			/* if "c" is not one of these two, then we
			* have a protocol violation. The best bet
			* seems to be to leave the byte alone and
			* just stuff it into the packet
			*/
			switch(c) {
				case ESC_END:
				c = END;
				break;
				case ESC_ESC:
				c = ESC;
				break;
			}
			/* here we fall into the default handler and let
			* it store the character for us
			*/
		   default:
			if(received < buffer_len)
				p[received++] = c;
		}
	}
}



#endif

