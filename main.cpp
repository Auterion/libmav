#include <iostream>
#include "mav/MessageSet.h"
#include "mav/Network.h"
#include "mav/Connection.h"
#include "mav/Serial.h"
#include "mav/TCP.h"
#include "mav/UDPPassive.h"
#include <array>
#include "mav/MessageFieldIterator.h"



int main() {
    mav::MessageSet message_set{"/Users/thomas/projects/mavlink/message_definitions/v1.0/common.xml"};

    message_set.addFromXMLString(R""""(
<mavlink>
    <messages>
        <message id="9912" name="TEMPERATURE_MEASUREMENT">
            <field type="float" name="temperature">The measured temperature in degress C</field>
        </message>
    </messages>
</mavlink>
)"""");
    auto message11 = message_set.create("TEMPERATURE_MEASUREMENT");
    message11["temperature"] = 13.3;
    message11.finalize(0, {1, 1});

    std::cout << "MAV_SYS_STATUS_SENSOR_SATCOM " << message_set.e("MAV_SYS_STATUS_SENSOR_SATCOM") << std::endl;

    //auto physical = mav::Serial("/dev/ttyACM0", 57600);
    //auto physical = mav::TCP("10.41.1.1", 5790);
    auto physical = mav::UDPPassive(14550);
    auto runtime = mav::NetworkRuntime({253, 1}, message_set, physical);

    auto connection = mav::Connection(message_set, {1, 1});
    runtime.addConnection(connection);
    int messages = 0;
    connection.addMessageCallback([&message_set, &messages](const mav::Message &message) {
        if (message.id() == message_set.idForMessage("HIGHRES_IMU")) {
            std::cout << "xacc: " << static_cast<float>(message["xacc"]) << std::endl;
//            for (auto &name : message.type()->fieldNames()) {
//                std::cout << "- " << name << std::endl;
//            }
        } else if (message.id() == message_set.idForMessage("PARAM_VALUE")) {
            std::cout << "PRAM_VALUE: " << static_cast<std::string>(message["param_id"]) << std::endl;
        }
        messages++;
    });

    uint64_t start = mav::millis();
    while((mav::millis() - start) < 1000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    };

    std::cout << "Received " << messages << " messages" << std::endl;

    auto message = connection.receive("HEARTBEAT");
    std::cout << "Received heartbeat..." << std::endl;



    auto exp = connection.expect("PARAM_VALUE");
    connection.send(message_set.create("PARAM_REQUEST_READ")({
            {"target_system", 1},
            {"target_component", 1},
            {"param_id", "SYS_AUTOSTART"},
            {"param_index", -1}
    }));
    auto response = connection.receive(exp);


    std::cout << "Got param value for " << static_cast<std::string>(response["param_id"])
        << " val: " << (response["param_value"].floatUnpack<int>()) << std::endl;




    while((mav::millis() - start) < 1000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    };
//    auto message = message_set.create("CHANGE_OPERATOR_CONTROL");
//

//
//    auto message12 = message_set.create("TEMPERATURE_MEASUREMENT")({
//                                                                           {"temperature", 13.3}
//                                                                   });
//
//
//    std::string s = message["passkey"];
//    std::cout << "PASSKEY BEFORE: " << s << std::endl;
//    message["passkey"] = "Hello world";
//
//    message.setFromString("passkey", "BLA");
//
//    std::cout << "PASSKEY AFTER: " << static_cast<std::string>(message["passkey"]) << std::endl;
//
//
//    message.set({
//                        {"passkey", "Init list test"}
//                });
//    std::cout << "PASSKEY INIT: " << static_cast<std::string>(message["passkey"]) << std::endl;
//
//    for (const auto &item : mav::FieldIterate(message)) {
//        std::cout << item.first << std::endl;
//    }
//
//
//    return 0;


//    connection.send({"HEARTBEAT", {
//
//    }});
//
//    auto response = connection.receive("HEARTBEAT");
//
//

//
//    auto a = message_set.createMessage("bla");
//
//    float q = a["bla"];
//    a["bla"] = 123;


//
//
//    auto message = message_set.createMessage("PARAM_EXT_SET");
//
//    message.set("target_system", 1);
//    message.set("target_component", 3);
//    message.set("param_id", "TEST_PARAM");
//    message.set("param_value", "BLAVALUE");
//
//
//    message["target_system"] = 1;
//    message["param_value"][3] = 'f';
//    message["param_value"];
//
//    std::vector<int> b{1,2,3};
//
//    message.set<std::vector<int>>("param_value", b);
//    message["param_value"]  = b;
//
//    std::array<float, 128> a = message["param_value"];
//    std::vector<float> q = message["param_value"];
//
//
//    message.get<std::array<char, 16>>("param_id");
//
//
//    message.get<char>("param_value", 5);
//
//    char c = message["param_value"][6];
//
//
//    message.set({
//        {"target_system", 1},
//        {"target_component", 2},
//        {"param_id", "TEST_PARAM"},
//        {"param_value", "TESTVALUE"}
//    });
//
//    message.header().seq() = 1;
//    message.header().msgId() = 15;
//
//
//    message.get<int>("param_id");
//
//    float x = message["param_id"];
//
//
//
//    auto message = message_set.createMessage("PARAM_EXT_SET");
//
//    message.set("target_system", 1);
//    message.set("target_component", 3);
//    message.set("param_id", "TEST_PARAM");
//    message.set("param_value", "BLAVALUE");
//
//
//    message["target_system"] = 1;
//    message["param_value"][3] = 'f';
//    message["param_value"];
//
//    std::vector<int> b{1,2,3};
//
//    message.set<std::vector<int>>("param_value", b);
//    message["param_value"]  = b;
//
////    std::array<float, 128> a = message["param_value"];
////    std::vector<float> q = message["param_value"];
//
//
//    message.get<std::array<char, 16>>("param_id");
//
//
//    message.get<char>("param_value", 5);
//
//    char cfs = message["param_value"][6];
//
//
//    message.set({
//        {"target_system", 1},
//        {"target_component", 2},
//        {"param_id", "TEST_PARAM"},
//        {"param_value", "TESTVALUE"}
//    });
//
//    message.header().seq() = 1;
//    message.header().msgId() = 15;
//
//
//    message.get<int>("param_id");
//
//    float y = message["param_id"];


    return 0;
}




