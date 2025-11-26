#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// No OS
#define NO_SYS                          1
#define SYS_LIGHTWEIGHT_PROT            0
// #define PPP_INPROC_IRQ_SAFE				1

// Memory
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (16 * 1024)

#define MEMP_NUM_PBUF                   16
#define PBUF_POOL_SIZE                  16
#define PBUF_POOL_BUFSIZE               512

// TCP / IP
#define LWIP_TCP                        1
#define LWIP_IPV4                       1
#define LWIP_ARP                        0
#define LWIP_ICMP                       1
#define LWIP_DNS                        1

// PPP
#define PPP_SUPPORT                     1
#define PPPOS_SUPPORT                   1
#define PAP_SUPPORT                     1
#define CHAP_SUPPORT                    0


// GEMINI(TM) ADDITIONS
//#define LWIP_SOCKET			0
//#define LWIP_NETCONN		0

#endif
