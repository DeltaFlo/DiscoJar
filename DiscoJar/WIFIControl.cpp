#include "WIFIControl.h"

// This code implements a simple web server that servers a static web page
// and accepts a POST with ArrayBuffer data.

// The current state
State _state;

State& getWebServerState()
{
  return _state;
}

// Port of the server, change to your needs:
#define PORT "80"

// some state for receiving POST data
// (our packets are small enough but this can handle
// POSTs bigger than a ESP packet size).
int missingPostData = 0;
int bufferPos = 0;
int postBufferLen = 0;
const int postBufferMax = 100;
unsigned char postBuffer[postBufferMax];

#define BUFFER_SIZE 1024
char buffer[BUFFER_SIZE];

void setupWiFi();
void readPost(int channelID, int packageBytes);
void serveHomePage(int channelID);

char OKString[] = "OK\r\n";

byte waitForResponse(int timeout, char* term=OKString) {
  unsigned long t=millis();
  bool found=false;
  int i=0;
  int len=strlen(term);
  // wait for at most timeout milliseconds
  // or if OK\r\n is found
  while(millis()<t+timeout) {
    if(Serial1.available()) {
      buffer[i++]=Serial1.read();
      if(i>=len) {
        if(strncmp(buffer+i-len, term, len)==0) {
          found=true;
          break;
        }
      }
    }
  }
  buffer[i]=0;
  #ifdef PRINT_DEBUG
  Serial.print(buffer);
  #endif
  return found;
}

void setupWebServer() {

  // assume esp8266 operates at 115200 baud rate
  // change if necessary to match your modules' baud rate
  Serial1.begin(115200);  // Teensy Hardware Serial port 1   (pins 0 and 1)
  #ifdef PRINT_DEBUG
  Serial.begin(115200);   // Teensy USB Serial Port
  #endif
  
  delay(5000);
  setupWiFi();

  // print device IP address
  #ifdef PRINT_DEBUG
  Serial.print("device ip addr: ");
  #endif
  Serial1.println("AT+CIFSR");
  waitForResponse(1000);
}

bool read_till_eol(int& bytesRead) {
  static int i=0;
  if(Serial1.available()) {
    buffer[i++]=Serial1.read();
    if(i==BUFFER_SIZE)  i=0;
    if((missingPostData>0 && buffer[i-1]==':') or (i>1 && buffer[i-2]==13 && buffer[i-1]==10)) {
      buffer[i]=0;
      bytesRead = i;
      i=0;
#ifdef PRINT_DEBUG
        Serial.print(buffer);
        #endif
      return true;
    }
  }
  return false;
}


void loopWebServer() {
  int channelID, packet_len;
  char *pb;
  int bytesRead;  
  if(read_till_eol(bytesRead)) {
    if(strncmp(buffer, "+IPD,", 5)==0) {
      // request: +IPD,ch,len:data
      sscanf(buffer+5, "%d,%d", &channelID, &packet_len);
      if (packet_len > 0) {
        // read serial until packet_len character received
        // start from :
        pb = buffer+5;
        while(*pb!=':') pb++;
        pb++;
        if (missingPostData>0) {
#ifdef PRINT_DEBUG
            Serial.println("-> read missing post");
            #endif
          readPost(channelID, packet_len);
          return;
        }

        if (strncmp(pb, "GET /", 5) == 0) {
          waitForResponse(1000);
#ifdef PRINT_DEBUG
            Serial.println("-> serve homepage");
            #endif
          serveHomePage(channelID);
        }
        if (strncmp(pb, "POST /", 6) == 0) {
#ifdef PRINT_DEBUG
            Serial.println("-> read post");
            #endif
          bufferPos = 0;
          missingPostData = 0;
          readPost(channelID, packet_len - (bytesRead - (pb - buffer)));
          return;
        }
      }
    }
  }
}



