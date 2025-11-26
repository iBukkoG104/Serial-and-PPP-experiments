// ==================================================================
// lwIP PPPoS Implementation per Arduino DUE
// ==================================================================
// Questo codice implementa lwIP con PPPoS su Arduino DUE
// Hardware: Arduino DUE + Serial per PPPoS + Display LCD

#include <Arduino.h>

// Configurazione lwIP
#define LWIP_SINGLE_NETIF 1
#define LWIP_NETIF_API 1
#define LWIP_SOCKET 0
#define LWIP_NETCONN 0

// Includi lwIP headers (assicurati di avere la libreria lwIP installata)
/*extern "C" {
  #include "include/lwip/opt.h"
  #include "lwip/init.h"
  #include "lwip/mem.h"
  #include "lwip/memp.h"
  #include "lwip/sys.h"
  #include "lwip/timeouts.h"
  #include "lwip/ip.h"
  #include "lwip/netif.h"
  #include "lwip/sio.h"
  #include "netif/ppp/pppos.h"
  #include "netif/ppp/ppp.h"
  #include "lwip/dns.h"
  #include "lwip/tcp.h"
}
*/



extern "C" {
  //#include "lwipopts.h"
  #include <lwIP_Arduino.h>
  #include <lwip/include/lwip/opt.h>
  #include <lwip/include/lwip/mem.h>
  #include <lwip/include/lwip/memp.h>
  #include <lwip/include/lwip/sys.h>
  #include <lwip/include/lwip/ip.h>
  #include <lwip/include/lwip/netif.h>
  #include <lwip/include/lwip/sio.h>
  #include <lwip/include/lwip/pbuf.h>
  //#include <lwip/include/netif/ppp/ppp_opts.h>
  #include <lwip/include/netif/ppp/pppos.h>
  #include <lwip/include/netif/ppp/ppp.h>
  #include <lwip/include/lwip/tcp.h>
  #include <lwip/include/lwip/dns.h>
}



// ==================================================================
// CONFIGURAZIONE
// ==================================================================
#define SERIAL_PPP Serial1  // Usa Serial1 per PPPoS
#define SERIAL_DEBUG Serial // Usa Serial per debug
#define PPP_BAUDRATE 115200

// Variabili globali lwIP
static ppp_pcb *ppp;
static struct netif ppp_netif;
static volatile bool ppp_connected = false;

// ==================================================================
// CALLBACK PPPoS
// ==================================================================

// Callback stato PPP
static void ppp_status_cb(ppp_pcb *pcb, int err_code, void *ctx) {
  struct netif *pppif = ppp_netif(pcb);
  
  switch(err_code) {
    case PPPERR_NONE:
      SERIAL_DEBUG.println("PPP: Connesso!");
      SERIAL_DEBUG.print("IP: ");
      SERIAL_DEBUG.println(ip4addr_ntoa(netif_ip4_addr(pppif)));
      SERIAL_DEBUG.print("Gateway: ");
      SERIAL_DEBUG.println(ip4addr_ntoa(netif_ip4_gw(pppif)));
      SERIAL_DEBUG.print("Netmask: ");
      SERIAL_DEBUG.println(ip4addr_ntoa(netif_ip4_netmask(pppif)));
      ppp_connected = true;
      break;
      
    case PPPERR_PARAM:
      SERIAL_DEBUG.println("PPP: Errore parametri");
      break;
      
    case PPPERR_OPEN:
      SERIAL_DEBUG.println("PPP: Impossibile aprire");
      break;
      
    case PPPERR_DEVICE:
      SERIAL_DEBUG.println("PPP: Errore device");
      break;
      
    case PPPERR_ALLOC:
      SERIAL_DEBUG.println("PPP: Errore allocazione");
      break;
      
    case PPPERR_USER:
      SERIAL_DEBUG.println("PPP: Disconnesso dall'utente");
      ppp_connected = false;
      break;
      
    case PPPERR_CONNECT:
      SERIAL_DEBUG.println("PPP: Errore connessione");
      ppp_connected = false;
      break;
      
    case PPPERR_AUTHFAIL:
      SERIAL_DEBUG.println("PPP: Autenticazione fallita");
      ppp_connected = false;
      break;
      
    case PPPERR_PROTOCOL:
      SERIAL_DEBUG.println("PPP: Errore protocollo");
      ppp_connected = false;
      break;
      
    case PPPERR_PEERDEAD:
      SERIAL_DEBUG.println("PPP: Peer disconnesso");
      ppp_connected = false;
      break;
      
    default:
      SERIAL_DEBUG.print("PPP: Errore sconosciuto: ");
      SERIAL_DEBUG.println(err_code);
      ppp_connected = false;
      break;
  }
}

