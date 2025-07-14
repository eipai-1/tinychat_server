#pragma once

#include <chrono>
#include <stdexcept>
#include <thread>
#include <iostream>
#include <mutex>

#include "utils/types.hpp"

namespace tcs {
namespace utils {
class SnowFlake {
public:
    static void init(u64 service_id, u64 own_epoch) {
        if (service_id_ != INIT_SERVICE_BITS) {
            throw std::runtime_error("SnowFlake has already been initialized. ");
        }
        if (service_id >= INIT_SERVICE_BITS) {
            throw std::runtime_error("SnowFlake must be inited with service bits <= 1023 and >= 0");
        }
        service_id_ = service_id;
        last_timestamp_ = time_gen();
        own_epoch_ = own_epoch;
    }

    static u64 next_id();

    // static void parse_id(u64 id) {
    //     std::cout << "Parsed ID: " << id << std::endl;
    //     u64 sequence_id = id & ((1 << SEQUENCE_BITS) - 1);

    //    std::cout << "Sequence ID: " << sequence_id << std::endl;

    //    id >>= SEQUENCE_BITS;
    //    u64 service_id = id & ((1 << SERVICE_BITS) - 1);

    //    std::cout << "Service ID: " << service_id << std::endl;

    //    id >>= SERVICE_BITS;
    //    u64 timestamp = id + own_epoch_;
    //    print_time_in_ms(timestamp);
    //}

    // static void print_time_in_ms(u64 ms_since_epoch) {
    //     // 1. 将毫秒数转换为一个 chrono::milliseconds 的 duration 对象
    //     std::chrono::milliseconds duration(ms_since_epoch);

    //    // 2. 将这个 duration 转换为一个 system_clock 的 time_point 对象 (它本质上是UTC)
    //    std::chrono::system_clock::time_point time_point_utc(duration);

    //    // 3. 定义目标时区。使用标准的 IANA 时区名称是最稳妥的。
    //    //    "Asia/Shanghai", "Asia/Taipei", "Asia/Hong_Kong" 都是 UTC+8。
    //    const std::chrono::time_zone* target_zone = std::chrono::locate_zone("Asia/Shanghai");

    //    // 4. 使用 zoned_time 将 UTC 时间点与目标时区关联起来
    //    std::chrono::zoned_time zoned_time_utc8(target_zone, time_point_utc);

    //    // 5. 使用 std::format 格式化 zoned_time 对象
    //    //    %Z 会自动输出时区简称 (例如 CST)
    //    //    %z 会输出时区偏移 (例如 +0800)
    //    std::string formatted_time = std::format("{:%Y-%m-%d %H:%M:%S %Z}", zoned_time_utc8);

    //    std::cout << "Formatted (UTC+8): " << formatted_time << std::endl;
    //}

private:
    static constexpr u64 INIT_SERVICE_BITS = 1024;
    static constexpr u64 SERVICE_BITS = 10;
    static constexpr u64 SEQUENCE_BITS = 12;

    // 当前服务的id
    static u64 service_id_;
    // 上一个生成id的时间戳
    static i64 last_timestamp_;
    static u64 own_epoch_;
    // 序列号
    static u64 sequence_id_;
    static std::mutex mtx_;

    static u64 time_gen() {
        auto duration = std::chrono::system_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

    static u64 wait_till_next_ms() {
        auto now = time_gen();
        while (time_gen() <= now) {
            std::this_thread::yield();
            now = time_gen();
        }
        return now;
    }
};
}  // namespace utils
}  // namespace tcs