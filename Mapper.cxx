#include <map>
#include <string>

#include <dds/core/corefwd.hpp>

#include <dds/core/Optional.hpp>
#include <dds/core/xtypes/DynamicData.hpp>
#include <dds/core/xtypes/StructType.hpp>
#include <rti/routing/processor/Processor.hpp>
#include <rti/routing/processor/ProcessorPlugin.hpp>

#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

#include "yaml-cpp/yaml.h"

#include "Mapper.hpp"

using namespace dds::core::xtypes;
using namespace prometheus;

Mapper::Mapper(std::string configFile) {
    filename = configFile;
}

void Mapper::registerMetric(std::shared_ptr<Registry> registry) {
    prometheus::Family<prometheus::Counter>& counter_family = BuildCounter()
                .Name("call_on_data_available_total")
                .Help("How many times this processor call on_data_available()")
                .Labels({{"label", "value"}})
                .Register(*registry);
    prometheus::Counter& second_counter = counter_family.Add({{"processor", "1"}});
    metricsMap["call_on_data_available_total"] = &counter_family;

    prometheus::Family<prometheus::Gauge>& gauge_family = BuildGauge()
                .Name("domainParticipant_process_statistics")
                .Help("Tell the current position of the squre shape")
                .Labels({{"job", "RTI_Shape"}})
                .Register(*registry);
    prometheus::Gauge& user_cpu_time = gauge_family.Add({{"process", "user_cpu_time"}});
    prometheus::Gauge& kernel_cpu_time = gauge_family.Add({{"process", "kernel_cpu_time"}});
    metricsMap["doaminParticipant_process_statistics"] = &gauge_family;
}

int Mapper::updateMetric(dds::core::xtypes::DynamicData data) {

    return 1;
}
