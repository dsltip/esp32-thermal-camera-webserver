/*
IDE 1.8.18
 MLX90640 thermal camera connected to a SparkFun Thing Plus - ESP32 WROOM

 Created by: Christopher Black
 */

#include <WiFi.h>
#include <ESPmDNS.h>
#include <Wire.h>  // Used for I2C communication
//#include <SFE_MicroOLED.h>  // Include the SFE_MicroOLED library
#include <WebSocketsServer.h>
#include <HTTPUpdate.h>

#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"
//#include "env.h"
#include "webpage.h"

// MicroOLED variables
//#define PIN_RESET 9  
//#define DC_JUMPER 1 
// MicroOLED oled(PIN_RESET, DC_JUMPER);    // I2C declaration

// WiFi variables
const char* ssid     = "DSLAN4G";
const char* password = "dslan3218";
WiFiServer server(80);
IPAddress apIP(192, 168, 214, 1);
IPAddress locip;
//IPAddress ip(192,168,253,240);   
//IPAddress gateway(192,168,253,1);   
//IPAddress subnet(255,255,255,0);   


//declare socket related variables
WebSocketsServer webSocket = WebSocketsServer(81);

// MLX90640 variables
//#define TA_SHIFT -64; // Default shift for MLX90640 in open air is 8
#define TA_SHIFT 8; // Default shift for MLX90640 in open air is 8
static float mlx90640To[768];

// Used to compress data to the client
char positive[42] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()-_+=9";
char negative[42] = "abcdefghijklmnopqrstuvwxyz<>.,'|~`?\\/:;{}";

TaskHandle_t TaskA;
/* this variable hold queue handle */
xQueueHandle xQueue;

int total = 0;
void update_started() {
  Serial.println("CALLBACK:  HTTP update process started");
}
void update_finished() {
  Serial.println("CALLBACK:  HTTP update process finished");
}
void update_progress(int cur, int total) {
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}
void update_error(int err) {
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}
void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    // Connect to the WiFi network
    WiFi.begin(ssid, password);
    //WiFi.config(ip, gateway, subnet);
    WiFi.setHostname("esp32thing1");
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        retry += 1;
        Serial.print(".");
        if (retry > 4 ) {
          // Retry after 5 seconds
          Serial.println("");
          WiFi.begin(ssid, password);
          retry = 0;
        }
    }

    Serial.println("");
//    Serial.println("WiFi connected!!.");
    Serial.println("IP address: ");
    locip = WiFi.localIP();
    Serial.println(locip);
    Serial.println(locip[3]);
    if(locip[3]==222){
      WiFiClient client0;
      httpUpdate.onStart(update_started);
      httpUpdate.onEnd(update_finished);
      httpUpdate.onProgress(update_progress);
      httpUpdate.onError(update_error);
  
      t_httpUpdate_return ret = httpUpdate.update(client0, "http://192.168.137.1/esp32termal.bin");
      switch (ret) {
        case HTTP_UPDATE_FAILED: Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());   break;
        case HTTP_UPDATE_NO_UPDATES: Serial.println("HTTP_UPDATE_NO_UPDATES");  break;
        case HTTP_UPDATE_OK: Serial.println("HTTP_UPDATE_OK");   break;
      }
      //while(1==1){};//halt
    }
    
  /*  WiFi.mode(WIFI_AP);
    delay(10);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0)); 
    WiFi.softAP("DSMXL", password); 
    delay(100);
    
    Serial.println("WiFi AP started.");
    Serial.println("IP address: ");
    Serial.println(WiFi.softAPIP());*/

    if (!MDNS.begin("thermal")) {
        Serial.println("Error setting up MDNS responder!");
    } else {
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("ws", "tcp", 81);
        Serial.println("mDNS responder started");
    }
    server.begin();
    
    xQueue = xQueueCreate(1, sizeof(mlx90640To));
    xTaskCreatePinnedToCore(
      Task1,                  /* pvTaskCode */
      "Workload1",            /* pcName */
      100000,                   /* usStackDepth */
      NULL,                   /* pvParameters */
      1,                      /* uxPriority */
      &TaskA,                 /* pxCreatedTask */
      0);                     /* xCoreID */
    xTaskCreate(
      receiveTask,           /* Task function. */
      "receiveTask",        /* name of task. */
      10000,                    /* Stack size of task */
      NULL,                     /* parameter of the task */
      1,                        /* priority of the task */
      NULL);                    /* Task handle to keep track of created task */

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
}

int value = 0;
int dataVal = 0;

