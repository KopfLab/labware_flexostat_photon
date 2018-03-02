// math
#include "math.h"
#include "Display.h"
// display
Display lcd (0x3f, 16, 2, 0); // 20x4 display, don't reserve anything for messages

// web logger
const char* webhook = "kopflab_eq";
char date_time_buffer[20];
char value_buffer[30];
char value_sd_buffer[30];
char ref_buffer[30];
char ref_sd_buffer[30];
char json_buffer[255];

// send to google spreadsheet
bool send_to_gs(const char* type, const char* variable, const char* value, const char* value_sd, const char* ref, const char* ref_sd, const char* unit, const char* msg) {
  Time.format(Time.now(), "%Y-%m-%d %H:%M:%S").toCharArray(date_time_buffer, sizeof(date_time_buffer));
  snprintf(json_buffer, sizeof(json_buffer),
    "{\"datetime\":\"%s\",\"type\":\"%s\",\"var\":\"%s\",\"value\":\"%s\",\"value_sd\":\"%s\",\"ref\":\"%s\",\"ref_sd\":\"%s\", \"units\":\"%s\",\"msg\":\"%s\"}",
    date_time_buffer, type, variable, value, value_sd, ref, ref_sd, unit, msg);
  return(Particle.publish(webhook, json_buffer));
}

// send generic text data
bool send_to_gs(const char* type, const char* variable, const char* value, const char* unit, const char* msg) {
  return(send_to_gs(type, variable, value, "", "", "", unit, msg));
}

// send generic integer data
bool send_to_gs(const char* type, const char* variable, int value, const char* unit, const char* msg) {
  sprintf(value_buffer, "%d", value);
  return(send_to_gs(type, variable, value_buffer, unit, msg));
}

// send numeric data (allow modification of decimal point?)
bool send_to_gs(const char* type, const char* variable, double value, double value_sd, double ref, double ref_sd, const char* unit, const char* msg) {
  sprintf(value_buffer, "%.1f", value);
  sprintf(ref_buffer, "%.1f", ref);
  sprintf(value_sd_buffer, "%.1f", value_sd);
  sprintf(ref_sd_buffer, "%.1f", ref_sd);
  return(send_to_gs(type, variable, value_buffer, value_sd_buffer, ref_buffer, ref_sd_buffer, unit, msg));
}

// indicator LED
const int led = D7;

// clock sync
#define ONE_DAY_MILLIS (24 * 60 * 60 * 1000)
unsigned long last_sync = millis();

// parameters for checking on flexostat
const int RECORD_INTERVAL = (1000*30); // record every 30 seconds
unsigned long last_record = millis();

const int AVG_N_READINGS = 6;
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
bool waiting_for_reply = FALSE;

// device name
char device_name[20];
void info_handler(const char *topic, const char *data) {
    Serial.println("Device name: " + String(data));
    strncpy ( device_name, data, sizeof(device_name) );
    lcd.print_line(1, device_name);
    Particle.unsubscribe();
}

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

    // LCD screen
    lcd.init();
    lcd.print_line(1, "Starting up...");


    // info
    const int gs_delay = 3000; // allow 3s for message to register
    Serial.println("INFO: Initializing and logging in google spreadsheet.");
    send_to_gs("event", "startup", "", "", "complete");
    delay(gs_delay);
    send_to_gs("information", "record interval", RECORD_INTERVAL/(1000*60), "min", "time between OD recordings");
    delay(gs_delay);
    send_to_gs("information", "read interval", READ_INTERVAL/(1000), "s", "time between OD readings");
    delay(gs_delay);
    send_to_gs("information", "averaging number", AVG_N_READINGS, "#", "number of readings averaged per record");

    // device name
    Particle.subscribe("spark/", info_handler);
    Particle.publish("spark/device/name");

    // start up complete
    Serial.println("INFO: startup complete");
}

// OD object
int byte_num = 0;
rawOD rod;
char lcd_buffer[20];

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
    if (!waiting_for_reply && millis() - last_read > READ_INTERVAL) {
      Serial.print("INFO: checking OD at ms ");
      Serial.println(millis());

      // reset value
      byte_num = 0;
      while(byte_num<sizeof(rawOD)) {
        rod.bytes[byte_num] = 0;
        byte_num++;
      }
      byte_num = 0;

      // empty buffer
      while(Serial1.available()) {
        Serial1.read();
      }

      // issue command
      digitalWrite(led, HIGH);
      last_command = millis();
      Serial1.print("."); // send anything
      last_read = millis();
      waiting_for_reply = true;
    }

    if (waiting_for_reply) {
      //now wait to get bytes for raw OD back
      while(Serial1.available()) {
          byte c = Serial1.read();
          if (c != 255) { // error return seems to be byte 255
            rod.bytes[byte_num] = c;
            byte_num++;
          }
      }

      // check for timeout
      if ((millis() - last_command)>READ_TIMEOUT) {
        Serial.print("ERROR: timeout at ");
        Serial.println(millis());
        waiting_for_reply = false;
      } else if (byte_num == 2*sizeof(uint32_t)) {
        sprintf (lcd_buffer, "LAST: %.4f", ((double) rod.values[1])/((double) rod.values[0]));
        lcd.print_line(1, lcd_buffer);
        Serial.print("GOT VALUE: ref = " + String(rod.values[0]));
        Serial.print(" / read = " + String(rod.values[1]));
        Serial.println(" ratio: " + String(lcd_buffer));

        // store values
        OD_values[index_counter] = rod.values[1];
        REF_values[index_counter] = rod.values[0];
        index_counter = (index_counter + 1) % AVG_N_READINGS;
        if (index_counter == 0) {
            // one full set of readings taken
            ready_to_record = true;
        }
        waiting_for_reply = false;
      }
      digitalWrite(led, LOW);
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
      sprintf (lcd_buffer, "AVG: %.4f", OD_avg/REF_avg);
      lcd.print_line(2, lcd_buffer);
      Serial.print("ref = " + String((int) REF_avg) + " +- " + String((int) REF_sdev));
      Serial.print(" / read = " + String((int) OD_avg) + " +- " + String((int) OD_sdev));
      Serial.println(" ratio: " + String(lcd_buffer));
      send_to_gs("data", "OD", OD_avg, OD_sdev, REF_avg, REF_sdev, "", "");

      // update timing counter
      last_record = millis();
    }

}
