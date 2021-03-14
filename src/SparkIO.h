#ifndef SparkIO_h
#define SparkIO_h

#include "BluetoothSerial.h"

// Bluetooth vars
#define  SPARK_NAME  "Spark 40 Audio"
#define  MY_NAME     "SparkChanger"

#define STR_LEN 40
//#define CHUNK_BUF_LEN 1000

class RingBuffer
{
  public:
    RingBuffer();
    bool add(uint8_t b);
    bool get(uint8_t *b);
    bool set_at_index(int ind, uint8_t b);
    bool get_at_index(int ind, uint8_t *b);
    int  get_len();
    bool is_empty();
    void commit();
    void drop();
    void clear();
    void dump();
    void dump2();
  private:
    static const int RB_BUFF_MAX = 5000;
    uint8_t rb[RB_BUFF_MAX];
    int st, en, len, t_len;
 };


typedef struct  {
  int  start_filler;
  int  preset_num;
  char UUID[STR_LEN];
  char Name[STR_LEN];
  char Version[STR_LEN];
  char Description[STR_LEN];
  char Icon[STR_LEN+1];
  float BPM;
  struct SparkEffects {
    char EffectName[STR_LEN];
    bool OnOff;
    int  NumParameters;
    float Parameters[10];
  } effects[7];
  uint8_t end_filler;
} SparkPreset;

typedef struct {
  int param1;
  int param2;
  int param3;
  int param4;
  float val;
  char str1[STR_LEN];
  char str2[STR_LEN];
  bool onoff;
} SparkMessage;


class SparkIO
{
  public:
    SparkIO();
    ~SparkIO();

    // bluetooth communictions

    void start_bt();
    void connect_to_spark();
    BluetoothSerial bt;

    // overall processing

    void process();

    //  receiving data   
    
    void process_in_blocks();
    void process_in_chunks();
    void process_in_message();

    // processing received messages    
    bool get_message(unsigned int *cmdsub, SparkMessage *msg, SparkPreset *preset);
    // creating messages to send

    void create_preset(SparkPreset *preset);
    void get_serial();
    void turn_effect_onoff(char *pedal, bool onoff);
    void change_hardware_preset(int preset_num);
    void change_effect(char *pedal1, char *pedal2);
    void change_effect_parameter(char *pedal, int param, float val);
    
    // sending data



    // chunk variables (read from bluetooth into a chunk ring buffer)
    // in_chunk.is_empty() is false when there is data to read

    RingBuffer in_chunk;
    int rb_state;
    int rb_len;

    // message variables (read from chunk read buffer into in_message store - a single message
    // in_mesage_ready is true when there is a full message to read

    RingBuffer in_message;
    int rc_state;
    bool in_message_bad;
    
    int rc_seq;
    int rc_cmd;
    int rc_sub;
    int rc_checksum;
    
    int rc_calc_checksum;

    bool rc_multi_chunk;
    int rc_data_pos;
    uint8_t rc_bitmask;
    int rc_bits;

    int rc_total_chunks;
    int rc_this_chunk;
    int rc_chunk_len;
    int rc_last_chunk;

    int rd_pos;
    
    // message variables for sending
    
    RingBuffer out_message;
    int om_cmd;
    int om_sub;
    
  private:
    // routines to read the msgpack data
    void read_string(char *str);
    void read_prefixed_string(char *str);
    void read_onoff(bool *b);
    void read_float(float *f);
    void read_byte(int *b);

    // routines to write the msgfmt data
    void start_message(int cmdsub);
    void end_message();
    void write_byte(byte b);
    void write_prefixed_string(const char *str);
    void write_long_string(const char *str);
    void write_string(const char *str);
    void write_float(float flt);
    void write_onoff(bool onoff);

};





#endif
      
