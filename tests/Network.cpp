//
// Created by thomas on 01.02.23.
//


#include "doctest.h"
#include "include/mav/Message.h"
#include "include/mav/Network.h"

using namespace mav;


class DummyInterface : public NetworkInterface {
public:
    void close() const override {}
    void send(const uint8_t *data, uint32_t size) override {}
    void receive(uint8_t *destination, uint32_t size) override {}
};





TEST_CASE("Message set creation") {

}