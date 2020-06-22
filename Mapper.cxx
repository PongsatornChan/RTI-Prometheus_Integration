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
                .Labels({{"job", "registerMetric"}})
                .Register(*registry);
    prometheus::Gauge& user_cpu_time = gauge_family.Add({{"process", "user_cpu_time"}});
    prometheus::Gauge& kernel_cpu_time = gauge_family.Add({{"process", "kernel_cpu_time"}});
    metricsMap["domainParticipant_process_statistics"] = &gauge_family;
}

int Mapper::updateMetric(const dds::core::xtypes::DynamicData& data) {
    // TODO-- function for metric retrival would be nice
    auto counter_fam = boost::any_cast<prometheus::Family<Counter>*>(metricsMap["call_on_data_available_total"]);
    prometheus::Counter& counter = counter_fam->Add({{"processor", "1"}});
    counter.Increment();

   
    auto gauge_fam_ptr = boost::any_cast<prometheus::Family<Gauge>*>(metricsMap["domainParticipant_process_statistics"]);

    prometheus::Gauge& user_cpu_time = gauge_fam_ptr->Add({{"process", "user_cpu_time"}});
    prometheus::Gauge& kernel_cpu_time = gauge_fam_ptr->Add({{"process", "kernel_cpu_time"}});
    // TODO-- function for data retrival would be nice too
    double var = (double) data.value<DynamicData>("process")
                .value<DynamicData>("user_cpu_time")
                .value<int64_t>("sec");
    user_cpu_time.Set(var);
    var = (double) data.value<DynamicData>("process")
                .value<DynamicData>("kernel_cpu_time")
                .value<int64_t>("sec");
    kernel_cpu_time.Set(var);

    return 1;
}
