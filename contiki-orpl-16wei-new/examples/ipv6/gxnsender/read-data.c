/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */
#include "contiki.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ip/uip-udp-packet.h"
#include "contiki-net.h"
#include "net/ipv6/multicast/uip-mcast6.h"
#include "net/rpl/rpl.h"
#include "dev/serial-line.h"
#include "simple-udp.h"
#include "dev/uart0.h"
//#include "dev/uart1.h"
#include "collect-common.h"
#include "collect-view.h"
#include "dev/leds.h"
#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"
#include <stdio.h>
#include <string.h>
#include "lib/ringbuf.h"
#include "simple-udp.h"
#include "util.h"
#include "node-id.h"
#include "sys/ctimer.h"
#define UIP_MCAST6_CONF_ENGINE UIP_MCAST6_ENGINE_SCF
#define MAX_BUFFER_LENGTH 127
//static char data_buffer[MAX_BUFFER_LENGTH];
//port area
#define UDP_CLIENT_PORT 8775//
#define UDP_SERVER_PORT 5688//
#define UDP_SERVER_DATA_PORT 8765
#define MCAST_SINK_UDP_PORT 3001 
#define UDP_SERVER_UNICAST_PORT 5656
//所有的指令
#define CTL_READ_DATA       '1' //读表指令
#define CTL_ACK_READ_DATA   '2' //包含ACK的读表指令
#define UNICAST_ACK_DATA    '3' //danbo
#define UNICAST_CONFIG_PERIOD   '4' //unicast config period
#define MCAST_CONFIG_PERIOD   '5' //mcast config period
#define HERAT_BODY        'h'
#define state_ready 0
#define state_ack 1
#define DEBUG DEBUG_PRINT
#define BASE_PERIOD 10* CLOCK_SECOND //10
#define PERIOD 5
#define SEND_INTERVAL   (PERIOD * CLOCK_SECOND)
#define SEND_TIME   (random_rand() % (SEND_INTERVAL))
static struct etimer con_ack_period;
static struct etimer report_period;
//#define WAIT_TIME      (random_rand() % (CLOCK_SECOND * 10)) 
//#define BASE_TIME   10 * CLOCK_SECOND
#include "net/ip/uip-debug.h"
#define MAX_PAYLOAD_LEN   255
#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

static struct uip_udp_conn *client_conn;
static struct uip_udp_conn *client_conn_data;
static struct uip_udp_conn *client_conn_unicast_data;

static uip_ipaddr_t server_ipaddr;
static struct uip_udp_conn *sink_conn;
static struct etimer ack_period;

static char global_type;
static char ack_buffer[MAX_PAYLOAD_LEN];
static char con_buffer[MAX_PAYLOAD_LEN];
static unsigned char appdata_length;
static int data_length;
static bool uploadFlag = false;
static int n_id;
static int global_period;
static int cur_State = state_ready; 

//static char recv_buffer[MAX_PAYLOAD_LEN];
//static int node_id;

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client process");
AUTOSTART_PROCESSES(&udp_client_process, &collect_common_process);
/*---------------------------------------------------------------------------*/
void
collect_common_set_sink(void)
{
  /* A udp client can never become sink */
}
/*---------------------------------------------------------------------------*/

void
collect_common_net_print(void)
{
  rpl_dag_t *dag;
  uip_ds6_route_t *r;

  /* Let's suppose we have only one instance */
  dag = rpl_get_any_dag();
  if(dag->preferred_parent != NULL) {
   // PRINTF("Preferred parent: ");
    PRINT6ADDR(rpl_get_parent_ipaddr(dag->preferred_parent));
   // PRINTF("\n");
  }
  for(r = uip_ds6_route_head();
      r != NULL;
      r = uip_ds6_route_next(r)) {
    PRINT6ADDR(&r->ipaddr);
  }
 // PRINTF("---\n");
}

// void print_uip_addr_t(uip_ipaddr_t addr){
//     int i;
//     for(i=0;i<16;i++){
//         printf("%02X", addr.u8[i]);
//         if(i == 15){
//           printf("\n");
//           return;
//         }
//         if( i % 2 == 1){
//             printf(":");
//         }
//     }
// }

