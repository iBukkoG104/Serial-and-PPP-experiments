#include <Arduino.h>
#include "lwipopts.h"
#include <lwIP_Arduino.h>
//#include <lwip/include/netif/ppp>
#include <lwip/include/netif/ppp/pppapi.h>
#include <lwip/include/netif/ppp/pppos.h>

#define MODEM Serial1
#define MODEM_BAUD 115200

static ppp_pcb *ppp;
static struct netif ppp_netif;

//
// Callback di output: manda i pacchetti al modem via seriale
//
static u32_t ppp_output_cb(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx) {
    MODEM.write(data, len);
    return len;
}

//
// Callback di stato PPP (up/down)
//
static void ppp_status_cb(ppp_pcb *pcb, int err_code, void *ctx) {
    if (err_code == PPPERR_NONE) {
        Serial.println("PPP LINK UP");
    } else {
        Serial.print("PPP ERROR: ");
        Serial.println(err_code);
    }
}

//
// Inizializza LwIP + PPP
//
void ppp_init_start() {
    lwip_init();

    MODEM.begin(MODEM_BAUD);

    // Autentica il PPP
    //ppp = pppos_create(&ppp_netif, ppp_output_cb, ppp_status_cb, NULL);
    ppp = pppos_create(&ppp_netif, NULL, ppp_status_cb, NULL);

    ppp_set_auth(ppp, PPPAUTHTYPE_PAP, "user", "pass");

    ppp_connect(ppp, 0);
}

//
// Feed dei dati PPP (modem → LwIP)
//
void ppp_input_task() {
    while (MODEM.available()) {
        uint8_t c = MODEM.read();
        pppos_input_tcpip(ppp, &c, 1);
    }
}


void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("Init LwIP + PPPoS...");
    ppp_init_start();
}

void loop() {
    // Gestisce input PPP
    ppp_input_task();

    // Gestisce timers (TCP timeout, ARP, ecc)
    sys_check_timeouts();
}
