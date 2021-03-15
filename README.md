# SparkIO
ESP32 C++ class to communicate with the Spark -generating and receiving proper messages   

NEWS  - V1.0 now on github

SparkIO creates the bluetooth connection to the Spark, creating and sending messages to the Spark and receiving messages from the Spark and unpacking them.   
It even works in the most complex case where a multi-chunk preset is sandwiched by other parameter change messages in the same set of blocks.   

To handle the asynchronous nature of the receipt of messages, a ring buffer is used to store the chunk data.  
This is then converted to msgpack data in a second ring buffer.  

Eventually this is converted to C++ structure SparkPreset or SparkMessage by a call to sp.get_message(&cmdsub, &msg, &preset)

To use create an instance of the class and a variables for the commnand, a preset and a message, then in loop() call the async processing part (process) and then retrieve the messages with get_message().    
get_message() returns true if a message has been retrieved, and it is stored in either &msg or &preset depending on whether it was a simple message or a preset.   
Using cmdsub will tell you what was done - this holds the command and sub-command from the Spark.  

Eg cmdsub == 0x0301 means a preset was returned from the Spark and therefore &preset has data.  

Message for the Spark can be created with the functions below. They will create a message from the parameters, queue it and send it to the Spark.   

```
    void create_preset(SparkPreset *preset);
    void get_serial();
    void turn_effect_onoff(char *pedal, bool onoff);
    void change_hardware_preset(uint8_t preset_num);
    void change_effect(char *pedal1, char *pedal2);
    void change_effect_parameter(char *pedal, int param, float val);
```

An example of the core program needed is:

``` 
SparkIO sp;

unsigned int cmdsub;
SparkPreset preset;
SparkMessage msg;

void setup() 
{
  ...
  sp.start_bt();
  sp.connect_to_spark();
  ...
}

```

To run the asynchronous part, ensure that you regularly call:

```  
void loop ()
{

  sp.process();

  if (something_happens_like_a_pin_read) {
    change_hardware_preset(2);
    }

  if (sp.get_message(&cmdsub, &msg, &preset)) {
    // do something based on the cmdsub field
    }
  
}

```
