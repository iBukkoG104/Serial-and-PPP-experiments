// ==================================================================
// PPPoS HTTP Client per Arduino DUE usando PPPoSClient
// ==================================================================
// Hardware: Arduino DUE + Serial1 per PPPoS + Display LCD
// Libreria: https://github.com/levkovigor/ppposclient

#include <Arduino.h>
extern "C"{
  #include <lwIP_Arduino.h>
  #include </home/matteo/Arduino/libraries/lwIP/src/lwip/include/lwip/dns.h>
}

#include <PPPOSClient.h>

// ==================================================================
// CONFIGURAZIONE
// ==================================================================
#define SERIAL_PPP Serial1     // Seriale per PPPoS
#define SERIAL_DEBUG Serial    // Seriale per debug
#define PPP_BAUDRATE 115200

#define HTTP_SERVER "filepedia.tplinkdns.com"  // Server da interrogare
#define HTTP_PORT 443
#define HTTP_PATH "/.secret/test.html"

// Opzionale: credenziali PPP (se il server le richiede)
// #define PPP_USERNAME "user"
// #define PPP_PASSWORD "pass"

// ==================================================================
// VARIABILI GLOBALI
// ==================================================================
PPPoSClient pppClient;
bool pppConnected = false;
unsigned long lastRequest = 0;
const unsigned long REQUEST_INTERVAL = 30000; // 30 secondi

// Buffer per risposta HTTP
String httpResponse = "";
bool requestInProgress = false;

// ==================================================================
// CALLBACK PPPoS
// ==================================================================
void pppEventCallback(ppp_client_event_t event) {
  switch(event) {
    case PPP_CLIENT_CONNECTED:
      SERIAL_DEBUG.println("\n=== PPP CONNESSO ===");
      SERIAL_DEBUG.print("IP locale: ");
      SERIAL_DEBUG.println(pppClient.localIP());
      SERIAL_DEBUG.print("Gateway: ");
      SERIAL_DEBUG.println(pppClient.gatewayIP());
      SERIAL_DEBUG.print("Subnet: ");
      SERIAL_DEBUG.println(pppClient.subnetMask());
      SERIAL_DEBUG.print("DNS1: ");
      SERIAL_DEBUG.println(pppClient.dnsIP(0));
      SERIAL_DEBUG.print("DNS2: ");
      SERIAL_DEBUG.println(pppClient.dnsIP(1));
      SERIAL_DEBUG.println("====================\n");
      pppConnected = true;
      break;
      
    case PPP_CLIENT_DISCONNECTED:
      SERIAL_DEBUG.println("\n!!! PPP DISCONNESSO !!!");
      pppConnected = false;
      break;
      
    case PPP_CLIENT_ERROR:
      SERIAL_DEBUG.println("\n!!! ERRORE PPP !!!");
      pppConnected = false;
      break;
      
    default:
      break;
  }
}

// ==================================================================
// FUNZIONI HTTP
// ==================================================================

// Funzione per fare richiesta HTTP GET
bool httpGet(const char* host, uint16_t port, const char* path, String &response) {
  if (!pppConnected) {
    SERIAL_DEBUG.println("Errore: PPP non connesso!");
    return false;
  }
  
  SERIAL_DEBUG.print("\n=== HTTP GET Request ===\n");
  SERIAL_DEBUG.print("Host: ");
  SERIAL_DEBUG.println(host);
  SERIAL_DEBUG.print("Port: ");
  SERIAL_DEBUG.println(port);
  SERIAL_DEBUG.print("Path: ");
  SERIAL_DEBUG.println(path);
  
  // Crea client WiFi (compatibile con PPPoSClient)
  WiFiClient client;
  
  // Connetti al server
  SERIAL_DEBUG.print("Connessione a ");
  SERIAL_DEBUG.print(host);
  SERIAL_DEBUG.print(":");
  SERIAL_DEBUG.println(port);
  
  if (!client.connect(host, port)) {
    SERIAL_DEBUG.println("Connessione fallita!");
    return false;
  }
  
  SERIAL_DEBUG.println("Connesso! Invio richiesta HTTP...");
  
  // Costruisci e invia richiesta HTTP GET
  client.print("GET ");
  client.print(path);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(host);
  client.println("Connection: close");
  client.println("User-Agent: Arduino-DUE-PPPoS");
  client.println();
  
  SERIAL_DEBUG.println("Richiesta inviata. Attendo risposta...");
  
  // Attendi risposta (timeout 10 secondi)
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 10000) {
      SERIAL_DEBUG.println("Timeout!");
      client.stop();
      return false;
    }
    delay(10);
  }
  
  // Leggi risposta
  response = "";
  while (client.available()) {
    char c = client.read();
    response += c;
  }
  
  client.stop();
  
  SERIAL_DEBUG.print("Risposta ricevuta: ");
  SERIAL_DEBUG.print(response.length());
  SERIAL_DEBUG.println(" bytes");
  
  return true;
}

