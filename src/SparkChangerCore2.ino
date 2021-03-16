#include <M5Core2.h>
#include "SparkPresets.h"
#include "SparkIO.h"

#define BACKGROUND TFT_BLACK
#define TEXT_COLOUR TFT_WHITE

#define MIDI   68
#define STATUS 34
#define IN     102
#define OUT    172

#define STD_HEIGHT 30
#define PANEL_HEIGHT 68

SparkIO sp;
SparkPreset preset;
SparkMessage msg;

// ------------------------------------------------------------------------------------------
// Display routintes

#define DISP_LEN 50

char outstr[DISP_LEN];
char instr[DISP_LEN];
char statstr[DISP_LEN];

int bar_pos;
int bar_dir;
unsigned long bar_count;

void display_background(const char *title, int y, int height)
{
   int x_pos;

   x_pos = 160 - 3 * strlen(title);
   M5.Lcd.drawRoundRect(0, y, 320, height, 4, TEXT_COLOUR);
   M5.Lcd.setCursor(x_pos, y);
   M5.Lcd.print(title);
   M5.Lcd.setCursor(8,y+8);
 
}

void do_backgrounds()
{
   M5.Lcd.setTextColor(TEXT_COLOUR, BACKGROUND);
   M5.Lcd.setTextSize(1);
   display_background("", 0, STD_HEIGHT);
   display_background(" STATUS ",    STATUS, PANEL_HEIGHT);
   display_background(" RECEIVED ",  IN,     PANEL_HEIGHT);
   display_background(" SENT ",      OUT,    PANEL_HEIGHT);         
   M5.Lcd.setTextSize(2); 
   M5.Lcd.setCursor (45, 8);
   M5.Lcd.print("Spark Changer Core 2");
   bar_pos=0;
   bar_dir = 1;
   bar_count = millis();
}

void display_bar()
{
   if (millis() - bar_count > 50) {
      bar_count = millis();
      M5.Lcd.fillRoundRect(15 + bar_pos*6, STATUS + STD_HEIGHT + 10 , 15, 15, 4, BACKGROUND);
      bar_pos += bar_dir;
      if (bar_dir == 1  && bar_pos == 45) bar_dir = -1;
      if (bar_dir == -1 && bar_pos == 0)  bar_dir = 1;
      M5.Lcd.fillRoundRect(15 + bar_pos*6, STATUS + STD_HEIGHT + 10, 15, 15, 4, TEXT_COLOUR);
   }
}

void display_val(float val)
{
   int dist;

   dist = int(val * 290);
   M5.Lcd.fillRoundRect(15 + dist, IN + STD_HEIGHT + 10 , 290 - dist, 15, 4, BACKGROUND);
   M5.Lcd.drawRoundRect(15, IN + STD_HEIGHT + 10 , 290 , 15, 4, TEXT_COLOUR);
   M5.Lcd.fillRoundRect(15, IN + STD_HEIGHT + 10 , dist, 15, 4, TEXT_COLOUR);
}

void display_str(const char *a_str, int y)
{
   char b_str[30];

   strncpy(b_str, a_str, 25);
   if (strlen(a_str) < 25) strncat(b_str, "                         ", 25-strlen(a_str));
   M5.Lcd.setCursor(8,y+8);
   M5.Lcd.print(b_str);
}



// ------------------------------------------------------------------------------------------

int i, j, p;
int pres;

int cmd, sub_cmd;
char a_str[STR_LEN];
char b_str[STR_LEN];
int param;
float val;

unsigned long keep_alive;

