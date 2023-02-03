//
// Created by thomas on 01.02.23.
//
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "mav/MessageSet.h"
#include "mav/Message.h"
#include "mav/MessageFieldIterator.h"
#include "mav/Network.h"
#include "mav/TCP.h"
#include "mav/UDPPassive.h"
#include "mav/Serial.h"