// Estrae il body HTML dalla risposta HTTP
String extractHTMLBody(const String &httpResponse) {
  // Trova l'inizio del body (dopo headers HTTP)
  int bodyStart = httpResponse.indexOf("\r\n\r\n");
  if (bodyStart == -1) {
    bodyStart = httpResponse.indexOf("\n\n");
    if (bodyStart != -1) {
      bodyStart += 2;
    }
  } else {
    bodyStart += 4;
  }
  
  if (bodyStart == -1) {
    return httpResponse; // Nessun header trovato, ritorna tutto
  }
  
  return httpResponse.substring(bodyStart);
}

// Rimuove tag HTML per mostrare solo testo
String stripHTMLTags(const String &html) {
  String result = "";
  bool inTag = false;
  
  for (size_t i = 0; i < html.length(); i++) {
    char c = html[i];
    
    if (c == '<') {
      inTag = true;
    } else if (c == '>') {
      inTag = false;
    } else if (!inTag) {
      result += c;
    }
  }
  
  return result;
}

// Mostra contenuto su LCD (esempio generico - adatta al tuo display)
void displayOnLCD(const String &content) {
  SERIAL_DEBUG.println("\n=== CONTENUTO DA MOSTRARE SU LCD ===");
  
  // Rimuovi tag HTML
  String textOnly = stripHTMLTags(content);
  
  // Rimuovi spazi multipli e newline eccessive
  textOnly.trim();
  textOnly.replace("\r\n", " ");
  textOnly.replace("\n", " ");
  textOnly.replace("\r", " ");
  while (textOnly.indexOf("  ") != -1) {
    textOnly.replace("  ", " ");
  }
  
  // Stampa su debug (qui dovresti usare il tuo LCD)
  SERIAL_DEBUG.println(textOnly);
  
  // Esempio per LCD 16x2 o 20x4:
  /*
  lcd.clear();
  lcd.setCursor(0, 0);
  
  // Mostra primi 16 caratteri sulla prima riga
  if (textOnly.length() > 0) {
    lcd.print(textOnly.substring(0, min(16, (int)textOnly.length())));
  }
  
  // Se hai più righe
  if (textOnly.length() > 16) {
    lcd.setCursor(0, 1);
    lcd.print(textOnly.substring(16, min(32, (int)textOnly.length())));
  }
  */
  
  SERIAL_DEBUG.println("====================================\n");
}

