
#include "utils/snowflake.hpp"

namespace tcs {
namespace utils {
u64 SnowFlake::service_id_ = INIT_SERVICE_BITS;
u64 SnowFlake::sequence_id_ = 0;
i64 SnowFlake::last_timestamp_ = 0;
u64 SnowFlake::own_epoch_ = 0;
std::mutex SnowFlake::mtx_;
u64 SnowFlake::next_id() {
    std::lock_guard<std::mutex> lock(mtx_);

    if (service_id_ == INIT_SERVICE_BITS) {
        throw std::runtime_error("SnowFlake need to be initialized first!");
    }

    u64 time_stamp = time_gen();
    if (time_stamp < last_timestamp_) {
        throw std::runtime_error("Clock moved backwards. Refusing to generate id for " +
                                 std::to_string(time_stamp) + " milliseconds");
    }
    if (time_stamp == last_timestamp_) {
        sequence_id_++;
        if (sequence_id_ >= (1 << (SEQUENCE_BITS - 1))) {
            time_stamp = wait_till_next_ms();
            // throw std::runtime_error("Sequence overflow, please try again later.");
        }
    } else {
        sequence_id_ = 0;
    }
    last_timestamp_ = time_stamp;
    time_stamp -= own_epoch_;
    time_stamp <<= (SERVICE_BITS + SEQUENCE_BITS);
    u64 service_id = service_id_ << SEQUENCE_BITS;

    // 序列id用递增前的
    return u64(time_stamp | service_id | sequence_id_);
}
}  // namespace utils
}  // namespace tcs