/*	
 *	CSE 5361
 *	Project 3 
 *
 *  By - Rohit Khanna
 */ 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/inet.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <net/route.h>
#include <net/protocol.h>
#include <linux/delay.h>
#include <linux/semaphore.h>	

#define IPPROTO_CSE536 234
#define CSE536_MAJOR 234

#define USER_DATA_LEN 236
#define HEADER_LEN 100
#define WAIT_ACK_TIMEOUT 5000

static void getLocalIPAddress(void);
int cse536_rcv(struct sk_buff*);
void cse536_err(void);


typedef struct Message{			  /*	Message structure 	*/
  
  	uint32_t record_id;	  	  // Record ID, ack=0 or event=1 		uint32_t = typedef unsigned long in
	uint32_t final_clock;	 	  // final counter  
	uint32_t orig_clock;	  	  // orig counter 
	__be32 source_ip;		  // Source IP  
	__be32 dest_ip;			  // Dest IP 
	uint8_t msg_data[USER_DATA_LEN];  // data	unit8_t =  unsigned char
}Message;


typedef struct rcvQueue			 /*	receive queue structure	*/
{
	struct rcvQueue *next;
	Message *message;
}rcvQueue;

rcvQueue *front, *rear;

uint32_t Local_Clock;			// Local Lamport Clock			

char Read_Buffer[USER_DATA_LEN];
char Write_Buffer[USER_DATA_LEN];	

__be32  Saddr;				// Source and Destination IP Addresses
__be32 Daddr;

struct semaphore mutex_rcv, mutex;

static const struct net_protocol cse536_protocol = {	/* cse536_protocol strructure, registered with IP layer.  */
	.handler = cse536_rcv,
	.err_handler = cse536_err,
	.no_policy = 1
};

static int debug_enable = 0;				// Command line arg for kernel module
module_param(debug_enable, int, 0);
MODULE_PARM_DESC(debug_enable, "Enable module debug mode.");

struct file_operations cse536_fops;



static int cse536_open(struct inode *inode, struct file *file)
{
	printk("cse536: cse536_open(): successful\n");
	return 0;
}


uint32_t update_local_clock(uint32_t rcvd_orig_clock){

	uint32_t final_clock=0;
	printk("cse536: update_local_clock()\n");
	
	Local_Clock++;
	printk("Local_Clock incremented to %d while receiving event\n", Local_Clock);
	
	if(Local_Clock > rcvd_orig_clock){				// local=10 and rcvd=5 --> local=10 and final=10
		final_clock=Local_Clock;
	}
		
	if(Local_Clock < rcvd_orig_clock){				// local=5 and rcvd=10 -->  local=11 and final=11
		Local_Clock=rcvd_orig_clock+1;
		final_clock=Local_Clock;
	}
		
	if(Local_Clock == rcvd_orig_clock){				// local=5 and rcvd=5  -->  local=6 and final=6
		Local_Clock++;
		final_clock=Local_Clock;
	}
		
	printk("cse536: Local_Clock is now = %d\n", Local_Clock);	
	return final_clock;
}





