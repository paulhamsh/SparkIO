#include "SparkIO.h"
#define DEBUG(x) Serial.println(x)

//
// RingBuffer class
//

/* Implementation of a ring buffer - with a difference
 * New data is written to a temp area with add_to_buffer() and committed into the buffer once done - commit()
 * Commited data is read from the head of the buffer with get_from_buffer()
 * Data in this area can be read and updated by index - get_at_index() and set_at_index()
 * 
 * If the temp data is not required it can be ignored using drop() rather than commit()
 * 
 *    +----------------------------------++----------------------------------------------------------------------+
 *    |  0 |  1 |  2 |  3 |  4 |  5 |  6 ||  7 |  8 |  9 | 10 | 11 | 12 | 13 || 14 | 15 | 16 | 17 | 18 | 19 | 20 |                                              |
 *    +----------------------------------++----------------------------------------------------------------------+  
 *       ^                             ^     ^                             ^
 *       st ---------- len ------------+     +----------- t_len --------- en
 * 
 *           committed data                      temporary data                                   empty
 */

RingBuffer::RingBuffer() {
  st = 0;
  en = 0;
  len = 0;
  t_len = 0;
}

bool RingBuffer::add(uint8_t b) {
  if (len + t_len < RB_BUFF_MAX) {
    rb[en] = b;
    t_len++;
    en++; 
    if (en >= RB_BUFF_MAX) en = 0; 
    return true;
  }
  else
    return false;
}

bool RingBuffer::get(uint8_t *b) {
  if (len > 0) {
    *b = rb[st];
    len--;
    st++; 
    if (st >= RB_BUFF_MAX) st = 0; 
    return true;
  }
  else
    return false;  
}

// set a value at a location in the temp area
bool RingBuffer::set_at_index(int ind, uint8_t b) {
  if (ind >= 0 && ind < t_len) {
    rb[(st+len+ind) % RB_BUFF_MAX] = b;
    return true;
  }
  else
    return false;
}

// get a value from a location in the temp area
bool RingBuffer::get_at_index(int ind, uint8_t *b) {
  if (ind >= 0 && ind < t_len) {
    *b = rb[(st+len+ind) % RB_BUFF_MAX];
    return true;
  }
  else
    return false;
}

int RingBuffer::get_len() {
  return t_len;
}

void RingBuffer::commit() {
  len += t_len;
  t_len = 0;
}

void RingBuffer::drop() {
  en = st + len;
  t_len = 0;
}

void RingBuffer::clear() {
  en = st;
  len = 0;
}

bool RingBuffer::is_empty() {
  return (len == 0);
}

void RingBuffer::dump() {
  int i;

  for (i=0; i<len; i++) {
    Serial.print("S ");
    Serial.print(st+i);
    Serial.print(" ");
    Serial.print((st+i) % RB_BUFF_MAX);
    Serial.print(" ");    
    Serial.println(rb[(st+i) % RB_BUFF_MAX], HEX);
  };
  for (i=0; i<t_len; i++) {
    Serial.print("T ");
    Serial.print(st+len+i);
    Serial.print(" ");
    Serial.print((st+len+i) % RB_BUFF_MAX);
    Serial.print(" ");    
    Serial.println(rb[(st+len+i) % RB_BUFF_MAX], HEX);
  };
}

void RingBuffer::dump2() {
  int i;

  Serial.println();
  for (i=0; i<len; i++) {
     Serial.print(rb[(st+i) % RB_BUFF_MAX], HEX);
     Serial.print(" ");
  };
  for (i=0; i<t_len; i++) {
    Serial.print(rb[(st+len+i) % RB_BUFF_MAX], HEX);
    Serial.print(" ");    
  };
  Serial.println();
}

//
// SparkIO class
//

SparkIO::SparkIO() {
  rb_state = 0;
  rc_state = 0;
}

SparkIO::~SparkIO() {
  
}

void SparkIO::start_bt() {
  if (!bt.begin (MY_NAME, true)) {
    DEBUG("Bluetooth init fail");
    while (true);
  }    
}

void SparkIO::connect_to_spark() {
  uint8_t b;
  bool connected;

  connected = false;

  while (!connected) {
    connected = bt.connect(SPARK_NAME);
    if (!(connected && bt.hasClient())) {
      connected = false;
      delay(2000);
    }
  }

  // flush anything read from Spark - just in case

  while (bt.available())
    b = bt.read(); 
}