static void sendCmd(char* cmd,int cmd_length){
  int i;
  for(i=0;i< cmd_length;i++){
    putchar((unsigned char )cmd[i]);
  }
}
static void
send_packet(void *ptr)
{
  
  uip_udp_packet_sendto(client_conn_data, "h", 1,
                      &server_ipaddr, UIP_HTONS(UDP_SERVER_DATA_PORT));
  //ctimer_set(&heart_back, 10*CLOCK_SECOND, send_packet, NULL);
}

/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
  if(uip_newdata()) {

    appdata_length = (unsigned char)((char *)uip_appdata)[0];
    char *appdata = (char *)malloc((appdata_length +1) *sizeof(char));
    memcpy(appdata,(char *)uip_appdata + 1,appdata_length);
    appdata[appdata_length] = '\0';
    int seconds;
    char type = appdata[0];
    unsigned char ctl_content_length = 0;
    memset(ack_buffer,0,MAX_PAYLOAD_LEN);
    global_type = type;
    switch(type){
      case CTL_READ_DATA://duobo chun zhiling
        sendCmd((char *)(appdata + 1),appdata_length -1);
      break;
      case CTL_ACK_READ_DATA:
          ctl_content_length = (unsigned char) appdata[1];
          if(reupload_by_acks((appdata + 2 + ctl_content_length),appdata_length - 2 - ctl_content_length ,n_id)){
            sendCmd((char *)(appdata + 2),ctl_content_length);
            //read_data((appdata + 2),ctl_content_length);
          }
      break;
      case UNICAST_ACK_DATA://
        sendCmd((char *)(appdata+1),appdata_length-1);
      break;
      case MCAST_CONFIG_PERIOD:
         global_period = atoi(appdata+1) *CLOCK_SECOND +SEND_TIME;
         con_buffer[0] = MCAST_CONFIG_PERIOD;
         //memcpy(con_buffer + 1,appdata+1,appdata_length-1);
         seconds =  BASE_PERIOD + SEND_TIME;
         //seconds=global_period;
         etimer_set(&report_period,seconds);
         cur_State = state_ack;
         uploadFlag = true;
     break;
     case UNICAST_CONFIG_PERIOD://config information
         //ack_buffer[0] = UNICAST_CONFIG_PERIOD;
          //memcpy(ack_buffer,appdata,appdata_length);
        global_period = atoi(appdata + 1) * CLOCK_SECOND +SEND_TIME;
        
        //con_buffer[0] = UNICAST_CONFIG_PERIOD;
        //memcpy(con_buffer + 1,appdata+1,appdata_length-1);

        uip_udp_packet_sendto(client_conn_data, "4",1,
                      &server_ipaddr, UIP_HTONS(UDP_SERVER_DATA_PORT));
        //uploadFlag = true;

      break;

      }
 
    free(appdata);
  }
}