int send(uint32_t msg_type, __be32 dest_ip, const char *data, uint32_t final_clock )
{
	Message *msg;
	struct sk_buff *skb;
	struct iphdr *iph;
	struct rtable *rt;
	struct net *net;
	char source_ip[16];
	char ack_msg[200];
	
	
	printk("cse536: 1. send()\n");
	skb = alloc_skb(1500, GFP_KERNEL);					// Allocate skb, skb has 1500 bytes of tail room only
	if (!skb)
		return -1;

	printk("cse536: 2. skb allocated\n");

	skb_reserve(skb, HEADER_LEN);						//reserve space for headers
	printk("cse536: 3. reserved space for headers\n");
	
	msg=(Message*)kmalloc(sizeof(Message), GFP_KERNEL);
	msg = (Message*)skb_put(skb, sizeof(Message));				//make space for user data
	
	if(msg_type==0){		//ack=0
		printk("cse536: 4. SENDIND ACK\n");
		msg->record_id=0;
		msg->final_clock=final_clock;
		
		sprintf(ack_msg, "%s", "ACKNOWLEDGEMENT");
		memset(msg->msg_data, '\0', sizeof(msg->msg_data));
		memcpy(msg->msg_data, ack_msg, strlen(ack_msg));
		
	}
	if(msg_type==1){		//data=1
		printk("cse536: 4. SENDING DATA\n");
		Local_Clock++;
		printk("cse536: 5. Local_Clock incremented to %d while sending event\n", Local_Clock);
		 msg->record_id=1;
		 if(copy_from_user( msg->msg_data , data, USER_DATA_LEN) ) {		//	Copy data to buffer
			printk("cse536: 5. ERROR: User Fault while copying data to msg");
			return -1;
		 }
		 
	}
	
	printk("cse536: 6. copied data to buffer, msg->msg_data = %s, strlen=%d\n", msg->msg_data, strlen(msg->msg_data));
	
	printk("cse536: 7. skb->data = \t\t<<<<  %s   >>>>\n" ,((Message*)(skb->data))->msg_data);
	
	(msg)->orig_clock=Local_Clock;
	(msg)->final_clock=Local_Clock;
	(msg)->source_ip=Saddr;
	(msg)->dest_ip=dest_ip;
	
	printk("cse536: dest_ip=%pI4\n", &dest_ip);
	printk("cse536: 8. sending msg=|%s| with Local_Clock=%d, record_id=%d, dest_ip=%pI4\n", 
				msg->msg_data, Local_Clock,msg->record_id, &(msg->dest_ip));
	
	skb_push(skb, sizeof(struct iphdr));				//	setup ip header
	printk("cse536: 9. push for ip header\n");
	skb_reset_network_header(skb);
	printk("cse536: 10. reset netwrok header \n");
	iph = ip_hdr(skb);
	printk("cse536: 11. iph=ip_hdr(skb)\n");

	iph->version = 4;
	iph->ihl = 5;
	iph->tos = 0;
	iph->tot_len = htons(skb->len);
	iph->frag_off = 0;
	iph->ttl      = 64;
	iph->protocol = IPPROTO_CSE536;
	iph->check    = 0;
	iph->check    = ip_fast_csum((unsigned char *)iph, iph->ihl);

	net = &init_net;
	printk("cse536: 12. *net = &init_net\n");

	rt = ip_route_output(net, dest_ip, Saddr, 0,0); 		//	 establish route
	printk("cse536: 13. called ip_route_output, \n");

	skb_dst_set(skb, &rt->dst);						// 	set destination
	printk("cse536: 14. skb_dst_set()\n");

	iph->saddr = rt->rt_src;
	iph->daddr = rt->rt_dst;

	printk("cse536: 15. calling ip_local_out\n");
	ip_local_out(skb);								// 	send data
	
		
	printk("cse536: 16. send() exiting\n");
	
	return skb->len;
	
}




