# SparkIO / SparkAppIO

ESP32 C++ class to communicate with the Spark amp and Spark App - generating and receiving proper messages   

NEWS  - V1.0 now on github

Some diagrams below to show the overview of the process within SparkIO. SparkAppIO is broadly the same but with different message numbers.


# Background   

SparkIO creates the bluetooth connection to the Spark, creating and sending messages to the Spark and receiving messages from the Spark and unpacking them.   
It even works in the most complex case where a multi-chunk preset is sandwiched by other parameter change messages in the same set of blocks.  

SparkAppIO assumes a serial connection to another device, which is in turn connected to the Spark app. This is because I can't find an ESP32 device that allows two bluetooth connections, so the App connection is proxied by a different device.

SparkComms handles the bluetooth connection and serial connections, so this is abstracted from SparkIO and SparkAppIO.   Both SparkIO and SparkAppIO need a reference to this class.   

To handle the asynchronous nature of the receipt of messages, a ring buffer is used to store the chunk data.  
This is then converted to msgpack data in a second ring buffer.  

Eventually this is converted to C++ structure SparkPreset or SparkMessage by a call to 

```
sp.get_message(&cmdsub, &msg, &preset)
```

# How to use   

This is similar for SparkIO and SparkAppIO - they handle messages to and from the amp / app in a similar way.   

You don't need both classes in a program - if you only want to talk to the amp just you only need SparkIO.   

Both classes allow creation with a boolean parameter - true allows passthru of all messages between app and amp whilst also processing them, false will block these messages so that all messages need to be processed in the ```loop()```   

To use create an instance of the class and a variables for the commnand, a preset and a message, then in ```loop()``` call the async processing part ```process()``` and then retrieve the messages with ```get_message()```.    

```get_message(&cmdsub, &msg, &preset)``` returns true if a message has been retrieved, and it is stored in either ```&msg``` or ```&preset``` depending on whether it was a simple message or a preset.   

Using ```cmdsub``` will tell you what was done - this holds the command and sub-command from the Spark.  

Eg cmdsub == 0x0301 means a preset was returned from the Spark and therefore ```&preset``` has data - otherwise ```&msg``` will have the data. All responses are either embedded in a message or a full preset it returned.    

Messages for the Spark can be created with the functions below. They will create a message from the parameters, queue it and send it to the Spark.   

```
void create_preset(SparkPreset *preset);

void turn_effect_onoff(char *pedal, bool onoff);
void change_hardware_preset(uint8_t preset_num);
void change_effect(char *pedal1, char *pedal2);
void change_effect_parameter(char *pedal, int param, float val);

void get_serial();
void get_name();
void get_hardware_preset_number();
void get_preset_details(unsigned int preset);
```

An example of the core program needed is:

``` 
SparkAppIO app_io(true);         // true in the parameter sends all messages from serial to the amp *via bt)  - passthru
SparkIO spark_io(true);          // true in the parameter sends all messages from the amp (via bt) to serial - passthru
SparkComms spark_comms;

unsigned int cmdsub;
SparkPreset preset;
SparkMessage msg;

void setup() 
{
  ...

  spark_io.comms = &spark_comms;  // link to the comms class
  app_io.comms = &spark_comms;    // link to the comms class

  spark_comms.start_bt();         // start bt
  spark_comms.connect_to_spark(); // connect bt to amp
  spark_comms.start_ser();        // start serial and assume another ESP 32 is linked to the app
  
  ...
}

```

To run the asynchronous part, ensure that you regularly call ```process()``` - that is where the magic happens.   
No messages are sent or received synchronously - they are all added to ring buffers which are then handled by ```process()```   

```  
void loop ()
{

  spark_io.process();             // where all the async processing happens - receive and transmit - to and from amp
  app_io.process();               // where all the async processing happens - receive and transmit - to and from app

  if (something_happens_like_a_pin_read) {
    spark_io.change_hardware_preset(2);                     // just an example message to create
    }

  if (spark_io.get_message(&cmdsub, &msg, &preset)) {       // get any messages from the amp
    // do something based on the cmdsub field
    switch (cmdsub) {                                 // process messages received
      case 0x0301:                                    
        // got a preset sent back
        break;
      case 0x0337:
        // got a change of effect parameter
        break;
    }
  }
}

```
And the two structures are shown below.  If the data returned is a Message then only specific fields are populated - see SparkIO.ino for details.

```
typedef struct  {
  uint8_t  curr_preset;
  uint8_t  preset_num;
  char UUID[STR_LEN];
  char Name[STR_LEN];
  char Version[STR_LEN];
  char Description[STR_LEN];
  char Icon[STR_LEN];
  float BPM;
  struct SparkEffects {
    char EffectName[STR_LEN];
    bool OnOff;
    uint8_t  NumParameters;
    float Parameters[10];
  } effects[7];
  uint8_t chksum;
} SparkPreset;

typedef struct {
  uint8_t param1;
  uint8_t param2;
  uint8_t param3;
  uint8_t param4;
  uint32_t param5;
  float val;
  char str1[STR_LEN];
  char str2[STR_LEN];
  bool onoff;
} SparkMessage;
```

# SparkIO messages handled   

## From Amp   

|cmdsub | str1 | str2 | val | param1 | param2 | onoff |
|-------|------|------|-----|--------|--------|-------|
|0323   | amp serial # | | | | | |
|0321   | amp name | | | | | |
|0327   |  | | | hardware preset number - first byte | hardware preset number - second byte| |
|0337   | effect name  | | effect val | effect number | | |
|0306   | old effect | new effect | | | | |
|0338   |  | | | 0 | new hw preset (0-3) | |
|0401   |
|0438   |
|0406   |

|cmdsub | preset | 
|-------|--------|
|0301   | preset |

## To Amp

|cmdsub | parameters |
|-------| -----------|
|0101   | SparkPreset |
|0104   | pedal, param, val |
|0106   | pedal1, pedal2 |
|0138   | preset_num |
|0115   | pedal, onoff |
|0223   | |
|0211   | |
|0211   | |
|0201   | preset_num |


# SparkAppIO messages handled   

## From App   

|cmdsub | str1 | str2 | val | param1 | param2 | onoff |
|-------|------|------|-----|--------|--------|-------|
|0123   | amp serial # | | | | | |
|0121   | amp name | | | | | |
|0110   |  | | | hardware preset number - first byte | hardware preset number - second byte| |
|0106   | old effect | new effect | | | | |
|0138   |  | | | 0 | new hw preset (0-3) | |
|0104   | effect name | | effect val | effect number | | |

|cmdsub | preset | 
|-------|--------|
|0101   | SparkPreset |

## To App   

|cmdsub | parameters |
|-------| -----------|
|0306   | pedal1, pedal2 |
|0338   | preset_num |
|0337   | pedal, param, val |
|0315   | pedal, onoff |
|0327   | preset_num |
|0301   | SparkPreset| 

# Overview of SparkIO class for sending messages   

![SparkIO4](https://github.com/paulhamsh/SparkIO/blob/main/SparkIO4.JPG)

# Overview of SparkIO class for receiving messages   

![SparkIO3](https://github.com/paulhamsh/SparkIO/blob/main/SparkIO3.JPG)

# Detail of SparkIO class for sending messages   

![SparkIO2](https://github.com/paulhamsh/SparkIO/blob/main/SparkIO2.JPG)

# Detail of SparkIO class for receiving messages   

![SparkIO1](https://github.com/paulhamsh/SparkIO/blob/main/SparkIO1.JPG)