//
// Routine to read the block from bluetooth and put into the in_chunk ring buffer
//

uint8_t chunk_header[16]{0x01, 0xfe, 0x00, 0x00, 0x41, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void SparkIO::process_in_blocks() {
  uint8_t b;
  bool boo;

  while (bt.available()) {
    b = bt.read();

    // check the 7th byte which holds the block length
    if (rb_state == 6) {
      rb_len = b - 16;
      rb_state++;
    }
    // check every other byte in the block header for a match to the header standard
    else if (rb_state > 0 && rb_state < 16) {
      if (b == chunk_header[rb_state]) {
        rb_state++;
      }
      else {
        rb_state = 0;
        DEBUG("Bad block header");
      }
    } 
    // and once past the header just read the next bytes as defined by rb_len
    // store these to the chunk buffer
    else if (rb_state == 16) {
      in_chunk.add(b);
      rb_len--;
      if (rb_len == 0) {
        rb_state = 0;
        in_chunk.commit();
      }
    }
      
    // checking for rb_state 0 is done separately so that if a prior step finds a mismatch
    // and resets the state to 0 it can be processed here for that byte - saves missing the 
    // first byte of the header if that was misplaced
    
    if (rb_state == 0) 
      if (b == chunk_header[0]) 
        rb_state++;
  }
}

//
// Routine to read chunks from the in_chunk ring buffer and copy to a in_message msgpack buffer
//

void SparkIO::process_in_chunks() {
  uint8_t b;
  bool boo;

  while (!in_chunk.is_empty()) {               // && in_message.is_empty()) {  -- no longer needed because in_message is now a proper ringbuffer
    boo = in_chunk.get(&b);
    if (!boo) DEBUG("Chunk is_empty was false but the buffer was empty!");

    switch (rc_state) {
      case 1:
        if (b == 0x01) 
          rc_state++; 
        else 
          rc_state = 0; 
        break;
      case 2:
        rc_seq = b; 
        rc_state++; 
        break;
      case 3:
        rc_checksum = b;
        rc_state++; 
        break;
      case 4:
        rc_cmd = b; 
        rc_state++; 
        break;
      case 5:
        rc_sub = b; 
        rc_state = 10;
        
        // set up for the main data loop - rc_state 10
        rc_bitmask = 0x80;
        rc_calc_checksum = 0;
        rc_data_pos = 0;
        
        // check for multi-chunk
        if (rc_cmd == 3 && rc_sub == 1) 
          rc_multi_chunk = true;
        else {
          rc_multi_chunk = false;
          in_message_bad = false;
          in_message.add(rc_cmd);
          in_message.add(rc_sub);
          in_message.add(0);
        }
        break;
      case 10:                    // the main loop which ends on an 0xf7
        if (b == 0xf7) {
          if (rc_calc_checksum != rc_checksum) 
            in_message_bad = true;
          rc_state = 0;
          if (!rc_multi_chunk || (rc_this_chunk == rc_total_chunks-1)) { //last chunk in message
            if (in_message_bad) {
              DEBUG("Bad message, dropped");
              in_message.drop();
            }
            else {
              in_message.set_at_index(2, in_message.get_len());
              in_message.commit();
            }  
          }
        }
        else if (rc_bitmask == 0x80) { // if the bitmask got to this value it is now a new bits 
          rc_calc_checksum ^= b;
          rc_bits = b;
          rc_bitmask = 1;
        }
        else {
          rc_data_pos++;
          rc_calc_checksum ^= b;          
          if (rc_bits & rc_bitmask) 
            b |= 0x80;
          rc_bitmask *= 2;
          
          if (rc_multi_chunk && rc_data_pos == 1) 
            rc_total_chunks = b;
          else if (rc_multi_chunk && rc_data_pos == 2) {
            rc_last_chunk = rc_this_chunk;
            rc_this_chunk = b;
            if (rc_this_chunk == 0) {
              in_message_bad = false;
              in_message.add(rc_cmd);
              in_message.add(rc_sub);
              in_message.add(0);
            }
            else if (rc_this_chunk != rc_last_chunk+1)
              in_message_bad = true;
          }
          else if (rc_multi_chunk && rc_data_pos == 3) 
            rc_chunk_len = b;
          else {  
            in_message.add(b);             
          }
          
        };
        break;
    }

    // checking for rc_state 0 is done separately so that if a prior step finds a mismatch
    // and resets the state to 0 it can be processed here for that byte - saves missing the 
    // first byte of the header if that was misplaced
    
    if (rc_state == 0) {
      if (b == 0xf0) 
        rc_state++;
    }
  }
}

void SparkIO::process_in_message() {

}

//// Routines to interpret the data

void SparkIO::read_byte(int *b)
{
  uint8_t a;
  in_message.get(&a);
  *b = a;
}   
   
void SparkIO::read_string(char *str)
{
  int a, i, len;

  read_byte(&a);
  if (a == 0xd9) {
    read_byte(&len);
  }
  else if (a > 0xa0) {
    len = a - 0xa0;
  }
  else {
    read_byte(&a);
    if (a < 0xa1 || a >= 0xc0) DEBUG("Bad string");
    len = a - 0xa0;
  }

  if (len > 0) {
    // process whole string but cap it at STR_LEN-1
    for (i = 0; i < len; i++) {
      read_byte(&a);
      if (i < STR_LEN -1) str[i]=a;
    }
    str[i > STR_LEN-1 ? STR_LEN-1 : i]='\0';
  }
  else {
    str[0]='\0';
  }
}   

void SparkIO::process() 
{
  sp.process_in_blocks();
  if (!in_chunk.is_empty()) {
    process_in_chunks();
  }
}

void SparkIO::read_prefixed_string(char *str)
{
  int a, i, len;

  read_byte(&a); 
  read_byte(&a);

  if (a < 0xa1 || a >= 0xc0) DEBUG("Bad string");
  len = a-0xa0;

  if (len > 0) {
    for (i = 0; i < len; i++) {
      read_byte(&a);
      if (i < STR_LEN -1) str[i]=a;
    }
    str[i > STR_LEN-1 ? STR_LEN-1 : i]='\0';
  }
  else {
    str[0]='\0';
  }
}   


void SparkIO::read_float(float *f)
{
  union {
    float val;
    byte b[4];
  } conv;   
  int a, i;

  read_byte(&a);  // should be 0xca
  if (a != 0xca) return;

  // Seems this creates the most significant byte in the last position, so for example
  // 120.0 = 0x42F00000 is stored as 0000F042  
   
  for (i=3; i>=0; i--) {
    read_byte(&a);
    conv.b[i] = a;
  } 
  *f = conv.val;
}


void SparkIO::read_onoff(bool *b)
{
  int a;
   
  read_byte(&a);
  if (a == 0xc3)
    *b = true;
  else // 0xc2
    *b = false;
}

// The functions to get the message

bool SparkIO::get_message(unsigned int *cmdsub, SparkMessage *msg, SparkPreset *preset)
{
  int cmd, sub, len;
  unsigned int cs;
   
  int junk;
  int i, j, num;

  if (in_message.is_empty()) return false;

  read_byte(&cmd);
  read_byte(&sub);
  read_byte(&len);

  cs = ((cmd & 0xff) << 8) | (sub & 0xff);

  *cmdsub = cs;

  
  if (cs == 0x0337 ) {
    read_string(msg->str1);
    read_byte(&msg->param1);
    read_float(&msg->val);
  }
  else if (cs == 0x0306 ) {
    read_string(msg->str1);
    read_string(msg->str2);
  }
  else if (cs == 0x0301) {
    read_byte(&junk);
    read_byte(&preset->preset_num);
    read_string(preset->UUID); 
    read_string(preset->Name);
    read_string(preset->Version);
    read_string(preset->Description);
    read_string(preset->Icon);
    read_float(&preset->BPM);

    for (j=0; j<7; j++) {
      read_string(preset->effects[j].EffectName);
      read_onoff(&preset->effects[j].OnOff);
      read_byte(&num);
      preset->effects[j].NumParameters = num - 0x90;
      for (i = 0; i < preset->effects[j].NumParameters; i++) {
        read_byte(&junk);
        read_byte(&junk);
        read_float(&preset->effects[j].Parameters[i]);
      }
    }
    read_byte(&junk);  
  }
  else {
    Serial.print("Unprocessed message ");
    Serial.println (cs, HEX);
    for (i = 0; i < len - 3; i++) {
      read_byte(&junk);
      Serial.print(junk, HEX);
      Serial.print(" ");
    }
    Serial.println();
  }

  return true;
}

    
//
//
//

void SparkIO::write_byte(byte b)
{
  out_message.add(b);
}

void SparkIO::write_prefixed_string(const char *str)
{
  int len, i;

  len = strnlen(str, STR_LEN);
  write_byte(byte(len));
  write_byte(byte(len + 0xa0));
  for (i=0; i<len; i++)
    write_byte(byte(str[i]));
}

void SparkIO::write_string(const char *str)
{
  int len, i;

  len = strnlen(str, STR_LEN);
  write_byte(byte(len + 0xa0));
  for (i=0; i<len; i++)
    write_byte(byte(str[i]));
}      
  
void SparkIO::write_long_string(const char *str)
{
  int len, i;

  len = strnlen(str, STR_LEN);
  write_byte(byte(0xd9));
  write_byte(byte(len));
  for (i=0; i<len; i++)
    write_byte(byte(str[i]));
}

void SparkIO::write_float (float flt)
{
  union {
    float val;
    byte b[4];
  } conv;
  int i;
   
  conv.val = flt;
  // Seems this creates the most significant byte in the last position, so for example
  // 120.0 = 0x42F00000 is stored as 0000F042  
   
  write_byte(0xca);
  for (i=3; i>=0; i--) {
    write_byte(byte(conv.b[i]));
  }
}

void SparkIO::write_onoff (bool onoff)
{
  byte b;

  if (onoff)
  // true is 'on'
    b = 0xc3;
  else
    b = 0xc2;
  write_byte(b);
}

//
//

void SparkIO::change_effect_parameter (char *pedal, int param, float val)
{
   start_message (0x0104);
   write_prefixed_string (pedal);
   write_byte (byte(param));
   write_float(val);
   end_message();
}


void SparkIO::change_effect (char *pedal1, char *pedal2)
{
   start_message (0x0106);
   write_prefixed_string (pedal1);
   write_prefixed_string (pedal2);
   end_message();
}

void SparkIO::change_hardware_preset (int preset_num)
{
   // preset_num is 0 to 3

   start_message (0x0138);
   write_byte (byte(0));
   write_byte (byte(preset_num))  ;     
   end_message();  
}

void SparkIO::turn_effect_onoff (char *pedal, bool onoff)
{
   start_message (0x0115);
   write_prefixed_string (pedal);
   write_onoff (onoff);
   end_message();
}

void SparkIO::get_serial()
{
   start_message (0x0223);
   end_message();  
}


void SparkIO::start_message(int cmdsub)
{
  om_cmd = (cmdsub & 0xff00) >> 8;
  om_sub = cmdsub & 0xff;
  
  // THIS IS TEMPORARY JUST TO SHOW IT WORKS!!!!!!!!!!!!!!!!
  sp.out_message.clear();
}

void SparkIO::end_message()
{
  sp.out_message.commit();
}

void SparkIO::create_preset(SparkPreset *preset)
{
  int i, j, siz;

  start_message (0x0101);

  write_byte (0x00);
  write_byte (preset->preset_num);   
  write_long_string (preset->UUID);
  write_string (preset->Name);
  write_string (preset->Version);
  if (strnlen (preset->Description, STR_LEN) > 31)
    write_long_string (preset->Description);
  else
    write_string (preset->Description);
  write_string(preset->Icon);
  write_float (preset->BPM);

   
  write_byte (byte(0x90 + 7));       // always 7 pedals

  for (i=0; i<7; i++) {
      
    write_string (preset->effects[i].EffectName);
    write_onoff(preset->effects[i].OnOff);

    siz = preset->effects[i].NumParameters;
    write_byte ( 0x90 + siz); 
      
    for (j=0; j<siz; j++) {
      write_byte (j);
      write_byte (byte(0x91));
      write_float (preset->effects[i].Parameters[j]);
    }
  }
  write_byte (preset->end_filler);  
  end_message();
}