int cse536_rcv(struct sk_buff *skb){		//No need of critical section while adding to rcvQueue as cse536_rcv is int handler 

	Message *msg_rcvd;
	uint32_t final_clock;
	rcvQueue *item;
	char ack_msg[50];
	char source_ip[20];
	
	printk("cse536: cse536_rcv() handler\n");
	
	msg_rcvd  = (Message*)kmalloc(sizeof(Message), GFP_KERNEL);
	msg_rcvd = (Message*)(skb->data);
	
	printk("cse536: MSG = |%s| received from %pI4 with record_id=%d and orig_clock=%d and final_clock=%d\n",msg_rcvd->msg_data, 
		&msg_rcvd->source_ip, msg_rcvd->record_id, msg_rcvd->orig_clock, msg_rcvd->final_clock );
	printk("cse536: strlen of msg=%d\n", strlen(msg_rcvd->msg_data));
	
	
	switch(msg_rcvd->record_id){
	
		case 0:	// ACK RECEIVED
			//if( (msg_rcvd->source_ip == Daddr) && (msg_rcvd->orig_clock==Local_Clock) )
			if (msg_rcvd->source_ip == Daddr){
				printk("cse536: ACK RECEIVED \n");
				printk("cse536: Daddr=%pI4\n", &Daddr);
			
				if( msg_rcvd->source_ip == Daddr){
					printk("cse536: increment mutex_rcv\n ");
					up(&mutex_rcv);			//signal waiting send
				}
				
				snprintf(source_ip, 16, "%pI4", &(msg_rcvd->source_ip));
				printk("cse536: source_ip = %s\n", source_ip);
				sprintf(ack_msg, "%s", "ACK RECEIVED ");
				//strncat(ack_msg, source_ip, 16);
				printk("ack_msg = %s\n", ack_msg);
				memset(msg_rcvd->msg_data, '\0', sizeof(msg_rcvd->msg_data));
				memcpy(msg_rcvd->msg_data, ack_msg, strlen(ack_msg));
			}

			break;
		
		case 1: // DATA RECEIVED
		
			/*	UPDATE CLOCK	*/
			final_clock = update_local_clock(msg_rcvd->orig_clock);
			
			
			/*		SEND ACK 			*/
			printk("cse536: SENDING ACK\n");
			send(0, msg_rcvd->source_ip, msg_rcvd->msg_data, final_clock );	// send ack now
			printk("cse536: ACK sent\n");
			
			break;

		default:
			printk("cse536: INVALID record_id=%d RECEIVED\n", msg_rcvd->record_id);
			break;
		
	}
	
	/*		Add to rcvQueue		*/
	item = kmalloc(sizeof(rcvQueue), GFP_KERNEL);
	if(item==NULL){
		printk("cse536: No Memory available\n");
		return -1;
	}
	
	item->message = msg_rcvd;			// copy rcvd message from skb->data
	item->next=NULL;
							// add item from rear	
	if(rear == NULL){				// rcvQueue is empty
		rear=item;
		front=rear;
		
	}
	else{	
		rear->next=item;
		rear=item;		
	}
	
	printk("cse536: Received message = |%s| added to rcvQueue\n", (rear->message)->msg_data);

	printk("cse536: cse536_rcv() exiting\n");
	return skb->len;
	
}



/*	calculates the local IP address and stores it in Saddr	*/
static void getLocalIPAddress(){

	struct net_device *eth0; 
	struct in_device *ineth0;

	eth0 = dev_get_by_name(&init_net, "eth2");				//	get network device by name
	printk("cse536: Init : addr of net_device eth0 %p\n", eth0);
			 
	ineth0 = in_dev_get(eth0);						//	convert network device to in_device
	printk("cse536: Init :  addr of in_device eth0 %p\n", ineth0);
	 
	for_primary_ifa(ineth0){
		Saddr = (ifa->ifa_address);
	} endfor_ifa(ineth0);
}




static int __init cse536_init(void)
{
	int ret;
	
	
	printk("\ncse536: --------------------------------------------------------------------\n");
	printk("cse536 module Init - debug mode is %s\n", debug_enable ? "enabled" : "disabled");
	ret = register_chrdev(CSE536_MAJOR, "cse5361", &cse536_fops);
	
	if (ret < 0) {
		printk("Error registering cse536 device\n");
		goto cse536_fail1;
	}

	printk("cse536: registered module successfully!\n");
	/* Init processing here... */

	inet_add_protocol(&cse536_protocol, IPPROTO_CSE536);
	printk("cse536: Init : cse536_protocol with no = %d added\n", IPPROTO_CSE536);

	getLocalIPAddress();
	Local_Clock=0;		//initialize logical counter with 0
	
	sema_init(&mutex_rcv,0);
	sema_init(&mutex,1);
	
	rear=NULL;		//init queue ptrs to NULL
	front=NULL;

	return 0;
	
	cse536_fail1:
	return ret;
}




