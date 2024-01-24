![build badge](https://github.com/Auterion/libmav/actions/workflows/test.yml/badge.svg)

# libmav

The better mavlink library.

Libmav is a library for interacting with [mavlink](https://mavlink.io) systems. 
It provides message serialization / deserialization, code to interact with physical 
networks and several of the mavlink internal protocols.

### What's the advantage?

There are many mavlink libraries out there. The main advantages of this library over others are:

- **Runtime defined message set**. No need to recompile on message set change
- [**Native python bindings**](https://github.com/Auterion/libmav-python) for C++ code. Faster than pure python.
- **Header-only, no dependencies**, C++ 17, >90% test coverage

## How to install

There are several ways to get it:

Install globally on your system
```bash
cmake -Bbuild -S.
sudo cmake --build build --target install
```
Since the library is header only, you only need the library on the build system.

You can also include the library as a submodule in your project.

### Running the tests

Libmav uses [doctest](https://github.com/doctest/doctest/) and [gcovr](https://github.com/gcovr/gcovr/).

To run the tests, build the library, then run the test executable. Test results will be output to console.

```bash
mkdir build && cd build && cmake .. && make tests
./tests/tests
```

To test coverage, simple invoke the coverage tool from the root directory.
```bash
gcovr
```

## Getting started

### Loading a message set

In libmav, the message set is runtime defined and loaded from XML files.
Any of the upstream XML files will work. The function also loads dependent XML files
recursively.
```C++
auto message_set = mav::MessageSet("/path/to/common.xml");
```

You can also extend a loaded message set with additional XML files or inline XML:

```C++
message_set.addFromXMLString(R""""(
<mavlink>
    <messages>
        <message id="9912" name="TEMPERATURE_MEASUREMENT">
            <field type="float" name="temperature">The measured temperature in degress C</field>
        </message>
    </messages>
</mavlink>
)"""");
```

### Creating messages

From the message set, you can instantiate and populate a message like so

```C++
auto message = message_set.create("PARAM_REQUEST_READ");
message["target_system"] = 1;
message["target_component"] = 1;
message["param_id"] = "SYS_AUTOSTART";
message["param_index"] = -1;
```

Alternatively you can also use an initializer list notation

```C++

message.set({
    {"target_system", 1},
    {"target_component", 1},
    {"param_id", "SYS_AUTOSTART"},
    {"param_index", -1}
});
```


### Connecting to a Network

Libmav has classes for the following protocols:
- Serial
- TCP Client
- TCP Server
- UDP Client
- UDP Server

Libmav does not do any threading, except for the `NetworkRuntime` class.
The `NetworkRuntime` class spawns two threads. One to drive the receive
loop and one to drive the HEARTBEAT transmission and timeout handling.

```C++
// Create interface for physical network
auto physical = mav::TCPClient("<ip>", 4560);

auto heartbeat_message = message_set.create("HEARTBEAT");
heartbeat_message.set({
    {"type", message_set.e("MAV_TYPE_ONBOARD_CONTROLLER")},
    {"autopilot", message_set.e("MAV_AUTOPILOT_GENERIC")},
    {"base_mode", 0},
    {"custom_mode", 0},
    {"system_status", message_set.e("MAV_STATE_ACTIVE")},
    {"mavlink_version", 2}
});

// Create a network runtime with
// This runtime will automatically send out HEARTBEAT messages, iff a HEARTBEAT
// message is defined here
auto runtime = mav::NetworkRuntime(
        message_set, heartbeat_message, physical);

// You can change the heartbeat message any time by calling
// setHeartbeatMessage on the runtime. Also you can clear
// the heartbeat at any time by calling clearHeartbeat

// Wait for the connection (as defined as we 
// receive some mavlink from the other end), with a 2s timeout
auto connection = runtime.awaitConnection(2000);

// Check if connection is still alive
if (connection->alive()) {
    // Do something with the connection
}
```

```C++
// Create interface for physical network
auto physical = mav::UDPServer(14559);

// Create a network runtime with
// This runtime will not automatically send HEARTBEAT messages
auto runtime = mav::NetworkRuntime(message_set, physical);

// Handle incoming connections
runtime.onConnection([](std::shared_ptr<mav::Connection> connection) {
    // Do something with the connection
});

runtime.onConnectionLost([](std::shared_ptr<mav::Connection> connection) {
    // Do something when a connection drops
});

```


The classes will throw `mav::NetworkError` if connection fails.

### Sending / receiving messages

#### Sync / promise API

The easiest way to send receive messages is using the synchronous API:

```C++

// create a message
auto message = message_set.create("PARAM_REQUEST_READ") ({
        {"target_system", 1},
        {"target_system", 1},
        {"param_id", "SYS_AUTOSTART"},
        {"param_index", -1}
});
// send a message
conection.send(message);

// ⚠️ Potential race condition here! (read below)

// receive a message, with a 1s timeout
auto response = connection->receive("PARAM_VALUE", 1000);
```

The call to receive is blocking.
Note that the code above has a potential race condition. To start receiving
for a response before the request goes out, you can do this:

```C++
// create a message
auto message = message_set.create("PARAM_REQUEST_READ") ({
    {"target_system", 1},
    {"target_system", 1},
    {"param_id", "SYS_AUTOSTART"},
    {"param_index", -1}
});

// Already watch for the answer
auto expectation = connection->expect("PARAM_VALUE");
// send the message
conection.send(message);
// Wait for the answer, with a 1s timeout
auto response = connection->receive(expecation, 1000);
```

#### Receive using a callback

Alternatively, you can also register regular callbacks

```C++
// adding a callback
auto callback_handle = connection->addMessageCallback(
        [](const mav::Message& message) {
            std::cout << message.name() << std::endl;
        });

// adding a callback with an error handler
auto callback_handle = connection->addMessageCallback(
        [](const mav::Message& message) {
            std::cout << message.name() << std::endl;
        }, 
        [](const std::exception_ptr& exception) {
            std::cout << "Exception" << std::endl;
        });

// removing a callback
connection->removeMessageCallback(callback_handle);
```
