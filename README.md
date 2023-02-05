# libmav

The better mavlink library.

Libmav is a library for interacting with [mavlink](https://mavlink.io) systems. 
It provides message serialization / deserialization, code to interact with physical 
networks and several of the mavlink internal protocols.

### What's the advantage?

There are many mavlink libraries out there. The main advantages of this library over
the others are:

- Runtime defined message set. No need to recompile on message set change
- Native python bindings for C++ code. Faster than pure python
- Header-only, no dependencies, C++ 17, >90% test coverage

## How to install

There are several ways to get it:

Install globally on your system
```bash
cmake .
sudo make install
```
Since the library is header only, you only need the library on the build system.

You can also include the library as a submodule in your project.

## Getting started

### Loading a message set

In libmav, the message set is runtime defined and loaded from XML files.
Any of the upstream XML files will work. The function also loads dependent XML files
recursively.
```C++
auto message_set = libmav::MessageSet("/path/to/common.xml");
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
- TCP
- UDP Passive *Respond to anybody that already sends us data*
- UDP Active *Initiate sending data to somebody, only accept data from there*

Libmav does not do any threading, except for the `NetworkRuntime` class.
The `NetworkRuntime` class spawns a single thread to drive the receive end
of a connection.

```C++
// Create interface for physical network
auto physical = libmav::TCP("<ip>", 14550);

// Create a network runtime with a receive thread. Specify
// our own system id / component id to be used for outgoing messages
auto runtime = libmav::NetworkRuntime({253, 1}, message_set, physical);

// Add a connection
auto connection = libmav::Connection(message_set);
runtime.addConnection(connection);
```

The network runtime will throw `libmav::NetworkError` if network connection fails.

