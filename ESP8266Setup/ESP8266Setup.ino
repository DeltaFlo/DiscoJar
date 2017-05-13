
void setup() {
  
    // Setup computer to Teensy serial
    Serial.begin(115200);

    delay(5000);

    // Setup Teensy to ESP8266 serial
    Serial1.begin(115200);

}

void loop() {

    // Send bytes from ESP8266 -> Teensy to Computer
    if ( Serial1.available() ) {
        Serial.write( Serial1.read() );
    }

    // Send bytes from Computer -> Teensy back to ESP8266
    if ( Serial.available() ) {
        Serial1.write( Serial.read() );
    }

}
