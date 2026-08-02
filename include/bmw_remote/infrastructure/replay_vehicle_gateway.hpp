#pragma once

#include <cstddef>
#include <cstdint>

#include "bmw_remote/domain/vehicle_state.hpp"
#include "bmw_remote/infrastructure/can_trace_replay.hpp"
#include "bmw_remote/infrastructure/ports.hpp"
#include "bmw_remote/infrastructure/vehicle_state_assembler.hpp"

namespace bmw::remote::infrastructure {

class ReplayVehicleGateway final : public VehicleGateway {
public:
    ReplayVehicleGateway(
        const CanFrame* frames,
        std::size_t frameCount,
        const CanFrameDecoder& decoder,
        SignalFreshnessConfig freshness = {}) noexcept;

    bool requestState() noexcept override;

    [[nodiscard]] bool setElapsedTime(std::uint32_t elapsedMs) noexcept;
    void reset() noexcept;

    [[nodiscard]] domain::VehicleState state() const noexcept;
    [[nodiscard]] constexpr ReplayBatch lastBatch() const noexcept { return lastBatch_; }
    [[nodiscard]] constexpr AssemblyStatistics statistics() const noexcept {
        return assembler_.statistics();
    }

private:
    CanTraceReplay replay_;
    VehicleStateAssembler assembler_;
    std::uint32_t elapsedMs_{0U};
    bool timeIsValid_{true};
    ReplayBatch lastBatch_{};
};

}  // namespace bmw::remote::infrastructure
