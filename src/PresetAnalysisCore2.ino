#include "BluetoothSerial.h"
#include <M5Core2.h>

#include "Spark.h"
#include "SparkIO.h"
#include "SparkComms.h"

SparkIO spark_io(false); // do NOT do passthru as only one device here, no serial to the app
SparkComms spark_comms;


unsigned int cmdsub;
SparkMessage msg;
SparkPreset preset;
SparkPreset presets[6];

unsigned long last_millis;
int my_state;
int scr_line;
char str[50];

SparkPreset preset0{0x0,0x7f,
  "07079063-94A9-41B1-AB1D-02CBC5D00790","Silver Ship","0.7","1-Clean","icon.png",120.000000,{ 
  {"bias.noisegate", false, 3, {0.138313, 0.224643, 0.000000}}, 
  {"LA2AComp", true, 3, {0.000000, 0.852394, 0.373072}}, 
  {"Booster", false, 1, {0.722592}}, 
  {"RolandJC120", true, 5, {0.632231, 0.281820, 0.158359, 0.671320, 0.805785}}, 
  {"Cloner", true, 2, {0.199593, 0.000000}}, 
  {"VintageDelay", false, 4, {0.378739, 0.425745, 0.419816, 1.000000}}, 
  {"bias.reverb", true, 7, {0.285714, 0.408354, 0.289489, 0.388317, 0.582143, 0.650000, 0.200000}} }, 
  0xb4 };
                                                               
SparkPreset preset1{0x01,0x01,
  "07079063-94A9-41B1-AB1D-02CBC5D00790","Wilver Whip","0.7","1-Clean","icon.png",120.000000,{ 
  {"bias.noisegate", false, 3, {0.138313, 0.224643, 0.000000}}, 
  {"LA2AComp", true, 3, {0.000000, 0.852394, 0.373072}}, 
  {"Booster", false, 1, {0.722592}}, 
  {"RolandJC120", true, 5, {0.632231, 0.281820, 0.158359, 0.671320, 0.805785}}, 
  {"Cloner", true, 2, {0.199593, 0.000000}}, 
  {"VintageDelay", false, 4, {0.378739, 0.425745, 0.419816, 1.000000}}, 
  {"bias.reverb", true, 7, {0.285714, 0.408354, 0.289489, 0.388317, 0.582143, 0.650000, 0.200000}} }, 
  0xb4 };

  /*
SparkPreset preset1{0x0,0x00,
  "97979963-94A9-41B1-AB1D-02CBC5D00790","Sjlver Ship","0.7","1-Clean","icon.png",120.000000,{ 
  {"bias.noisegate", true, 3, {0.138313, 0.224643, 0.000000}}, 
  {"LA2AComp", true, 3, {0.000000, 0.852394, 0.373072}}, 
  {"Booster", true, 1, {0.722592}}, 
  {"RolandJC120", true, 5, {0.632231, 0.281820, 0.158359, 0.671320, 0.805785}}, 
  {"Cloner", true, 2, {0.199593, 0.000000}}, 
  {"VintageDelay", false, 4, {0.378739, 0.425745, 0.419816, 1.000000}}, 
  {"bias.reverb", true, 7, {0.285714, 0.408354, 0.289489, 0.388317, 0.582143, 0.650000, 0.200000}} }, 
  0xb4 };
*/

void printit(char *str) {
  if (scr_line >= 8) {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("Preset Analyser");
    M5.Lcd.println();
    scr_line = 1;
  }
  M5.Lcd.println(str);
  scr_line++;
}

void dump_preset(SparkPreset preset) {
  int i,j;

  Serial.print(preset.curr_preset); Serial.print(" ");
  Serial.print(preset.preset_num); Serial.print(" ");
  Serial.print(preset.Name); Serial.print(" ");

  Serial.println(preset.Description);

  for (j=0; j<7; j++) {
    Serial.print("    ");
    Serial.print(preset.effects[j].EffectName); Serial.print(" ");
    if (preset.effects[j].OnOff == true) Serial.print(" On "); else Serial.print (" Off ");
    for (i = 0; i < preset.effects[j].NumParameters; i++) {
      Serial.print(preset.effects[j].Parameters[i]); Serial.print(" ");
    }
    Serial.println();
  }
  Serial.println(preset.end_filler);
  Serial.println();
}


void setup() {
  M5.begin();
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextSize(2);
  scr_line = 1;
  
  printit("Alive");
  Serial.println("Alive");

  spark_io.comms = &spark_comms;
  spark_comms.start_bt();
  spark_comms.connect_to_spark();

  printit("Connected");
  last_millis = millis();
  my_state = 0;
  delay(2000);
  printit("Starting");
}

  
void loop() {
  spark_io.process();
  
  if (spark_io.get_message(&cmdsub, &msg, &preset)) { 
    sprintf(str, "< %4.4x", cmdsub);
    printit(str);
    
    if (cmdsub == 0x0301) { // got a response to a 0x0201 preset info request
      dump_preset(preset);
    }
  }

  if (millis() - last_millis > 1000) {

    last_millis = millis();
  
    switch (my_state) {
      case 0:
        spark_io.change_hardware_preset(0x00); 
        break;
      case 1: 
        spark_io.create_preset(&preset0); 
        spark_io.change_hardware_preset(0x7f);
        break;
      case 2:
        spark_io.get_preset_details(0x007f);
        break;
      case 3: 
        spark_io.create_preset(&preset1); 
        spark_io.change_hardware_preset(0x03); // *************
        break;
      case 4:
        Serial.println("007f");
        spark_io.get_preset_details(0x0001);
      default:
        Serial.println (">>>>");
        spark_io.get_preset_details(0x0101);
        break;       
    }

    my_state++;
  }
}