// Funzione output per PPPoS - invia dati sulla seriale
static u32_t ppp_output_cb(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx) {
  return SERIAL_PPP.write(data, len);
}

// ==================================================================
// GESTIONE SERIALE PPPoS
// ==================================================================

void ppp_serial_input() {
  static uint8_t buffer[512];
  
  if (SERIAL_PPP.available()) {
    size_t len = SERIAL_PPP.readBytes(buffer, sizeof(buffer));
    if (len > 0) {
      pppos_input(ppp, buffer, len);
    }
  }
}

// ==================================================================
// HTTP CLIENT
// ==================================================================

struct http_request {
  char *hostname;
  char *path;
  uint16_t port;
  void (*callback)(const char *response, size_t len);
  tcp_pcb *pcb;
  char *response_buffer;
  size_t response_len;
  size_t response_capacity;
};

static struct http_request *current_request = NULL;

// Callback errore TCP
static void http_error_cb(void *arg, err_t err) {
  SERIAL_DEBUG.print("HTTP Error: ");
  SERIAL_DEBUG.println(err);
  
  if (current_request) {
    if (current_request->response_buffer) {
      free(current_request->response_buffer);
    }
    free(current_request);
    current_request = NULL;
  }
}

// Callback ricezione dati TCP
static err_t http_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
  if (err != ERR_OK || p == NULL) {
    if (current_request) {
      if (p == NULL && current_request->callback && current_request->response_buffer) {
        // Connessione chiusa, chiama callback con dati ricevuti
        current_request->callback(current_request->response_buffer, current_request->response_len);
      }
      
      tcp_close(tpcb);
      if (current_request->response_buffer) {
        free(current_request->response_buffer);
      }
      free(current_request);
      current_request = NULL;
    }
    return ERR_OK;
  }
  
  // Copia dati dal pbuf al buffer
  if (current_request) {
    size_t copy_len = p->tot_len;
    
    // Alloca o espandi buffer se necessario
    if (current_request->response_len + copy_len > current_request->response_capacity) {
      current_request->response_capacity = current_request->response_len + copy_len + 1024;
      current_request->response_buffer = (char*)realloc(current_request->response_buffer, 
                                                        current_request->response_capacity);
    }
    
    // Copia dati
    pbuf_copy_partial(p, current_request->response_buffer + current_request->response_len, 
                      copy_len, 0);
    current_request->response_len += copy_len;
  }
  
  tcp_recved(tpcb, p->tot_len);
  pbuf_free(p);
  
  return ERR_OK;
}

// Callback connessione TCP stabilita
static err_t http_connected_cb(void *arg, struct tcp_pcb *tpcb, err_t err) {
  if (err != ERR_OK) {
    SERIAL_DEBUG.println("Errore connessione TCP");
    return err;
  }
  
  SERIAL_DEBUG.println("TCP connesso, invio richiesta HTTP...");
  
  if (!current_request) return ERR_ARG;
  
  // Costruisci richiesta HTTP GET
  char request[512];
  snprintf(request, sizeof(request),
           "GET %s HTTP/1.1\r\n"
           "Host: %s\r\n"
           "Connection: close\r\n"
           "User-Agent: Arduino-DUE-lwIP\r\n"
           "\r\n",
           current_request->path,
           current_request->hostname);
  
  // Invia richiesta
  err_t write_err = tcp_write(tpcb, request, strlen(request), TCP_WRITE_FLAG_COPY);
  if (write_err == ERR_OK) {
    tcp_output(tpcb);
    SERIAL_DEBUG.println("Richiesta HTTP inviata");
  } else {
    SERIAL_DEBUG.print("Errore invio: ");
    SERIAL_DEBUG.println(write_err);
  }
  
  return ERR_OK;
}