void loop(){
  webSocket.loop();
  
  WiFiClient client = server.available();   // listen for incoming clients

  if (client) {
    Serial.println("New Client.");
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        if (c == '\n') {

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.print("<script>const ipAddress = '"); 
            client.print(WiFi.localIP());
            //client.print(WiFi.softAPIP());
            client.print("'</script>");
            client.println();
            // the content of the HTTP response follows the header:
            client.print(canvas_htm);
            
            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println("Client Disconnected.");
  }
}

// Capture thermal image on a different thread
void Task1( void * parameter )
{
    int tick = 0;
    const byte MLX90640_address = 0x33; //Default 7-bit unshifted address of the MLX90640
    
   // MicroOLED oled(PIN_RESET, DC_JUMPER);    // I2C declaration
    Wire.setClock(400000L);
    Wire.begin();
    
    //oled.begin();    // Initialize the OLED
    //oled.clear(ALL); // Clear the display's internal memory
    //delay(50);
    //oled.clear(PAGE); // Clear the buffer
    //oled.print("Welcome!");
    delay(400);
    //oled.display(); // Draw on the screen
    paramsMLX90640 mlx90640;
    Wire.beginTransmission((uint8_t)MLX90640_address);
    if (Wire.endTransmission() != 0) {
        Serial.println("MLX90640 not detected at default I2C address. Please check wiring. Freezing.");
        while (1);
    }
    Serial.println("MLX90640 online!");

    //Get device parameters - We only have to do this once
    int status;
    uint16_t eeMLX90640[832];
    status = MLX90640_DumpEE(MLX90640_address, eeMLX90640);

    if (status != 0) {
        Serial.println("Failed to load system parameters");
    }
    status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
    if (status != 0) {
        Serial.println("Parameter extraction failed");
    }
    MLX90640_SetRefreshRate(MLX90640_address, 0x05);
    Wire.setClock(1000000L);
    float mlx90640Background[768];
    for( ;; )
    {
//      String startMessage = "Capturing thermal image on core ";
//      startMessage.concat(xPortGetCoreID());
//      Serial.println( startMessage );
//      long startTime = millis();
      for (byte x = 0 ; x < 2 ; x++) //Read both subpages
      {
        uint16_t mlx90640Frame[834];
        int status = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
        if (status < 0)
        {
          Serial.print("GetFrame Error: ");
          Serial.println(status);
        }
    
        float vdd = MLX90640_GetVdd(mlx90640Frame, &mlx90640);
        float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);
  
        float tr = Ta - TA_SHIFT; //Reflected temperature based on the sensor ambient temperature
        float emissivity = 0.95;
  
        MLX90640_CalculateTo(mlx90640Frame, &mlx90640, emissivity, tr, mlx90640Background);
      }
//      long stopReadTime = millis();
//      Serial.print("Read rate: ");
//      Serial.print( 1000.0 / (stopReadTime - startTime), 2);
//      Serial.println(" Hz");
      /*tick += 1;
      if (tick > 10) {
        float maxReading = mlx90640To[0];
        float minReading = mlx90640To[0];
        float avgReading = mlx90640To[0];
        for (int x = 0 ; x < 768 ; x++)
        {
          if (isnan(mlx90640To[x])) {
            continue;
          }
          avgReading = (avgReading + mlx90640To[x]) / 2;
          if ( mlx90640To[x] > maxReading) {
            maxReading = mlx90640To[x];
          }
          if ( mlx90640To[x] < minReading) {
            minReading = mlx90640To[x];
          }
        }
        avgReading = avgReading * 1.8 + 32;
        maxReading = maxReading * 1.8 + 32;
        minReading = minReading * 1.8 + 32;
        String output = "Max:";
        output.concat(maxReading);
        String minOutput = "Min:";
        minOutput.concat(minReading);
        String avgOutput = "Avg:";
        avgOutput.concat(avgReading);
        oled.setCursor(0, 0);
        oled.clear(PAGE); 
        oled.print(output);
        oled.setCursor(0, 10);
        oled.print(minOutput);
        oled.setCursor(0, 20);
        oled.print(avgOutput);
        oled.display();
        tick = 0;
      }*/
      /* time to block the task until the queue has free space */
      const TickType_t xTicksToWait = pdMS_TO_TICKS(100);
      xQueueSendToFront( xQueue, &mlx90640Background, xTicksToWait );
      
      const TickType_t xDelay = 20 / portTICK_PERIOD_MS; // 8 Hz is 1/8 second
      vTaskDelay(xDelay);
  }
}

void receiveTask( void * parameter )
{
  /* keep the status of receiving data */
  BaseType_t xStatus;
  /* time to block the task until data is available */
  const TickType_t xTicksToWait = pdMS_TO_TICKS(100);
  for(;;){
    /* receive data from the queue */
    xStatus = xQueueReceive( xQueue, &mlx90640To, xTicksToWait );
    /* check whether receiving is ok or not */
    if(xStatus == pdPASS){
      compressAndSend();
      total += 1;
    }
  }
  vTaskDelete( NULL );
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println("Socket Disconnected.");
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.println("Socket Connected.");
                // send message to client
                webSocket.sendTXT(num, "Connected");
            }
            break;
        case WStype_TEXT:
            // send message to client
            break;
        case WStype_BIN:
        case WStype_ERROR:      
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
            break;
    }
}


void compressAndSend() 
{
  String resultText = "";
  int previousValue = mlx90640To[0]*10;
  resultText.concat(previousValue);
  resultText.concat(".");

  for (int x = 1 ; x < 768; x += 1)
  {
    int currentValue = mlx90640To[x]*10;
    int diffIndex = abs((int)(currentValue - previousValue));
       diffIndex = diffIndex / 2;
       if(diffIndex<41){
        if(currentValue>previousValue)resultText.concat(positive[diffIndex]); else resultText.concat(negative[diffIndex]);
       } else {
         if(diffIndex>409)diffIndex=409;
         int count = diffIndex / 41;
         int newIndex = diffIndex - 41*count;
         resultText.concat(count-1);
         if(currentValue>previousValue)resultText.concat(positive[newIndex]); else resultText.concat(negative[newIndex]);
       }
    if(currentValue> previousValue) previousValue = previousValue + diffIndex*2; else previousValue = previousValue - diffIndex*2;
  } //end for
  webSocket.broadcastTXT(resultText);
}
