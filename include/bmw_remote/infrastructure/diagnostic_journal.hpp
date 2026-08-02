#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "bmw_remote/application/controller.hpp"

namespace bmw::remote::infrastructure {

enum class DiagnosticRecordType : std::uint8_t {
    CommandReceived,
    StateTransition,
    RequestRejected,
    FaultEntered,
    InfrastructureFailure,
    SafingFailure,
};

enum class DiagnosticReason : std::uint8_t {
    None,
    RemoteStartDisabled,
    ProfileNotReady,
    SafetyPolicy,
    InvalidState,
    UnsafeReset,
};

struct DiagnosticRecord final {
    std::uint32_t sequence{0U};
    std::uint32_t timestampMs{0U};
    std::uint16_t safetyReasons{0U};
    DiagnosticRecordType type{DiagnosticRecordType::CommandReceived};
    application::EventType trigger{application::EventType::VehicleStateUpdated};
    application::ControllerState previousState{application::ControllerState::Idle};
    application::ControllerState state{application::ControllerState::Idle};
    application::FaultCode fault{application::FaultCode::None};
    DiagnosticReason reason{DiagnosticReason::None};
    std::uint8_t profileReasons{0U};
};

static_assert(
    sizeof(DiagnosticRecord) <= 24U,
    "Diagnostic records must remain compact for the embedded target");

class DiagnosticJournal final {
public:
    static constexpr std::size_t MaximumRecords = 32U;

    void observe(
        application::Event event,
        const application::Decision& decision,
        std::uint32_t timestampMs) noexcept;

    void recordInfrastructureFailure(
        application::EventType trigger,
        application::ControllerState state,
        application::FaultCode fault,
        std::uint32_t timestampMs) noexcept;

    void recordSafingFailure(
        application::ControllerState state,
        application::FaultCode fault,
        std::uint32_t timestampMs) noexcept;

    [[nodiscard]] bool read(
        std::size_t chronologicalIndex,
        DiagnosticRecord& record) const noexcept;

    void clear() noexcept;

    [[nodiscard]] constexpr std::size_t size() const noexcept {
        return size_;
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return MaximumRecords;
    }

    [[nodiscard]] constexpr std::uint32_t overwrittenCount() const noexcept {
        return overwrittenCount_;
    }

private:
    void append(DiagnosticRecord record) noexcept;
    void recordFailure(
        DiagnosticRecordType type,
        application::EventType trigger,
        application::ControllerState state,
        application::FaultCode fault,
        std::uint32_t timestampMs) noexcept;

    std::array<DiagnosticRecord, MaximumRecords> records_{};
    std::size_t oldestIndex_{0U};
    std::size_t size_{0U};
    std::uint32_t nextSequence_{1U};
    std::uint32_t overwrittenCount_{0U};
};

[[nodiscard]] const char* toString(DiagnosticRecordType type) noexcept;
[[nodiscard]] const char* toString(DiagnosticReason reason) noexcept;

}  // namespace bmw::remote::infrastructure