static ssize_t cse536_read(struct file *file, char *buf, size_t count, loff_t *ptr)
{
	ssize_t retCount;
	rcvQueue *temp;
	
	retCount=0;
	
	printk("cse536: cse536_read() , \n");
	if((front == rear) && (rear == NULL)){			// rcvQueue is empty		
		
		printk("cse536: rcvQueue is empty, NO DATA TO READ !!! \n");
		retCount = sprintf(buf, "NO DATA TO READ !!");
	}
	else{							// Read Data				
		printk("cse536: data to be read = |%s| \n", (front->message)->msg_data );	
		retCount = sprintf(buf,"%s, ORIG CLOCK=|%d|, LOCAL CLOCK=|%d|, SOURCE IP=|%pI4|, DEST IP=|%pI4|", (front->message)->msg_data, (front->message)->orig_clock, Local_Clock, &(front->message)->source_ip, &(front->message)->dest_ip);
	
		temp=front;					// Dequeue from front 	
		front=front->next;
		if(rear==temp){					//only one node and rear should also be made NULL
			rear=rear->next;			//rear->next would be NULL
		}
		kfree(temp);
	}
	
	printk("cse536: cse536_read returning |%s| = |%d| bytes\n",buf, retCount);

	return retCount;
}



//	finds the index of delimiter '_'
static int find_underscore(const char *str){
	int i;

	for(i=0;i<strlen(str);i++){
		if(str[i] == '_'){
			printk("cse536: underscore found at i = %d\n", i);
			return i;
		}
	}
	
	return 0;

}




static ssize_t cse536_write(struct file *file, const char *wr_buf, size_t count, loff_t * ppos)
{
	
	down_interruptible(&mutex);		/*    CRITICAL SECTION BEGINS	*/
	int ip_len, under_score_at;
	char Dest_IP[16];
	printk("cse536: cse536_write()\n");
	printk("cse536: WRITE:  wr_buf = |%s| with strlen = |%d|\n", wr_buf, strlen(wr_buf) );

	memset(Dest_IP, '\0', sizeof(Dest_IP));
	
	under_score_at = find_underscore(wr_buf);
	memcpy(Dest_IP, wr_buf, under_score_at);
	
	printk("cse536: Dest_IP=%s, strlen=%d\n", Dest_IP, strlen(Dest_IP));
	
	ip_len = strlen(Dest_IP);
	printk("cse536: WRITE:  wr_buf = |%s| and wr_buf+ip_len+1 = |%s| \n", wr_buf, wr_buf+ip_len+1);

	Daddr=0;
	Daddr = in_aton(Dest_IP);
	printk("cse536: Daddr=%pI4\n", &Daddr);						//	in_aton returns in __be32
	
	/*		Send Data		*/	
	printk("cse536: WRITE:  calling send()----------------\n");
	send(1, Daddr, wr_buf+ip_len+1, 0 );	// send data
	
	printk("cse536: WRITE: mutex_rcv=->count=%d\n", mutex_rcv.count);
	if(down_timeout(&mutex_rcv, msecs_to_jiffies(5000)) != 0){	//mutex_rcv=0
		printk("cse536: WRITE: calling send() again----------------\n");
		send(1, Daddr, wr_buf+ip_len+1, 0 );
	}

	up(&mutex);
	printk("cse536: WRITE:  exiting\n");
	return count;
	
}



static long cse536_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	printk("cse536_ioctl: cmd=%d, arg=%ld\n", cmd, arg);
	return 0;
}



static int cse536_release(struct inode *inode, struct file *file)
{
	printk("cse536_release: successful\n");
	return 0;
}




static void __exit cse536_exit(void)
{
	int ret;
	unregister_chrdev(CSE536_MAJOR, "cse5361"); 

	ret = inet_del_protocol(&cse536_protocol, IPPROTO_CSE536);
	if(ret < 0 ){
		printk("Could not unregister protocol!\n");
	}

	printk("cse536 module Exit\n");
}




void cse536_err(){
	printk("cse536: cse536_err handler\n");
	
}

struct file_operations cse536_fops = {
	owner: THIS_MODULE,
	read: cse536_read,
	write: cse536_write,
	unlocked_ioctl: cse536_ioctl,
	open: cse536_open,
	release: cse536_release,
};

module_init(cse536_init);
module_exit(cse536_exit);

MODULE_AUTHOR("Rohit Khanna");
MODULE_DESCRIPTION("CSE536 Module for Project 1C");
MODULE_LICENSE("GPL");

