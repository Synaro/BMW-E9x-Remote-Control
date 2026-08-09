void testFeatureCatalogHasStableCompleteIdentifiers() {
    const auto& catalog = bmw::remote::application::featureCatalog();
    CHECK(catalog.size() == 3U);

    for (std::size_t index = 0U; index < catalog.size(); ++index) {
        const auto& descriptor = catalog[index];
        CHECK(static_cast<std::size_t>(descriptor.id) == index);
        CHECK(descriptor.code != nullptr && descriptor.code[0] != '\0');
        CHECK(bmw::remote::application::findFeature(descriptor.id) ==
              &descriptor);
        CHECK(bmw::remote::application::findFeature(descriptor.code) ==
              &descriptor);
        for (std::size_t other = index + 1U; other < catalog.size(); ++other) {
            CHECK(std::string_view{descriptor.code} != catalog[other].code);
        }
    }

    CHECK(bmw::remote::application::findFeature("not_a_feature") == nullptr);
    CHECK(bmw::remote::application::findFeature(
              static_cast<FeatureId>(255U)) == nullptr);
}

void testFeatureRequestsDefaultOffAndRejectUnknownBits() {
    FeatureRequests requests{};
    CHECK(requests.mask() == 0U);
    CHECK(requests.valid());
    for (const auto& feature : bmw::remote::application::featureCatalog()) {
        CHECK(!requests.enabled(feature.id));
    }

    CHECK(requests.setEnabled(FeatureId::ColdEngineGuard, true));
    CHECK(requests.enabled(FeatureId::ColdEngineGuard));
    CHECK(requests.setEnabled(FeatureId::ColdEngineGuard, false));
    CHECK(!requests.enabled(FeatureId::ColdEngineGuard));
    CHECK(!requests.setEnabled(static_cast<FeatureId>(255U), true));
    CHECK(!FeatureRequests{std::uint64_t{1U} << 63U}.valid());
}

void testFeatureResolverSeparatesRequestCapabilityAndQualification() {
    FeatureRequests requests{};
    CHECK(requests.setEnabled(FeatureId::ColdEngineGuard, true));
    FeatureRuntimeContext context{};
    context.implementedFeatures = requests.mask();

    CHECK(bmw::remote::application::resolveFeature(
              FeatureRequests{}, FeatureId::ColdEngineGuard, context)
              .status == FeatureResolutionStatus::DisabledByUser);

    context.implementedFeatures = 0U;
    CHECK(bmw::remote::application::resolveFeature(
              requests, FeatureId::ColdEngineGuard, context)
              .status == FeatureResolutionStatus::NotImplemented);

    context.implementedFeatures = requests.mask();
    const auto missing = bmw::remote::application::resolveFeature(
        requests, FeatureId::ColdEngineGuard, context);
    CHECK(missing.status == FeatureResolutionStatus::MissingCapabilities);
    CHECK((missing.missingCapabilities &
           bmw::remote::application::featureCapabilityMask(
               FeatureCapability::VehicleStateRead)) != 0U);

    context.availableCapabilities =
        bmw::remote::application::featureCapabilityMask(
            FeatureCapability::VehicleStateRead);
    CHECK(bmw::remote::application::resolveFeature(
              requests, FeatureId::ColdEngineGuard, context)
              .status == FeatureResolutionStatus::SignalsUnqualified);

    context.vehicleSignalsQualified = true;
    CHECK(bmw::remote::application::resolveFeature(
              requests, FeatureId::ColdEngineGuard, context)
              .status == FeatureResolutionStatus::Available);
    context.target = FeatureExecutionTarget::Simulation;
    CHECK(bmw::remote::application::resolveFeature(
              requests, FeatureId::ColdEngineGuard, context)
              .status == FeatureResolutionStatus::Simulated);
}

TelemetryMonitorConfig simulatedTelemetryConfig() {
    TelemetryMonitorConfig config{};
    CHECK(config.requestedFeatures.setEnabled(FeatureId::ColdEngineGuard, true));
    CHECK(config.requestedFeatures.setEnabled(
        FeatureId::DpfRegenerationIndicator, true));
    CHECK(config.requestedFeatures.setEnabled(
        FeatureId::TransmissionOverheatAlert, true));
    config.runtime.target = FeatureExecutionTarget::Simulation;
    config.runtime.implementedFeatures = config.requestedFeatures.mask();
    config.runtime.availableCapabilities =
        bmw::remote::application::featureCapabilityMask(
            FeatureCapability::VehicleStateRead);
    config.runtime.vehicleSignalsQualified = true;
    return config;
}

