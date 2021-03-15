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

bool RingBuffer::set_bit_at_index(int ind, uint8_t b) {
  if (ind >= 0 && ind < t_len) {
    rb[(st+len+ind) % RB_BUFF_MAX] |= b;    
    return true;
  }
  else
    return false; 
}

int RingBuffer::get_len() { // total temp len
  return t_len;
}

int RingBuffer::get_pos() { // current position
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
  uint8_t v;

  Serial.println();
  for (i=0; i<len; i++) {
    v=rb[(st+i) % RB_BUFF_MAX];
    if (v < 16) Serial.print("0");
    Serial.print(v, HEX);
    Serial.print(" ");
    //Serial.print(rb[(st+i) % RB_BUFF_MAX], HEX);
    //Serial.print(" ");
  };
  for (i=0; i<t_len; i++) {
    v=rb[(st+len+i) % RB_BUFF_MAX];
    if (v < 16) Serial.print("0");
    Serial.print(v, HEX);
    Serial.print(" ");
    
    //Serial.print(rb[(st+len+i) % RB_BUFF_MAX], HEX);
    //Serial.print(" ");    
  };
  Serial.println();
}

//
// Helpers
//


void uint_to_bytes(unsigned int i, uint8_t *h, uint8_t *l) {
  *h = (i & 0xff00) / 256;
  *l = i & 0xff;
}

void bytes_to_uint(uint8_t h, uint8_t l,unsigned int *i) {
  *i = (h & 0xff) * 256 + (l & 0xff);
}

//
// SparkIO class
//

