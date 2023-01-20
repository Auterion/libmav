#include <iostream>
#include "MessageSet.h"
#include "BuiltInMessages.h"
#include "Network.h"
#include "Connection.h"
#include <array>


int main() {

    libmavlink::MessageSet m{"/home/thomas/projects/mavlink/message_definitions/v1.0/common.xml"};

    auto message = m.createMessage("PARAM_EXT_SET");

    message.set("target_system", 1);
    message.set("target_component", 3);
    message.set("param_id", "TEST_PARAM");
    message.set("param_value", "BLAVALUE");


    message["target_system"] = 1;
    message["param_value"][3] = 'f';
    message["param_value"];

    std::vector<int> b{1,2,3};

    message.set<std::vector<int>>("param_value", b);
    message["param_value"]  = b;

    std::array<float, 128> a = message["param_value"];
    std::vector<float> q = message["param_value"];


    message.get<std::array<char, 16>>("param_id");


    message.get<char>("param_value", 5);

    char c = message["param_value"][6];


    message.set({
        {"target_system", 1},
        {"target_component", 2},
        {"param_id", "TEST_PARAM"},
        {"param_value", "TESTVALUE"}
    });

    message.header().seq() = 1;
    message.header().msgId() = 15;


    message.get<int>("param_id");

    float x = message["param_id"];


    message.finalize(0, 1, 1);

    std::cout << serial_size  << std::endl;

    return 0;
}