/*---------------------------------------------------------------------------*/
void
collect_common_send(void)
{
  static uint8_t seqno;
  struct {
    uint8_t seqno;
    uint8_t for_alignment;
    struct collect_view_data_msg msg;
  } msg;
  /* struct collect_neighbor *n; */
  uint16_t parent_etx;
  uint16_t rtmetric;
  uint16_t num_neighbors;
  uint16_t beacon_interval;
  rpl_parent_t *preferred_parent;
  linkaddr_t parent;
  rpl_dag_t *dag;

  if(client_conn == NULL) {
    /* Not setup yet */
    return;
  }
  memset(&msg, 0, sizeof(msg));
  seqno++;
  if(seqno == 0) {
    /* Wrap to 128 to identify restarts */
    seqno = 128;
  }
  msg.seqno = seqno;

  linkaddr_copy(&parent, &linkaddr_null);
  parent_etx = 0;

  /* Let's suppose we have only one instance */
  dag = rpl_get_any_dag();
  if(dag != NULL) {
    preferred_parent = dag->preferred_parent;
    if(preferred_parent != NULL) {
      uip_ds6_nbr_t *nbr;
      nbr = uip_ds6_nbr_lookup(rpl_get_parent_ipaddr(preferred_parent));
      if(nbr != NULL) {
        /* Use parts of the IPv6 address as the parent address, in reversed byte order. */
        parent.u8[LINKADDR_SIZE - 1] = nbr->ipaddr.u8[sizeof(uip_ipaddr_t) - 2];
        parent.u8[LINKADDR_SIZE - 2] = nbr->ipaddr.u8[sizeof(uip_ipaddr_t) - 1];
        parent_etx = rpl_get_parent_rank((linkaddr_t *) uip_ds6_nbr_get_ll(nbr)) / 2;
      }
    }
    rtmetric = dag->rank;
    beacon_interval = (uint16_t) ((2L << dag->instance->dio_intcurrent) / 1000);
    num_neighbors = uip_ds6_nbr_num();
  } else {
    rtmetric = 0;
    beacon_interval = 0;
    num_neighbors = 0;
  }

  /* num_neighbors = collect_neighbor_list_num(&tc.neighbor_list); */
  collect_view_construct_message(&msg.msg, &parent,
                                 parent_etx, rtmetric,
                                 num_neighbors, beacon_interval);

  uip_udp_packet_sendto(client_conn, &msg, sizeof(msg),
                        &server_ipaddr, UIP_HTONS(UDP_SERVER_PORT));

}
/*---------------------------------------------------------------------------*/
void
collect_common_net_init(void)
{
#if CONTIKI_TARGET_Z1
  uart0_set_input(serial_line_input_byte);
#else
  uart0_set_input(serial_line_input_byte2);
#endif
  serial_line_init2();
}
/*---------------------------------------------------------------------------*/
static void
print_local_addresses(void)
{
  int i;
  uint8_t state;

  //PRINTF("Client IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      //PRINTF("\n");
      /* hack to make address "final" */
      if (state == ADDR_TENTATIVE) {
        uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
set_global_address(void)
{
  uip_ipaddr_t ipaddr;

  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  /* set server address */
  uip_ip6addr(&server_ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);

}
static uip_ds6_maddr_t *
join_mcast_group(void)
{
  uip_ipaddr_t addr;
  uip_ds6_maddr_t *rv;

  /* First, set our v6 global */
  uip_ip6addr(&addr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&addr, &uip_lladdr);
  uip_ds6_addr_add(&addr, 0, ADDR_AUTOCONF);
  uint8_t state;
  int i;
   for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      uip_debug_ipaddr_print(&uip_ds6_if.addr_list[i].ipaddr);
      //printf("\n");
    }
  }

  /*
   * IPHC will use stateless multicast compression for this destination
   * (M=1, DAC=0), with 32 inline bits (1E 89 AB CD)
   */
  uip_ip6addr(&addr, 0xFF1E,0,0,0,0,0,0x89,0xABCD);
  rv = uip_ds6_maddr_add(&addr);

  if(rv) {
    //printf("Joined multicast group ");
    PRINT6ADDR(&uip_ds6_maddr_lookup(&addr)->ipaddr);
    //PRINTF("\n");
  }
  return rv;
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  PROCESS_BEGIN();
  //printf("node--------------------------------node%d\n",uip_htons(node_id) );
  // static struct etimer periodic;
  // static struct ctimer backoff_timer;
  // static struct etimer et;
  // static struct etimer wait_et;
  if(join_mcast_group() == NULL) {
   //PRINTF("Failed to join multicast group\n");
    PROCESS_EXIT();
  }
  
  PROCESS_PAUSE();

  set_global_address();

  //PRINTF("UDP client process started\n");

  print_local_addresses();
  static struct ctimer heart_back;
  static struct etimer heart_et;
  /* new connection with remote host */
  client_conn = udp_new(NULL, UIP_HTONS(UDP_SERVER_PORT), NULL);
  client_conn_data=udp_new(NULL, UIP_HTONS(UDP_SERVER_DATA_PORT), NULL);
  
  udp_bind(client_conn, UIP_HTONS(UDP_CLIENT_PORT));
  //etimer_set(&periodic, SEND_INTERVAL);
  sink_conn = udp_new(NULL, UIP_HTONS(0), NULL);

  client_conn_unicast_data= udp_new(NULL,UIP_HTONS(0),NULL);

  udp_bind(sink_conn, UIP_HTONS(MCAST_SINK_UDP_PORT));

  udp_bind(client_conn_unicast_data,UIP_HTONS(UDP_SERVER_UNICAST_PORT));
  //printf("%s\n","listen" );
  n_id = uip_htons(node_id);
  int heart_sec = 60*CLOCK_SECOND;
  etimer_set(&heart_et,heart_sec);
  global_period = BASE_PERIOD + SEND_TIME;
  int seconds;
  while(1) {
    PROCESS_YIELD();
    if(ev == tcpip_event) {
      leds_toggle(LEDS_GREEN);
      tcpip_handler();
    }
    // if(etimer_expired(&ack_period)){
    //   if(uploadFlag){
    //    //uip_udp_packet_sendto(client_conn_data, ack_buffer, appdata_length + 1,
    //                  // &server_ipaddr, UIP_HTONS(UDP_SERVER_DATA_PORT));
    //     uip_udp_packet_sendto(client_conn_data, ack_buffer, data_length + 1,
    //                   &server_ipaddr, UIP_HTONS(UDP_SERVER_DATA_PORT));
    //     leds_off(LEDS_GREEN);
    //     uploadFlag = false;
    //   } 
    // }
    /*触发串口事件，读取串口助手的char数据，存在data里*/
    if (ev==serial_line_event_message2)
    {
      //printf("hellworld process message \n:%s\n",(char*)data);
      //char buf[MAX_PAYLOAD_LEN];
      //sprintf(ack_buffer, "%s",(char*)data);//封装在buffer里
      memset(ack_buffer,0,MAX_PAYLOAD_LEN);
      data_length =(int)((char *)data)[0];
      //memcpy(ack_buffer,(char *)data,strlen(data));
      ack_buffer[0] = global_type;
      memcpy(ack_buffer + 1,(char *)data+1,data_length);
      //seconds =  BASE_PERIOD + SEND_TIME;
      //etimer_set(&ack_period,global_period);
      etimer_set(&report_period,global_period);
      uploadFlag = true;
      //uip_udp_packet_sendto(client_conn_data, buf, strlen(buf),
      //                  &server_ipaddr, UIP_HTONS(UDP_SERVER_DATA_PORT));
      leds_toggle(LEDS_GREEN);
      //PRINT6ADDR(&server_ipaddr);
    }
    if(etimer_expired(&heart_et)){
      etimer_reset(&heart_et);
      ctimer_set(&heart_back, SEND_TIME, send_packet, NULL);
      //uip_udp_packet_sendto(client_conn_data, HERAT_BODY, 1,
                     // &server_ipaddr, UIP_HTONS(UDP_SERVER_DATA_PORT));
      leds_toggle(LEDS_GREEN);
        
    }
    if(etimer_expired(&report_period)){
      if(uploadFlag){
        if(cur_State == state_ack){
           uip_udp_packet_sendto(client_conn_data, con_buffer, 1,
                      &server_ipaddr, UIP_HTONS(UDP_SERVER_DATA_PORT));
           cur_State = state_ready;
        }else if(cur_State == state_ready){
           uip_udp_packet_sendto(client_conn_data, ack_buffer, data_length + 1,
                      &server_ipaddr, UIP_HTONS(UDP_SERVER_DATA_PORT));
        }

        leds_toggle(LEDS_GREEN);
        uploadFlag = false;
      }
      
    }

  //  if (ev == serial_line_event_message2)
  //     {
  //       /* code */
  //       memset(data_buffer,0,MAX_BUFFER_LENGTH);
  //       int data_length = (int)((char *)data)[0];
  //      memcpy(data_buffer,(char *)data + 1,data_length);
  //      etimer_set(&wait_et, WAIT_TIME+BASE_TIME);
  //     }
  //     if(etimer_expired(&wait_et)){
  //        uip_udp_packet_sendto(client_conn_data, data_buffer, strlen(data_buffer),
  //                      &server_ipaddr, UIP_HTONS(UDP_SERVER_DATA_PORT));
  //     }
  // }
}
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
