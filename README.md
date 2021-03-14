# SparkIO
ESP32 C++ class to communicate with the Spark -generating and receiving proper messages   


THIS IS A WORK IN PROGRESS

SparkClass and SparkClass.h are also used currently - they will be removed once no longer required, and fully replaced by SparkIO.   

Currently SparkIO works for creating the connection to the Spark and receiving messages from the Spark and unpacking them.   
It even works in the most complex case where a multi-chunk preset is sandwiched by other parameter change messages in the same set of blocks.   

It can also partially create messages to send to the Spark but this is not complete yet.   

To handle the asynchronous nature of the receipt of messages, a ring buffer is used to store the chunk data.  
This is then converted to msgpack data in a second ring buffer.  

Eventually this is converted to C++ structure SparkPreset or SparkMessage by a call to sp.get_message(&cmdsub, &msg, &preset)


To use create an instance of the class and a variables for the commnand, a preset and a message, then in loop() call the async processing part (process) and then retrieve the messages with get_message().    
get_message() returns true if a message has been retrieved, and it is stored in either &msg or &preset depending on whether it was a simple message or a preset.   
Using cmdsub will tell you what was done - this holds the command and sub-command from the Spark.  

Eg cmdsub == 0x0301 means a preset was returned from the Spark and therefore &preset has data.

``` 
SparkIO sp;

unsigned int cmdsub;
SparkPreset preset;
SparkMessage msg;
```

To run the asynchronous part, ensure that you regularly call:

```   
void loop ()
{

sp.process();

if (sp.get_message(&cmdsub, &msg, &preset)) {

// do something based on the cmdsub field
  }
  
}

```