void serveHomePage(int channelID) {
  
  String header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n";

  String content;
content+="<html><head>                                                                                              \n";
content+="<title>DiscoJar</title>\n";
content+="<style type='text/css'>\n";
content+="form  { display: table;      }\n";
content+="p     { display: table-row;  }\n";
content+="label { display: table-cell; }\n";
content+="input { display: table-cell; }\n";
content+="</style>\n";
content+="<meta http-equiv='content-type' content='text/html; charset=windows-1252'><script type='text/javascript'> \n";
content+="                                                                                                          \n";
content+="function send() {                                                                                         \n";
content+="                                                                                                          \n";
content+="  var myArray = new ArrayBuffer(20);                                                                      \n";
content+="  var view = new Uint8Array(myArray);                                                                     \n";
content+="  var brightness = document.getElementById('brightness');                                                 \n";
content+="  mode = document.form.mode.value                                                                         \n";
content+="  color0 = document.form.color0.value                                                                     \n";
content+="  color1 = document.form.color1.value                                                                     \n";
content+="  view[0] = Number(mode);                                                                                 \n";
content+="  view[1] = brightness.value;                                                                             \n";
content+="  view[2] = parseInt(color0.substr(1,2),16);                                                              \n";
content+="  view[3] = parseInt(color0.substr(3,2),16);                                                              \n";
content+="  view[4] = parseInt(color0.substr(5,2),16);                                                              \n";
content+="  view[5] = parseInt(color1.substr(1,2),16);                                                              \n";
content+="  view[6] = parseInt(color1.substr(3,2),16);                                                              \n";
content+="  view[7] = parseInt(color1.substr(5,2),16);                                                              \n";
content+="  view[8] = 47;                                                                                           \n";
content+="  view[9] = 11;                                                                                           \n";
content+="  view = new DataView(myArray);                                                                           \n";
content+="  view.setFloat32(10, document.form.decay.value / 100., true);                                            \n";
content+="  view.setFloat32(14, document.form.gain.value , true);                                                   \n";
content+="                                                                                                          \n";
content+="  var xhr = new XMLHttpRequest;                                                                           \n";
content+="  xhr.open('POST', window.location.origin + '/', false);                                                  \n";
content+="  xhr.send(myArray);                                                                                      \n";
content+="}                                                                                                         \n";
content+="                                                                                                          \n";
content+="function init() {                                                                                         \n";
content+="}                                                                                                         \n";
content+="</script>                                                                                                 \n";
content+="</head><body onload='init()'>                                                                             \n";
content+="<h2>DiscoJar Remote Control</h2>                                                                          \n";
content+="<div class='container'>                                                                                   \n";
content+="<form name='form' onsubmit='return false'>                                                                \n";
content+="    <p><label>Brightness:</label> <input type='range' id='brightness' name='brightness' min='0' max='255' value='32'></p>   \n";
content+="    <p><label>Decay:</label> <input type='range' id='decay' name='decay' min='1' max='100' value='40'></p>                \n";
content+="    <p><label>Gain:</label> <input type='range' id='gain' name='gain' min='200' max='600' value='320'></p>                \n";
content+="    <p> </p><p><label>Mode:</label></p>                                                                    \n";
content+="  <p><input type='radio' name='mode' value='0' checked>Spectrum with rainbow</input></p>                   \n";
content+="    <p><input type='radio' name='mode' value='1'>Spectrum with plasma</input></p>                          \n";
content+="    <p><input type='radio' name='mode' value='2'>Plasma</input></p>                                        \n";
content+="    <p><input type='radio' name='mode' value='3'>Confetti</input></p>                                      \n";
content+="    <p><input type='radio' name='mode' value='4'>Gradient</input></p>                                      \n";
content+="    <p><input type='color' name='color0' value='#ff0000'>                                                  \n";
content+="       <input type='color' name='color1' value='#0000ff'></p>                                              \n";
content+="    <p><input value='Send' onclick='send()' type='button'></p>                                             \n";
content+="</form>                                                                                                    \n";
content+="</div>                                                                                                     \n";
content+="</body></html>                                                                                             \n";                     

  content += "\r\n";       

  header += "Content-Length:";
  header += (int)(content.length());
  header += "\r\n\r\n";
  header += content;
  int pos = 0;
  int fullLen = header.length();
  while (fullLen>0) {
    int curLen = fullLen;
    if (curLen>1024) {
      curLen = 1024;
    }
    fullLen-=curLen;
    Serial1.print("AT+CIPSEND=");
    Serial1.print(channelID);
    Serial1.print(",");
    Serial1.println(curLen);
    // we always get: OK > data SEND_OK
    if(waitForResponse(2000)) {
      // user serial write in a loop...
      for (int i = 0;i<curLen;i++) {
        Serial1.write(header[pos++]);
      }
      waitForResponse(2000, "SEND OK\r\n");
    } 
     else {
      Serial1.print("AT+CIPCLOSE=");
    Serial1.println(channelID);
    }
  } 
}

