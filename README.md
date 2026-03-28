# serial_library
A small library for easily receiving, parsing, assembling, and sending strings.

Using this library, users may create *transceivers* which send and receive data through various interfaces. Some interfaces built into the library already include:
- Serial port (linux)
- UDP socket (linux)
- ROS pub/sub

...and Windows serial port support is under active development.

Transceivers are owned by the `SerialProcessor`, which parses incoming bytes into a map of values using frame definitions provided by the user. Conversely, users can set values within the map and have the processor assemble and send frames.

This library was originally created for the Ohio State [Underwater Robotics Team](https://github.com/osu-uwrt) as serial processing code for the team's new fiber optic gyro, but has shown great potential as a general-purpose library. This repo serves as the grounds for continued development into a more useful and extensible package.

## Building
### Normal CMake
To add this library to a CMake project, simply add the repository (cloned, submoduled, etc) as a CMake subdirectory.

```cmake
add_subdirectory(serial_library)
```

This will make a target available called `serial_library`. Then, simply link it

```cmake
add_subdirectory(serial_library)

add_executable(my_program
    src/a.cpp
    src/b.cpp)

target_link_libraries(my_program PUBLIC
    serial_library)
```

### Ament/ROS

You can also build this library as a ROS package using `colcon build`. Then, in your project, you can treat `serial_library` like a normal ROS package:

```cmake
find_package(serial_library REQUIRED)

add_executable(my_program
    src/a.cpp
    src/b.cpp)

ament_target_dependencies(my_program
    serial_library)

install(TARGETS my_program
    DESTINATION lib/${PROJECT_NAME})
```

## Usage
### Includes
Include `serial_library.hpp` to gain access to all serial_library functions and types:

```cpp
#include <serial_library.hpp>

// optionally, add the namespace to make the code nicer
using namespace serial_library; 
```

### Creating a frame

A frame is composed of "fields" which are independent values in the map. Start by defining their names in an enum:

```cpp
enum ExampleFrames
{
    MOTOR_FRAME
}

enum ExampleFields
{
    FIELD_MOTOR_SPEED,
    FIELD_MOTOR_POSITION,
    FIELD_MOTOR_THROTTLE
};
```

Then, arrange your fields into a frame:

```cpp
serial_library::SerialFrame frame = ({
    FIELD_SYNC,             // byte 0
    FIELD_MOTOR_THROTTLE,   // byte 1
    FIELD_MOTOR_SPEED,      // byte 2
    FIELD_MOTOR_SPEED,      // ...
    FIELD_MOTOR_POSITION,
    FIELD_MOTOR_POSITION,
});
```

When defining a frame, each item in the vector is considered a "byte". In this example, the second byte in the serial frame (byte 1) holds the value of the motor throttle. 

If a field occupies multiple positions in a vector, then the value is split among those positions. In this example, bytes 2 and 3 contain the motor speed, with byte 2 containing the most significant bits, and byte 3 containing the least significant bits (unless endianness is switched). The `SerialProcessor` will combine these bytes and present the user with a 16-bit value representing motor speed. The same can be said for the motor position field.

Byte 0 contains FIELD_SYNC, which is required to be present in all frames, and specifies where in the frame the processor can find a known sync byte which it can use to align the data.

You can also interleave fields with each other:
```cpp
serial_library::SerialFrame frame = ({
    FIELD_SYNC,
    FIELD_MOTOR_THROTTLE,
    FIELD_MOTOR_SPEED,      // speed msb
    FIELD_MOTOR_POSITION,   // position msb
    FIELD_MOTOR_SPEED,      // speed lsb
    FIELD_MOTOR_POSITION,   // position lsb
});
```

Longer serial frames can be created using the utility function `assembleSerialFrame`

```cpp
serial_library::SerialFrame frame = serial_library::assembleSerialFrame({
    { FIELD_SYNC, 1 },
    { FIELD_MOTOR_THROTTLE, 1},
    { FIELD_MOTOR_SPEED, 2},     // 2 consecutive bytes of FIELD_MOTOR_SPEED
    { FIELD_MOTOR_POSITION, 2}   // 2 consecutive bytes of FIELD_MOTOR_POSITION
})
```

### Setting up a Serial Processor to receive the frame

The following example uses the frame created in the previous example and creates a processor to parse it. Though the example uses `LinuxSerialTransceiver` to send and receive data, any properly-implemented class extending `SerialTransceiver` will work.

```cpp
// define frame map
serial_library::SerialFramesMap allFrames = {
    {
        MOTOR_FRAME,
        frame
    } //, ...
};

const char syncValue = ';';

// create transceiver
auto transceiver = std::make_unique<serial_library::LinuxSerialTransceiver>(
    port,
    GYRO_BAUD,
    1, 0, O_RDONLY
);

// (optional) callbacks
serial_library::SerialProcessorCallbacks cbs = DEFAULT_CALLBACKS;
cbs.newMessageCallback = ...;       // called when a new valid message is received
cbs.checksumEvaluationFunc = ...;   // called with an incoming message buffer to verify checksum, return true for valid and false for invalid
cbs.checksumGenerationFunc = ...;   // called with an outgoing message to generate a checksum

auto proc = std::make_shared<serial_library::SerialProcessor>(
    std::move(transceiver), // (optional) transceiver
    allFrames,              // frame map
    MOTOR_FRAME,            // default frame id
    &syncValue, 1,          // sync value and len
    false,                  // (optional) dont switch endianness
    cbs                     // (optional) callbacks
);

// now, in order for the processor to work, it must be spun
while(true)
{
    proc->update(serial_library::curtime());
}
```

Note that in the `SerialProcessor` constructor, the transceiver is marked as optional. If it is unspecified, the processor will be idle (skipping update()'s) until given one:

```cpp
proc->setTransceiver(transceiver);
```

### Setting/Accessing fields

```cpp
// getting
uint8_t desired_status = proc->getFieldValue<uint16_t>(ExampleFields::FIELD_MOTOR_SPEED);

// setting
proc->setFieldValue<uint8_t>(ExampleFields::FIELD_MOTOR_THROTTLE, 42, serial_library::curtime());
```

### Using multiple frames

`SerialProcessor` can parse more than one type of frame. In a multi-frame pattern, all frames are required to include not just FIELD_SYNC, but also FIELD_FRAME, to indicate which byte in the packet will specify the type of frame being used. So, lets split the frame in the previous examples into three frames. 

- Frame 1 (out):
    - Motor throttle command
- Frame 2 (in):
    - Motor throttle received
    - Motor velocity
    - Motor position
- Frame 3 (in):
    - Motor throttle received
    - Motor temperature

The directions of the frames don't actually matter to the library but were only specified to illustrate how they might be designed for a basic motor driver.

```cpp
enum MultiExampleFrames
{
    MOTOR_COMMAND_FRAME,
    MOTOR_KINEMATICS_FRAME,
    MOTOR_TEMPERATURE_FRAME
}

enum MultiExampleFields
{
    THROTTLE_COMMAND,
    THROTTLE_RECEIVED,
    VELOCITY,
    POSITION,
    TEMPERATURE
}

serial_library::SerialFramesMap frames = {
    {
        MOTOR_COMMAND_FRAME,
        {
            FIELD_SYNC,         // sync (already required)
            FIELD_FRAME,        // frame (required in multi-frame pattern)
            THROTTLE_COMMAND
        }
    },
    {
        MOTOR_KINEMATICS_FRAME,
        {
            FIELD_SYNC,
            FIELD_FRAME,
            THROTTLE_RECEIVED,      // throttle received
            VELOCITY,
            VELOCITY,
            POSITION,
            POSITION,
        }
    },
    {
        MOTOR_TEMPERATURE_FRAME,
        {
            FIELD_SYNC,
            FIELD_FRAME,
            THROTTLE_RECEIVED,      // throttle received again (you can reuse fields in different frames)
            TEMPERATURE,
            TEMPERATURE
        }
    }
};
```

### Building custom transceivers

serial_library uses a simple C++ interface to allow users to implement their own transceivers. Simply create a class that extends `SerialTransceiver` and override the four pure virtual functions:

- `bool init()`: Initializes the transceiver. For example, for a serial port, open and configure it here. Return true for success. On failure, either throw a `NonFatalSerialLibraryException` or return false.
- `void send(const char *data, size_t numData)`: Send `numData` bytes out of the `data` buffer.
- `size_t recv(char *data, size_t numData)`: Read *up to* `numData` bytes to the `data` buffer and return the actual number of bytes read. This function can block or return immediately, whatever suits the user's application best.
- `void deinit()`: Destroy the transceiver. For example, for a serial port, close it here. Users should be able to call init() on the transceiver again and be able to use it normally.
