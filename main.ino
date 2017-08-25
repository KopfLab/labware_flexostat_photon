// math
#include "math.h"

// web logger
#include "GsWebhook.h"
GsWebhook gs ("kopflab_eq");
const char* webhook = "kopflab_eq";

// indicator LED
const int led = D7;

// clock sync
#define ONE_DAY_MILLIS (24 * 60 * 60 * 1000)
unsigned long last_sync = millis();

// parameters for checking on flexostat
const int RECORD_INTERVAL = (5*1000*60); // record every 5 minutes
unsigned long last_record = millis();

const int AVG_N_READINGS = 20;
const int READ_INTERVAL = RECORD_INTERVAL/AVG_N_READINGS;
unsigned long last_read = millis();

const int READ_TIMEOUT = 500;
unsigned long last_command = millis();

// data types and arrays
// OD data definition
typedef union __rodd
{
    uint8_t bytes[2*sizeof(uint32_t)];
    uint32_t values[2];
} rawOD;

uint32_t OD_values[AVG_N_READINGS];
uint32_t REF_values[AVG_N_READINGS];
int index_counter = 0; // keeping track which index to record in
bool ready_to_record = FALSE; // make sure at least one full set of readings was taken before calculting the first average


// setup function
void setup() {

    // initialize serial communications for loggin
    Serial.begin(9600);

    // Serial communication with OD board
    Serial1.begin(19200, SERIAL_8N1);

    // time zone setup
    Time.zone(-6); // Mountain time (daylight savings?)

    // pin setup
    pinMode(led, OUTPUT);
    digitalWrite(led, LOW);

    // info
    const int gs_delay = 3000; // allow 3s for message to register
    Serial.println("INFO: Initializing and logging in google spreadsheet.");
    gs.init();
    delay(gs_delay);
    gs.send("information", "record interval", RECORD_INTERVAL/(1000*60), "min", "time between OD recordings");
    delay(gs_delay);
    gs.send("information", "read interval", READ_INTERVAL/(1000), "s", "time between OD readings");
    delay(gs_delay);
    gs.send("information", "averaging number", AVG_N_READINGS, "#", "number of readings averaged per record");

    // start up complete
    Serial.println("INFO: startup complete");
}


// loop function
void loop() {

     // Time sync (once a day)
    if (millis() - last_sync > ONE_DAY_MILLIS) {
        // Request time synchronization from the Particle Cloud
        Particle.syncTime();
        last_sync = millis();
        //gs.send("event", "time sync", "", "", "complete");
    }

    // check OD
    if (millis() - last_read > READ_INTERVAL) {
      Serial.print("INFO: checking OD at ms ");
      Serial.println(millis());

      // OD object
      rawOD rod;

      // empty buffer
      while(Serial1.available()) {
        Serial1.read();
      }

      // issue command
      digitalWrite(led, HIGH);
      last_command = millis();
      Serial1.print(";"); // send anything

      //now wait to get bytes for raw OD back
      int byte_num = 0;
      while(byte_num<sizeof(rawOD) && (millis()-last_command)<READ_TIMEOUT ) {
          while(!Serial1.available() && (millis()-last_command)<READ_TIMEOUT);
          byte c = Serial1.read();
          rod.bytes[byte_num] = c;
          byte_num++;
      }

      // check for timeout
      if ((millis() - last_command)>READ_TIMEOUT) {
        Serial.print("ERROR: timeout at ");
        Serial.println(millis());
      } else {
        Serial.print("VALUE: ref = " + String(rod.values[0]));
        Serial.println(" / read = " + String(rod.values[1]));

        // store values
        OD_values[index_counter] = rod.values[1];
        REF_values[index_counter] = rod.values[0];
        index_counter = (index_counter + 1) % AVG_N_READINGS;
        if (index_counter == 0) {
            // one full set of readings taken
            ready_to_record = true;
        }
      }
      digitalWrite(led, LOW);
      last_read = millis();
    }

    // record averaged OD & ref values
    if ( ready_to_record && millis() > (last_record + RECORD_INTERVAL) ) {

      // average
      double OD_sum = 0;
      double REF_sum = 0;
      for (int i = 0 ; i < AVG_N_READINGS ; i++) {
          OD_sum += OD_values[i];
          REF_sum += REF_values[i];
      }
      double OD_avg = OD_sum / ((double) AVG_N_READINGS);
      double REF_avg = REF_sum / ((double) AVG_N_READINGS);

      // standard deviation
      double OD_ss = 0;
      double REF_ss = 0;
      for (int i = 0 ; i < AVG_N_READINGS ; i++) {
          OD_ss += pow(OD_values[i] - OD_avg, 2);
          REF_ss += pow(REF_values[i] - REF_avg, 2);
      }
      double OD_sdev = sqrt(OD_ss / ((double) AVG_N_READINGS - 1));
      double REF_sdev = sqrt(REF_ss / ((double) AVG_N_READINGS - 1));

      Serial.print("RECORDING AVERAGES: ");
      Serial.print("ref = " + String((int) REF_avg) + " +- " + String((int) REF_sdev));
      Serial.print(" / read = " + String((int) OD_avg) + " +- " + String((int) OD_sdev));
      gs.send("data", "OD", OD_avg, REF_avg, "", "");

      // update timing counter
      last_record = millis();
    }

}