void setup() {
  M5.begin();

  M5.Lcd.fillScreen(BACKGROUND);
  do_backgrounds();

  display_str("Started",      STATUS);
  display_str("Nothing out",  OUT);
  display_str("Nothing in",   IN);

  sp.start_bt();
  sp.connect_to_spark();

  // If you start the amp ater the app it takes time to be able to process messages properly - 7s delay seems to work
  for (i=7; i>0; i--) {
    snprintf(statstr, DISP_LEN, "Connected - waiting %ds", i);
    display_str(statstr, STATUS);
    delay(1000);
  }
  display_str("Active", STATUS);
  keep_alive = millis();
  
// TESTS 
/*
  sp.change_hardware_preset(0x7f);
  sp.process_out_chunks();
  sp.process_out_blocks(); 

  sp.create_preset(&preset0);
  sp.process_out_chunks();
  sp.process_out_blocks();
  while (!sp.ob_ok_to_send) {
    sp.process();
    }
  sp.process_out_blocks();
  while (!sp.ob_ok_to_send) {
    sp.process();
    }
  sp.process_out_blocks();
  while (!sp.ob_ok_to_send) {
    sp.process();
    }
  sp.process_out_blocks();
  while (!sp.ob_ok_to_send) {
    sp.process();
  }
  sp.process_out_blocks();
  while (!sp.ob_ok_to_send) {
    sp.process();
    }
  sp.change_hardware_preset(0x7f);
  sp.process_out_chunks();
  sp.process_out_blocks(); 
/*  
  for (val=0.0; val < 0.9; val+=0.05) {
    sp.change_effect_parameter("RolandJC120", 0, val);
    sp.process_out_chunks();
    sp.process_out_blocks();     
  }
*/

//  sp.change_hardware_preset(0x7f);
  sp.create_preset(&preset0);
  Serial.println("Tests queued");

// TESTS END
}

/*
uint8_t get_a_preset[]{0x01,0xfe,0x00,0x00,0x53,0xfe,0x3c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                       0xf0,0x01,0x06,0x02,0x02,0x01,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf7};
*/
                   
void loop() {
  int av;
  int ret;
  int ct;
  uint8_t b;
  int i, j;
  unsigned int cmdsub;
   
  display_bar();

  // Keeps the connection alive

  if (millis() - keep_alive  > 10000) {
    Serial.println("Keep alive");
    keep_alive = millis();
    sp.get_serial();
    // sp.bt.write(get_a_preset, sizeof(get_a_preset));
  }
    
  delay(10);

  sp.process();

  if (sp.get_message(&cmdsub, &msg, &preset)) { //there is something there
    keep_alive = millis();    
  
    switch (cmdsub) {
      case 0x0301: // get a preset full details
        /*
        snprintf(instr, DISP_LEN, "Preset: %-.20s", preset.Name);
        display_str(instr, IN);
        
        // Dump preset to Serial if you want to - just to prove it works
        Serial.println(preset.Name);
        for (j=0; j<7; j++) {
          Serial.print(preset.effects[j].EffectName);
          Serial.print(" ");
          Serial.print(preset.effects[j].OnOff);
          Serial.print(" ");
          Serial.print(preset.effects[j].NumParameters);
          for (i = 0; i < preset.effects[j].NumParameters; i++) {
            Serial.print(" ");
            Serial.print(preset.effects[j].Parameters[i]);
          };
          Serial.println();
        }
        */ 
        break;
      case 0x0323: // get serial number
        // snprintf(instr, DISP_LEN, "-> %-.20s", msg.str1);
        // display_str(instr, IN);   
        break; 
      case 0x0306: // get amp change
        snprintf(instr, DISP_LEN, "-> %-.20s", msg.str2);
        display_str(instr, IN);
        if      (!strcmp(msg.str2, "FatAcousticV2"))    pres = 16;
        else if (!strcmp(msg.str2, "GK800"))            pres = 17;
        else if (!strcmp(msg.str2, "Twin"))             pres = 3;
        else if (!strcmp(msg.str2, "TwoStoneSP50"))     pres = 12;
        else if (!strcmp(msg.str2, "OverDrivenJM45"))   pres = 5; 
        else if (!strcmp(msg.str2, "AmericanHighGain")) pres = 22;
        else if (!strcmp(msg.str2, "EVH"))              pres = 7;
        
        // queue the commands
        sp.create_preset(presets[pres]);
//        sp.process_out_chunks();  // TEMP
//        sp.process_out_blocks();  // TEMP
        sp.change_hardware_preset(0x7f);
//        sp.process_out_chunks();  // TEMP
//        sp.process_out_blocks();  // TEMP 
        
        snprintf(outstr, DISP_LEN, "Preset: %-.20s", presets[pres]->Name);
        display_str(outstr, OUT);
        break;
      case 0x0337: // get parameter change
        snprintf(instr, DISP_LEN, "%-.20s %0.1d %0.2f", msg.str1, msg.param1, msg.val);
        display_str(instr, IN);  
        display_val(msg.val);
        break;
      default:
        break;
    }
  }
}
