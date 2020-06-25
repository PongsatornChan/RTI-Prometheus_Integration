#include <map>
#include <string>

#include <dds/core/corefwd.hpp>

#include <dds/core/Optional.hpp>
#include <dds/core/xtypes/DynamicData.hpp>
#include <dds/core/xtypes/StructType.hpp>
#include <rti/routing/processor/Processor.hpp>
#include <rti/routing/processor/ProcessorPlugin.hpp>

#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/summary.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/metric_type.h>

#include <prometheus/detail/builder.h>

#include "yaml-cpp/yaml.h"

#include "Mapper.hpp"

using namespace std;
using namespace dds::core::xtypes;
using namespace prometheus;

namespace YAML {
template<>
struct convert<map<string, string>> {
    static Node encode(const map<string, string> &map) {
        YAML::Node node;
        node = YAML::Load("{}");
        for (std::map<string, string>::const_iterator it = map.begin(); it != map.end(); ++it) {
            node[it->first] = it->second;
        }
        return node;
    }

    static bool decode(const Node &node, map<string, string> &map) {
        if (!node.IsMap()) {
            return false;
        }
        for (YAML::const_iterator it = node.begin(); it != node.end(); ++it) {
            map[it->first.as<string>()] = it->second.as<string>();
        }
        return true;
    }
};
}

//---------Mapper---------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------

/*
* CONFIG_FILE as path to .yml file which specify how to make metrics
* The yml file should be in the root directory of build.
* Note: You have to run routing service from within build dir. 
*/
Mapper::Mapper(std::string configFile) {
    configFilename = "../";
    configFilename.append(configFile);
}

/*
* Limitation of current implementation:
*   - Seqence
*   - Evolving Extendable type
*/
void Mapper::registerMetrics(std::shared_ptr<Registry> registry) {
    YAML::Node config = YAML::LoadFile(configFilename);
    YAML::Node family1 = config["Family"];
    string name = family1["name"].as<string>();
    MetricType type = whatType(family1["type"].as<string>());
    string help = family1["description"].as<string>();
    map<string, string> labelsMap = family1["labels"].as<map<string, string>>(); 
    
    Family_variant temp = createFamily(MetricType::Counter, 
                                    "call_on_data_available_total", 
                                    "How many times this processor call on_data_available()",
                                    {{"label", "value"}}, registry);
    // FIXME it doesn't for some reason.
    //boost::apply_visitor(add_metric(), temp, {{"processor", "1"}});
    metricsMap["call_on_data_available_total"] = temp;

    temp = createFamily(type, name, help, labelsMap, registry);
    //boost::apply_visitor(add_metric(), temp, {{"process", "user_cpu_time"}}); 
    //boost::apply_visitor(add_metric(), temp, {{"process", "kernel_cpu_time"}}); 
    metricsMap["domainParticipant_process_statistics"] = temp;
}

/*
* Create Family (container of metrics) 
* and register it to REGISTRY
* Return: Family<T>* for T = Counter, Gauge, Histogram, or Summary
* and return boost::blank if fail
*/
Family_variant Mapper::createFamily(MetricType type, string name, string detail, 
                       const map<string, string>& labels, shared_ptr<Registry> registry) {
    switch(type){
        case MetricType::Counter:
            return &(BuildCounter().Name(name).Help(detail).Labels(labels).Register(*registry));
        case MetricType::Gauge:
            return &(BuildGauge().Name(name).Help(detail).Labels(labels).Register(*registry));
        case MetricType::Histogram:
            return &(BuildHistogram().Name(name).Help(detail).Labels(labels).Register(*registry));
        case MetricType::Summary:
            return &(BuildSummary().Name(name).Help(detail).Labels(labels).Register(*registry));
        default:
            return boost::blank();
    }
}

int Mapper::updateMetrics(const dds::core::xtypes::DynamicData& data) {
    // TODO-- function for metric retrival would be nice
    // auto counter_fam = boost::get<prometheus::Family<Counter>*>(metricsMap["call_on_data_available_total"]);
    // prometheus::Counter& counter = counter_fam->Add({{"processor", "1"}});
    // counter.Increment();

    // auto gauge_fam_ptr = boost::get<prometheus::Family<Gauge>*>(metricsMap["domainParticipant_process_statistics"]);

    // prometheus::Gauge& user_cpu_time = gauge_fam_ptr->Add({{"process", "user_cpu_time"}});
    // prometheus::Gauge& kernel_cpu_time = gauge_fam_ptr->Add({{"process", "kernel_cpu_time"}});
    // // TODO-- function for data retrival would be nice too
    // double var = (double) data.value<DynamicData>("process")
    //             .value<DynamicData>("user_cpu_time")
    //             .value<int64_t>("sec");
    // user_cpu_time.Set(var);
    // var = (double) data.value<DynamicData>("process")
    //             .value<DynamicData>("kernel_cpu_time")
    //             .value<int64_t>("sec");
    // kernel_cpu_time.Set(var);

    return 1;
}

//--- end Mapper -----------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------

MetricType whatType(std::string type) {
    if (boost::iequals(type, "counter")) {
        return MetricType::Counter;
    } else if (boost::iequals(type, "gauge")) {
        return MetricType::Gauge;
    } else if (boost::iequals(type, "histogram")) {
        return MetricType::Histogram;
    } else if (boost::iequals(type, "summary")) {
        return MetricType::Summary;
    } else {
        return MetricType::Untyped;
    }
}

bool add_metric::operator()( Family<prometheus::Counter>* operand, map<string, string> labels) const {
    try {
        operand->Add(labels);
        return true;
    } catch(const std::exception& e) {
        return false;
    }
}

bool add_metric::operator()( Family<prometheus::Gauge>* operand, map<string, string> labels ) const {
    try {
        operand->Add(labels);
        return true;
    } catch(const std::exception& e) {
        return false;
    }
}
bool add_metric::operator()( Family<prometheus::Summary>* operand, map<string, string> labels ) const {
    try {
        auto quantile = Summary::Quantiles{{0.5, 0.05}, {0.7, 0.03}, {0.90, 0.01}};
        operand->Add(labels, quantile);
        return true;
    } catch(const std::exception& e) {
        return false;
    }
}
bool add_metric::operator()( Family<prometheus::Histogram>* operand, map<string, string> labels ) const {
    try {
        operand->Add(labels, prometheus::Histogram::BucketBoundaries{0, 1, 2});
        return true;
    } catch(const std::exception& e) {
        return false;
    }
}

