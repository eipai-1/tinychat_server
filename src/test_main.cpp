#include <thread>
#include <iostream>
#include <semaphore>
#include <vector>
#include <chrono>
#include <memory>

#include "utils/config.hpp"
#include "snowflake_test.hpp"

using AppConfig = tcs::utils::AppConfig;

void init() {
    AppConfig::init("../../doc/config.ini");
    SnowFlake::init(AppConfig::get().server().service_id(),
                    AppConfig::get().server().custom_epoch());
}

int main(int argc, char *argv[]) {
    try {
        init();
        test::SnowFlakeTest snowflake(AppConfig::get().server().custom_epoch());
        snowflake.multi_thread_test();
    } catch (std::exception &e) {
        std::cerr << "Excpetion in main: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}