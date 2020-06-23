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

#include <prometheus/detail/builder.h>

#include "yaml-cpp/yaml.h"

#include "Mapper.hpp"

using namespace std;
using namespace dds::core::xtypes;
using namespace prometheus;

/*
* CONFIG_FILE as path to .yml file which specify how to make metrics
* The yml file should be in the root directory of build.
* Note: You have to run routing service from within build dir. 
*/
Mapper::Mapper(std::string configFile) {
    configFilename = "../";
    configFilename.append(configFile);
}

void Mapper::registerMetrics(std::shared_ptr<Registry> registry) {
    YAML::Node config = YAML::LoadFile(configFilename);
    
    METRIC_VARIANT temp = createFamily(counter, "call_on_data_available_total", "How many times this processor call on_data_available()",
        {{"label", "value"}}, registry);
    prometheus::Family<prometheus::Counter>* counter_family = boost::get<prometheus::Family<prometheus::Counter>*>(temp);
    prometheus::Counter& second_counter = counter_family->Add({{"processor", "1"}});
    metricsMap["call_on_data_available_total"] = counter_family;

    prometheus::Family<prometheus::Gauge>& gauge_family = BuildGauge()
                .Name("domainParticipant_process_statistics")
                .Help("Tell the current position of the squre shape")
                .Labels({{"job", "registerMetric"}})
                .Register(*registry);
    prometheus::Gauge& user_cpu_time = gauge_family.Add({{"process", "user_cpu_time"}});
    prometheus::Gauge& kernel_cpu_time = gauge_family.Add({{"process", "kernel_cpu_time"}});
    metricsMap["domainParticipant_process_statistics"] = &gauge_family;
}

/*
* Create Family (container of metrics) 
* and register it to REGISTRY
* Return: Family<T>* for T = Counter, Gauge, Histogram, or Summary
* and return boost::blank if fail
*/
METRIC_VARIANT Mapper::createFamily(metricTypes type, string name, string detail, 
                       const map<string, string>& labels, shared_ptr<Registry> registry) {
    
    switch(type){
        case counter:
            return &(BuildCounter().Name(name).Help(detail).Labels(labels).Register(*registry));
        case gauge:
            return &(BuildGauge().Name(name).Help(detail).Labels(labels).Register(*registry));
        case histogram:
            return &(BuildHistogram().Name(name).Help(detail).Labels(labels).Register(*registry));
        case summary:
            return &(BuildSummary().Name(name).Help(detail).Labels(labels).Register(*registry));
        default:
            return boost::blank();
    }
}

int Mapper::updateMetrics(const dds::core::xtypes::DynamicData& data) {
    // TODO-- function for metric retrival would be nice
    auto counter_fam = boost::get<prometheus::Family<Counter>*>(metricsMap["call_on_data_available_total"]);
    prometheus::Counter& counter = counter_fam->Add({{"processor", "1"}});
    counter.Increment();

    auto gauge_fam_ptr = boost::get<prometheus::Family<Gauge>*>(metricsMap["domainParticipant_process_statistics"]);

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