// ==================================================================
// SETUP
// ==================================================================
void setup() {
  // Inizializza seriale debug
  SERIAL_DEBUG.begin(115200);
  delay(2000);
  
  SERIAL_DEBUG.println("\n\n");
  SERIAL_DEBUG.println("========================================");
  SERIAL_DEBUG.println("  Arduino DUE - PPPoS HTTP Client");
  SERIAL_DEBUG.println("========================================");
  
  // Inizializza seriale PPPoS
  SERIAL_PPP.begin(PPP_BAUDRATE);
  SERIAL_DEBUG.print("Seriale PPPoS inizializzata a ");
  SERIAL_DEBUG.print(PPP_BAUDRATE);
  SERIAL_DEBUG.println(" baud");
  
  // Configura PPPoS Client
  SERIAL_DEBUG.println("\nConfigurazione PPPoS Client...");
  
  // Imposta callback per eventi PPP
  pppClient.setCallback(pppEventCallback);
  
  // Inizializza PPPoS sulla seriale
  pppClient.begin(&SERIAL_PPP);
  
  // Opzionale: imposta credenziali se necessario
  #ifdef PPP_USERNAME
  pppClient.setAuth(PPP_USERNAME, PPP_PASSWORD);
  SERIAL_DEBUG.println("Autenticazione PPP configurata");
  #endif
  
  SERIAL_DEBUG.println("\nAvvio connessione PPP...");
  SERIAL_DEBUG.println("Attendi...\n");
  
  // Inizializza il tuo LCD qui
  /*
  lcd.begin(16, 2); // o lcd.init() per I2C
  lcd.backlight();
  lcd.clear();
  lcd.print("Connessione PPP");
  lcd.setCursor(0, 1);
  lcd.print("In corso...");
  */
}

// ==================================================================
// LOOP
// ==================================================================
void loop() {
  // Mantieni attiva la connessione PPP
  pppClient.loop();
  
  // Se PPP è connesso e non c'è una richiesta in corso
  if (pppConnected && !requestInProgress) {
    // Fai richiesta HTTP ogni REQUEST_INTERVAL millisecondi
    if (millis() - lastRequest >= REQUEST_INTERVAL) {
      lastRequest = millis();
      requestInProgress = true;
      
      SERIAL_DEBUG.println("\n>>> Avvio richiesta HTTP periodica <<<");
      
      // Fai richiesta HTTP GET
      String response = "";
      if (httpGet(HTTP_SERVER, HTTP_PORT, HTTP_PATH, response)) {
        // Estrai body HTML
        String htmlBody = extractHTMLBody(response);
        
        // Mostra su LCD
        displayOnLCD(htmlBody);
        
        // Se vuoi vedere l'HTML completo sul debug
        SERIAL_DEBUG.println("\n=== HTML BODY COMPLETO ===");
        SERIAL_DEBUG.println(htmlBody);
        SERIAL_DEBUG.println("==========================\n");
      } else {
        SERIAL_DEBUG.println("Richiesta HTTP fallita!");
        
        // Mostra errore su LCD
        /*
        lcd.clear();
        lcd.print("Errore HTTP!");
        */
      }
      
      requestInProgress = false;
    }
  }
  
  // Piccolo delay per non sovraccaricare
  delay(100);
}

// ==================================================================
// FUNZIONI HELPER AGGIUNTIVE
// ==================================================================

// Funzione per fare richiesta HTTPS (se supportato)
// Nota: richiede che PPPoSClient supporti WiFiClientSecure
/*
bool httpsGet(const char* host, uint16_t port, const char* path, String &response) {
  if (!pppConnected) {
    return false;
  }
  
  WiFiClientSecure client;
  client.setInsecure(); // Salta verifica certificato (solo per test!)
  
  if (!client.connect(host, port)) {
    return false;
  }
  
  client.print("GET ");
  client.print(path);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(host);
  client.println("Connection: close");
  client.println();
  
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 10000) {
      client.stop();
      return false;
    }
  }
  
  response = "";
  while (client.available()) {
    response += (char)client.read();
  }
  
  client.stop();
  return true;
}
*/

// Funzione per fare POST request
bool httpPost(const char* host, uint16_t port, const char* path, 
              const String &postData, String &response) {
  if (!pppConnected) {
    return false;
  }
  
  WiFiClient client;
  
  if (!client.connect(host, port)) {
    return false;
  }
  
  client.print("POST ");
  client.print(path);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(host);
  client.println("Connection: close");
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.print("Content-Length: ");
  client.println(postData.length());
  client.println();
  client.print(postData);
  
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 10000) {
      client.stop();
      return false;
    }
  }
  
  response = "";
  while (client.available()) {
    response += (char)client.read();
  }
  
  client.stop();
  return true;
}