// Funzione pubblica per fare richiesta HTTP GET
void http_get(const char *hostname, uint16_t port, const char *path, 
              void (*callback)(const char *response, size_t len)) {
  
  if (!ppp_connected) {
    SERIAL_DEBUG.println("PPP non connesso!");
    return;
  }
  
  if (current_request) {
    SERIAL_DEBUG.println("Richiesta già in corso!");
    return;
  }
  
  // Alloca struttura richiesta
  current_request = (struct http_request*)malloc(sizeof(struct http_request));
  current_request->hostname = strdup(hostname);
  current_request->path = strdup(path);
  current_request->port = port;
  current_request->callback = callback;
  current_request->response_buffer = NULL;
  current_request->response_len = 0;
  current_request->response_capacity = 0;
  
  // Crea PCB TCP
  current_request->pcb = tcp_new();
  if (!current_request->pcb) {
    SERIAL_DEBUG.println("Errore creazione TCP PCB");
    free(current_request);
    current_request = NULL;
    return;
  }
  
  // Setup callbacks
  tcp_err(current_request->pcb, http_error_cb);
  tcp_recv(current_request->pcb, http_recv_cb);
  
  // Risolvi hostname
  SERIAL_DEBUG.print("Risoluzione DNS per: ");
  SERIAL_DEBUG.println(hostname);
  
  ip_addr_t server_ip;
  //err_t err = dns_gethostbyname(hostname, &server_ip, NULL, NULL);
  
  /*if (err == ERR_INPROGRESS) {
    SERIAL_DEBUG.println("DNS in corso...");
    // In una implementazione completa dovresti usare un callback DNS
    // Per semplicità qui usiamo un IP diretto se disponibile
    return;
  } else if (err == ERR_OK) {
    SERIAL_DEBUG.print("IP risolto: ");
    SERIAL_DEBUG.println(ip4addr_ntoa(&server_ip));
    
    // Connetti al server
    err = tcp_connect(current_request->pcb, &server_ip, port, http_connected_cb);
    if (err != ERR_OK) {
      SERIAL_DEBUG.print("Errore tcp_connect: ");
      SERIAL_DEBUG.println(err);
    }
  }*/
}

// ==================================================================
// SETUP E LOOP
// ==================================================================

void setup() {
  // Inizializza seriali
  SERIAL_DEBUG.begin(115200);
  SERIAL_PPP.begin(PPP_BAUDRATE);
  
  delay(2000);
  SERIAL_DEBUG.println("\n=== Arduino DUE lwIP PPPoS ===");
  
  // Inizializza lwIP
  SERIAL_DEBUG.println("Inizializzazione lwIP...");
  lwip_init();
  
  // Crea interfaccia PPPoS
  SERIAL_DEBUG.println("Creazione interfaccia PPPoS...");
  //ppp = pppos_create(&ppp_netif, ppp_output_cb, ppp_status_cb, NULL);
  ppp = pppos_create(&ppp_netif, NULL, ppp_status_cb, NULL);
  
  if (!ppp) {
    SERIAL_DEBUG.println("ERRORE: Impossibile creare PPPoS!");
    while(1);
  }
  
  // Imposta default netif
  netif_set_default(&ppp_netif);
  
  // Configura autenticazione (se necessario)
  // ppp_set_auth(ppp, PPPAUTHTYPE_PAP, "username", "password");
  
  // Connetti PPP
  SERIAL_DEBUG.println("Connessione PPP...");
  ppp_connect(ppp, 0);
  
  SERIAL_DEBUG.println("Setup completato. In attesa connessione PPP...");
}

void loop() {
  // Processa input seriale PPP
  ppp_serial_input();
  
  // Processa timer lwIP
  sys_check_timeouts();
  
  // Esempio: fai richiesta HTTP ogni 30 secondi se connesso
  static unsigned long lastRequest = 0;
  if (ppp_connected && millis() - lastRequest > 30000) {
    lastRequest = millis();
    
    SERIAL_DEBUG.println("\n=== Invio richiesta HTTP ===");
    http_get("filepedia.tplinkdns.com", 443, "/.secret/test.html", [](const char *response, size_t len) {
      SERIAL_DEBUG.println("\n=== Risposta ricevuta ===");
      SERIAL_DEBUG.print("Lunghezza: ");
      SERIAL_DEBUG.println(len);
      SERIAL_DEBUG.println("Contenuto:");
      SERIAL_DEBUG.write((const uint8_t*)response, min(len, (size_t)1000));
      SERIAL_DEBUG.println("\n=== Fine risposta ===");
    });
  }
  
  delay(10);
}