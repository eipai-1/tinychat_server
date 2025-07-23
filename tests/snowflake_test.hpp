#include <stdexcept>
#include <thread>
#include <chrono>
#include <vector>
#include <map>

#include "utils/snowflake.hpp"
#include "utils/config.hpp"

using SnowFlake = tcs::utils::SnowFlake;

namespace test {
class SnowFlakeTest {
public:
    SnowFlakeTest(u64 epoch) : own_epoch_(epoch) {}

    void multi_thread_test() {
        std::vector<std::vector<u64>> vv_id;
        vv_id.resize(6);
        std::vector<std::thread> threads;
        for (int i = 0; i < 4; i++) {
            threads.emplace_back([&vv_id, i]() {
                for (int j = 0; j < 10; j++) {
                    vv_id[i].push_back(SnowFlake::next_id());
                }
            });
        }

        for (auto &t : threads) {
            t.join();
        }

        for (auto &ids : vv_id) {
            for (const auto &id : ids) {
                u64 sequence_id = parse_id(id);
                std::cout << "-----" << std::endl;
            }
        }

        // std::map<u64, u64> ans_map;
        // for (const auto &ids : vv_id) {
        //     for (const auto &id : ids) {
        //         u64 sequence_id = parse_id(id);
        //         if (ans_map.find(sequence_id) == ans_map.end()) {
        //             ans_map[sequence_id] = 1;
        //         } else {
        //             ans_map[sequence_id]++;
        //         }
        //         // print_time_in_ms(id);
        //     }
        // }

        // u64 v_sum = 0;
        // std::cout << "In ans_map:" << std::endl;
        // for (auto const &[k, v] : ans_map) {
        //     std::cout << k << ": " << v << std::endl;
        //     v_sum += v;
        // }
        // std::cout << "v_sum=" << v_sum << std::endl;
    }

private:
    u64 own_epoch_;
    static constexpr u64 SERVICE_BITS = 10;
    static constexpr u64 SEQUENCE_BITS = 12;

    u64 parse_id(u64 id) {
        std::cout << "Parsed ID: " << id << std::endl;
        u64 sequence_id = id & ((1 << SEQUENCE_BITS) - 1);

        std::cout << "Sequence ID: " << sequence_id << std::endl;

        id >>= SEQUENCE_BITS;
        u64 service_id = id & ((1 << SERVICE_BITS) - 1);

        std::cout << "Service ID: " << service_id << std::endl;

        id >>= SERVICE_BITS;
        u64 timestamp = id + own_epoch_;
        print_time_in_ms(timestamp);

        return sequence_id;
    }

    void print_time_in_ms(u64 ms_since_epoch) {
        // 1. 将毫秒数转换为一个 chrono::milliseconds 的 duration 对象
        std::chrono::milliseconds duration(ms_since_epoch);

        // 2. 将这个 duration 转换为一个 system_clock 的 time_point 对象 (它本质上是UTC)
        std::chrono::system_clock::time_point time_point_utc(duration);

        // 3. 定义目标时区。使用标准的 IANA 时区名称是最稳妥的。
        //    "Asia/Shanghai", "Asia/Taipei", "Asia/Hong_Kong" 都是 UTC+8。
        const std::chrono::time_zone *target_zone = std::chrono::locate_zone("Asia/Shanghai");

        // 4. 使用 zoned_time 将 UTC 时间点与目标时区关联起来
        std::chrono::zoned_time zoned_time_utc8(target_zone, time_point_utc);

        // 5. 使用 std::format 格式化 zoned_time 对象
        //    %Z 会自动输出时区简称 (例如 CST)
        //    %z 会输出时区偏移 (例如 +0800)
        std::string formatted_time = std::format("{:%Y-%m-%d %H:%M:%S %Z}", zoned_time_utc8);

        std::cout << "Formatted (UTC+8): " << formatted_time << std::endl;
    }
};
}  // namespace test