void readPost(int channelID, int packageBytes) {
  
  int len = missingPostData;
  if (missingPostData==0) {
  do {
    int bytesRead;
    while (!read_till_eol(bytesRead)) {
    }
    packageBytes -= bytesRead;
    if(strncmp(buffer, "Content-Length: ", 16)==0) {
      sscanf(buffer+16, "%d", &len);
    }
    if (*buffer==13) {
      // first empty line, data starts...
      break;
    }
  } while (1);

  }
  int p = len;
  if (packageBytes<len)  {
    p = packageBytes;
  }
  len -= p;
  int i;
  if (p > postBufferMax) p = postBufferMax;
  for (i = bufferPos;i<p;i++) {
    while (!Serial1.available());
    postBuffer[i] = Serial1.read();
    packageBytes--;
  }
  if (len > 0) {
#ifdef PRINT_DEBUG
          Serial.println("left over");
        Serial.println(len);
        #endif
    missingPostData = len;
    bufferPos = i;
    return; 
  } else {
    missingPostData = 0;
    bufferPos = 0;
#ifdef PRINT_DEBUG
      Serial.println("all received");
      #endif
    postBufferLen = i;
  }
         
  /*unsigned short* d = (unsigned short*)postBuffer;
  for (int i = 0;i<totalLen/2;i++) {
    Serial.println(*d++);
  }*/
  //Serial.println(postBuffer[0]);
  //Serial.println(postBuffer[1]);
  _state.mode = postBuffer[0];
  _state.brightness = postBuffer[1];
  _state.color0[0] = postBuffer[2];
  _state.color0[1] = postBuffer[3];
  _state.color0[2] = postBuffer[4];
  _state.color1[0] = postBuffer[5];
  _state.color1[1] = postBuffer[6];
  _state.color1[2] = postBuffer[7];
  _state.param0 = postBuffer[8];
  _state.param1 = postBuffer[9];
  _state.decay  = *((float*)(postBuffer + 10));
  _state.gain  = *((float*)(postBuffer + 14));
  
  String header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n";
  String content="";
  header += "Content-Length:";
  header += (int)(content.length());
  header += "\r\n\r\n";
  Serial1.print("AT+CIPSEND=");
  Serial1.print(channelID);
  Serial1.print(",");
  Serial1.println(header.length()+content.length());
  if(waitForResponse(2000)) {
   Serial1.print(header);
   Serial1.print(content);
   waitForResponse(2000, "SEND OK\r\n");
  } 
  else {
  Serial1.print("AT+CIPCLOSE=");
  Serial1.println(channelID);
 }
}

void setupWiFi() {

  // turn on echo
  Serial1.println("ATE1");
  waitForResponse(1000);
  
  // try empty AT command
  Serial1.println("AT");
  waitForResponse(1000);
  
  // start server
  Serial1.println("AT+CIPMUX=1");
  waitForResponse(1000);
  
  // create TCP server
  Serial1.print("AT+CIPSERVER=1,");
  Serial1.println(PORT);
  waitForResponse(1000);
  
  Serial1.println("AT+CIPSTO=30");  
  waitForResponse(1000);

  Serial1.println("AT+GMR");
  waitForResponse(1000);
  
  Serial1.println("AT+CWJAP?");
  waitForResponse(1000);
  
  Serial1.println("AT+CIPSTA?");
  waitForResponse(1000);
  
  Serial1.println("AT+CWMODE?");
  waitForResponse(1000);
  
  Serial1.println("AT+CIFSR");
  waitForResponse(5000);
  
  Serial1.println("AT+CWLAP");
  waitForResponse(5000);
  
#ifdef PRINT_DEBUG
    Serial.println("---- Web Server running ----");
#endif  
}