SparkIO::SparkIO() {
  rb_state = 0;
  rc_state = 0;
  oc_seq = 0;
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
  unsigned int len;
  uint8_t len_h, len_l;

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
              len = in_message.get_len();
              uint_to_bytes(len, &len_h, &len_l);

              in_message.set_at_index(2, len_h);
              in_message.set_at_index(3, len_l);
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

//// Routines to interpret the data

void SparkIO::read_byte(uint8_t *b)
{
  uint8_t a;
  in_message.get(&a);
  *b = a;
}   
   
void SparkIO::read_string(char *str)
{
  uint8_t a, len;
  int i;

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
  // process inputs
  sp.process_in_blocks();
  if (!in_chunk.is_empty()) {
    process_in_chunks();
  }

  // process outputs
//  sp.process_out_chunks();
//  sp.process_out_blocks();
}

void SparkIO::read_prefixed_string(char *str)
{
  uint8_t a, len;
  int i;

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
  uint8_t a;
  int i;

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
  uint8_t a;
   
  read_byte(&a);
  if (a == 0xc3)
    *b = true;
  else // 0xc2
    *b = false;
}

// The functions to get the message

bool SparkIO::get_message(unsigned int *cmdsub, SparkMessage *msg, SparkPreset *preset)
{
  uint8_t cmd, sub, len_h, len_l;
  unsigned int len;
  unsigned int cs;
   
  uint8_t junk;
  int i, j;
  uint8_t num;

  if (in_message.is_empty()) return false;

  read_byte(&cmd);
  read_byte(&sub);
  read_byte(&len_h);
  read_byte(&len_l);
  bytes_to_uint(len_h, len_l, &len);

  bytes_to_uint(cmd, sub, &cs);

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
    for (i = 0; i < len - 4; i++) {
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


void SparkIO::start_message(int cmdsub)
{
  om_cmd = (cmdsub & 0xff00) >> 8;
  om_sub = cmdsub & 0xff;

  // THIS IS TEMPORARY JUST TO SHOW IT WORKS!!!!!!!!!!!!!!!!
  sp.out_message.clear();

  out_message.add(om_cmd);
  out_message.add(om_sub);
  out_message.add(0);      // placeholder for length
  out_message.add(0);      // placeholder for length
}


void SparkIO::end_message()
{
  unsigned int len;
  uint8_t len_h, len_l;
  
  len = out_message.get_len();
  uint_to_bytes(len, &len_h, &len_l);
  
  out_message.set_at_index(2, len_h);   
  out_message.set_at_index(3, len_l);
  sp.out_message.commit();
}


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

void SparkIO::change_hardware_preset (uint8_t preset_num)
{
   // preset_num is 0 to 3

   start_message (0x0138);
   write_byte (0);
   write_byte (preset_num)  ;     
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

//
//
//




void SparkIO::out_store(uint8_t b)
{
  uint8_t bits;
  
  if (oc_bit_mask == 0x80) {
    oc_bit_mask = 1;
    oc_bit_pos = out_chunk.get_pos();
    out_chunk.add(0);
  }
  
  if (b & 0x80) {
    out_chunk.set_bit_at_index(oc_bit_pos, oc_bit_mask);
    oc_checksum ^= oc_bit_mask;
  }
  out_chunk.add(b & 0x7f);
  oc_checksum ^= (b & 0x7f);

  oc_len++;

  /*
  if (oc_bit_mask == 0x40) {
    out_chunk.get_at_index(oc_bit_pos, &bits);
    oc_checksum ^= bits;    
  }
*/  
  oc_bit_mask *= 2;
}


void SparkIO::process_out_chunks() {
  int i, j, len;
  int checksum_pos;
  uint8_t b;
  uint8_t len_h, len_l;

  uint8_t num_chunks, this_chunk, this_len;
 
  while (!out_message.is_empty()) {
    out_message.get(&oc_cmd);
    out_message.get(&oc_sub);
    out_message.get(&len_h);
    out_message.get(&len_l);
    bytes_to_uint(len_h, len_l, &oc_len);
    len = oc_len -4;

    if (len > 0x80) { //this is a multi-chunk message
      num_chunks = int(len / 0x80) + 1;
      for (this_chunk=0; this_chunk < num_chunks; this_chunk++) {
       
        // create chunk header
        out_chunk.add(0xf0);
        out_chunk.add(0x01);
        out_chunk.add(oc_seq);
        checksum_pos = out_chunk.get_pos();
        out_chunk.add(0); // checksum
        
        out_chunk.add(oc_cmd);
        out_chunk.add(oc_sub);

        if (num_chunks == this_chunk+1) 
          this_len = len % 0x80; 
        else 
          this_len = 0x80;

        oc_bit_mask = 0x80;
        oc_checksum = 0;
        
        // create chunk sub-header          
        out_store(num_chunks);
        out_store(this_chunk);
        out_store(this_len);
        
        for (i = 0; i < this_len; i++) {
          out_message.get(&b);
          out_store(b);
        }
        out_chunk.set_at_index(checksum_pos, oc_checksum);        
        out_chunk.add(0xf7);
      }
    } 
    else { 
    // create chunk header
      out_chunk.add(0xf0);
      out_chunk.add(0x01);
      out_chunk.add(oc_seq);

      checksum_pos = out_chunk.get_pos();
      out_chunk.add(0); // checksum

      out_chunk.add(oc_cmd);
      out_chunk.add(oc_sub);

      oc_bit_mask = 0x80;
      oc_checksum = 0;
      for (i = 0; i < len; i++) {
        out_message.get(&b);
        out_store(b);
      }
     out_chunk.set_at_index(checksum_pos, oc_checksum);        
     out_chunk.add(0xf7);
    }
    out_chunk.commit();
  }
}

void SparkIO::process_out_blocks() {
  int i;
  int len;
  uint8_t b;  

  while (!out_chunk.is_empty()) {
    ob_pos = 16;
  
    out_block[0]= 0x01;
    out_block[1]= 0xfe;  
    out_block[2]= 0x00;    
    out_block[3]= 0x00;
    out_block[4]= 0x53;
    out_block[5]= 0xfe;
    out_block[6]= 0x00;
    for (i=7; i<16;i++) 
      out_block[i]= 0x00;
    
    b = 0;
    while (b != 0xf7) {
      sp.out_chunk.get(&b);
      out_block[ob_pos++] = b;
    }

    out_block[6] = ob_pos;

    for (i=0; i<ob_pos; i++) {
      if (out_block[i]<16) Serial.print("0");
      Serial.print(out_block[i], HEX); 
    }

    sp.bt.write(out_block, ob_pos);
    delay(200);
  }
}