void testTelemetryMonitorIsReadOnlyFeatureGatedAndEdgeTriggered() {
    VehicleState vehicle = safeAutomaticVehicle();
    vehicle.engineRpm = Observed<std::uint16_t>::fresh(2'500U);
    vehicle.coolantTemperatureC = Observed<std::int16_t>::fresh(60);
    vehicle.engineOilTemperatureC = Observed<std::int16_t>::fresh(55);
    vehicle.transmissionOilTemperatureC = Observed<std::int16_t>::fresh(111);
    vehicle.dpfRegenerationActive = Observed<bool>::fresh(true);

    TelemetryMonitor disabled{};
    const auto disabledReport = disabled.evaluate(vehicle);
    CHECK(disabledReport.coldEngineGuard == TelemetryConditionState::Disabled);
    CHECK(disabledReport.dpfRegeneration == TelemetryConditionState::Disabled);
    CHECK(disabledReport.transmissionOverheat ==
          TelemetryConditionState::Disabled);
    CHECK(disabledReport.alertCount == 0U);

    TelemetryMonitor monitor{simulatedTelemetryConfig()};
    const auto active = monitor.evaluate(vehicle);
    CHECK(active.coldEngineGuard == TelemetryConditionState::Active);
    CHECK(active.dpfRegeneration == TelemetryConditionState::Active);
    CHECK(active.transmissionOverheat == TelemetryConditionState::Active);
    CHECK(active.contains(TelemetryAlertType::ColdEngineHighRpm));
    CHECK(active.contains(TelemetryAlertType::DpfRegenerationStarted));
    CHECK(active.contains(TelemetryAlertType::TransmissionOverheat));

    const auto repeated = monitor.evaluate(vehicle);
    CHECK(repeated.alertCount == 0U);

    vehicle.engineRpm = Observed<std::uint16_t>::fresh(1'800U);
    vehicle.dpfRegenerationActive = Observed<bool>::fresh(false);
    vehicle.transmissionOilTemperatureC = Observed<std::int16_t>::fresh(104);
    const auto recovered = monitor.evaluate(vehicle);
    CHECK(recovered.coldEngineGuard == TelemetryConditionState::Normal);
    CHECK(recovered.dpfRegeneration == TelemetryConditionState::Normal);
    CHECK(recovered.transmissionOverheat == TelemetryConditionState::Normal);
    CHECK(recovered.contains(TelemetryAlertType::ColdEngineRecovered));
    CHECK(recovered.contains(TelemetryAlertType::DpfRegenerationStopped));
    CHECK(recovered.contains(
        TelemetryAlertType::TransmissionTemperatureRecovered));
}

void testTelemetryMonitorRequiresFreshSignalsAndUsesHysteresis() {
    TelemetryMonitor monitor{simulatedTelemetryConfig()};
    VehicleState vehicle = safeAutomaticVehicle();
    vehicle.engineRpm = Observed<std::uint16_t>::fresh(3'000U);
    vehicle.coolantTemperatureC = {};
    vehicle.engineOilTemperatureC = {};
    vehicle.transmissionOilTemperatureC = {};
    vehicle.dpfRegenerationActive = {};

    const auto unavailable = monitor.evaluate(vehicle);
    CHECK(unavailable.coldEngineGuard == TelemetryConditionState::Unavailable);
    CHECK(unavailable.dpfRegeneration == TelemetryConditionState::Unavailable);
    CHECK(unavailable.transmissionOverheat ==
          TelemetryConditionState::Unavailable);
    CHECK(unavailable.alertCount == 0U);

    vehicle.coolantTemperatureC = Observed<std::int16_t>::fresh(80);
    vehicle.engineOilTemperatureC = Observed<std::int16_t>::fresh(80);
    vehicle.transmissionOilTemperatureC = Observed<std::int16_t>::fresh(111);
    vehicle.dpfRegenerationActive = Observed<bool>::fresh(false);
    CHECK(monitor.evaluate(vehicle).transmissionOverheat ==
          TelemetryConditionState::Active);
    vehicle.transmissionOilTemperatureC = Observed<std::int16_t>::fresh(108);
    CHECK(monitor.evaluate(vehicle).transmissionOverheat ==
          TelemetryConditionState::Active);
    vehicle.transmissionOilTemperatureC = Observed<std::int16_t>::fresh(104);
    CHECK(monitor.evaluate(vehicle).transmissionOverheat ==
          TelemetryConditionState::Normal);

    TelemetryMonitorConfig invalidConfig = simulatedTelemetryConfig();
    invalidConfig.temperatureHysteresisC = 0U;
    TelemetryMonitor invalidMonitor{invalidConfig};
    CHECK(invalidMonitor.evaluate(vehicle).transmissionOverheat ==
          TelemetryConditionState::Unavailable);
}
