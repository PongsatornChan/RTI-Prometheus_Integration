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
using namespace boost;

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

//---------FamilyConfig---------------------------------------------------------------------------------
FamilyConfig::FamilyConfig(MetricType iType, string iName, string iHelp, 
string iDataPath, map<string, string> iLabels, unsigned long num) : 
            type (iType),
            name (iName),
            help (iHelp),
            labels (iLabels),
            numMetrics (num),
            dataPath (iDataPath)
{

}

FamilyConfig::~FamilyConfig() {}

//------------------------------------------------------------------------------------------------
//---------Mapper---------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------

Mapper::Mapper(std::string configFile) {
    string configFilename = "../";
    configFilename.append(configFile);
    YAML::Node topic = YAML::LoadFile(configFilename);
    YAML::Node config = topic["Topic"];
    if (config.IsNull()) {
        throw YAML::BadFile(configFilename);
    }
    
    for (YAML::const_iterator it = config["Metrics"].begin(); it != config["Metrics"].end(); ++it) {
        string name = "rti_dds_monitoring_domainParticipantEntityStatistics";
        string dataPath = it->second["data"].as<string>();
        name.append(boost::replace_all_copy(dataPath, ".", "_"));

        MetricType type;
        if (it->second["type"]) {
            type = whatType(it->second["type"].as<string>());
        } else {
            type = MetricType::Gauge;
        }

        string help = it->second["description"].as<string>();
        // TODO-- get key member before getting sample!?
        map<string, string> labelsMap = {{"key", "Member ID"}}; 

        FamilyConfig* famConfig = new FamilyConfig(type, name, help, dataPath, labelsMap, 0);
        configMap[name] = famConfig;
    }
}

/**
 * Deallocate metrics_vec (vector<MetricConfig*>)
 * configMap (map<string, FamilyConfig*>)
 */ 
Mapper::~Mapper() {
     for (map<string, FamilyConfig*>::iterator it = configMap.begin(); it != configMap.end(); ++it) {
         delete it->second;
     }
     configMap.clear();
}

/*
* Limitation of current implementation:
*   - Seqence
*   - Evolving Extendable type
*/
void Mapper::registerMetrics(std::shared_ptr<Registry> registry) {
    // call_on_data_avaialable_total is default metric
    Family_variant temp = createFamily(MetricType::Counter, 
                                    "call_on_data_available_total", 
                                    "How many times this processor call on_data_available()",
                                    {{"Test", "on_data_available"}}, registry);
    add_metric adder;
    // TODO-- get topic name this processor associate with
    adder.labels = {{"Topic", "topic name"}};
    boost::apply_visitor(adder, temp);
    familyMap["call_on_data_available_total"] = temp;

    for (map<string, FamilyConfig*>::const_iterator cit = configMap.begin();
        cit != configMap.end(); ++cit) {
        
        FamilyConfig* fam = cit->second;
        // DEBUG
        std::cout << "fam->name: " << fam->name << endl;
        std::cout << "fam->numMetrics: " << fam->numMetrics << endl;
        temp = createFamily(fam, registry);
        familyMap[fam->name] = temp;
    }
}

/*
* Create Family (container of metrics) 
* and register it to REGISTRY
* Return: Family<T>* for T = Counter, Gauge, Histogram, or Summary
* and return boost::blank if fail
*/
Family_variant Mapper::createFamily(MetricType type, string name, string detail, 
                       const map<string, string>& labels, std::shared_ptr<Registry> registry) {
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

Family_variant Mapper::createFamily(FamilyConfig* famConfig, std::shared_ptr<Registry> registry) {
    return createFamily(famConfig->type, famConfig->name, famConfig->help, famConfig->labels, registry);
}

/* 
*  Utility function to determine type of metric 
*  based on sting TYPE given
*  Return: MetricType 
*/
MetricType Mapper::whatType(std::string type) {
    if (boost::iequals(type, "counter")) {
        return MetricType::Counter;
    } else if (boost::iequals(type, "gauge")) {
        return MetricType::Gauge;
    } else if (boost::iequals(type, "histogram")) {
        return MetricType::Histogram;
    } else if (boost::iequals(type, "summary")) {
        return MetricType::Summary;
    } else {
        string msg = type + " does not match any metric types.";
        msg.append("\nUsing Gauge instead.");
        std::cout << msg << endl;
        return MetricType::Gauge;
    }
}

/* 
*  Utility function to get data from a sample
*  Return: double 
*/
double Mapper::getData(const dds::core::xtypes::DynamicData& data, string path) {
    std::vector<string> results;
    boost::split(results, path, [](char c){return c == '.';});
    DynamicData temp = data;
    for (int i = 0; i < results.size(); ++i) {
        if (i == results.size()-1) {
            return (double) temp.value<int64_t>(results[i]);
        }
        temp = temp.value<DynamicData>(results[i]);
    }
    throw new std::runtime_error("can't find value in " + path);
    return 0;
}


int Mapper::updateMetrics(const dds::core::xtypes::DynamicData& data, 
const dds::sub::SampleInfo& info) {
    Family<Counter>* counter_fam = 
        boost::get<Family<Counter>*>(familyMap["call_on_data_available_total"]);
    Counter& counter = counter_fam->Add({{"Topic", "topic name"}});
    counter.Increment();

    for (map<string, FamilyConfig*>::const_iterator cit = configMap.begin(); cit != configMap.end(); ++cit) {
       
        string dataPath = cit->second->dataPath;
        double var;
        try{
            var = Mapper::getData(data, dataPath);
            // DEBUG
            std::cout << "getData with " << dataPath;
            std::cout << " return " << var << endl;
        } catch(std::exception& e) {
            std::cout << "getData error: set to 0" << endl; 
            var = 0;
        }
        update_metric updater;
        updater.value = var;
        std::stringstream ss;
        ss << info.instance_handle();
        updater.labels = {{"Instance_ID", ss.str()}} ;
        boost::apply_visitor(updater, familyMap[cit->first]);
    }

    return 1;
}
//--- end Mapper -----------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------

//--- add_metric -----------------------------------------------------------------------------------------------------
bool add_metric::operator()( Family<prometheus::Counter>* operand) const {
    try {
        operand->Add(labels);
        return true;
    } catch(const std::exception& e) {
        return false;
    }
}

bool add_metric::operator()( Family<prometheus::Gauge>* operand) const {
    try {
        operand->Add(labels);
        return true;
    } catch(const std::exception& e) {
        return false;
    }
}
bool add_metric::operator()( Family<prometheus::Summary>* operand) const {
    try {
        auto quantile = Summary::Quantiles{{0.5, 0.05}, {0.7, 0.03}, {0.90, 0.01}};
        operand->Add(labels, quantile);
        return true;
    } catch(const std::exception& e) {
        return false;
    }
}
bool add_metric::operator()( Family<prometheus::Histogram>* operand) const {
    try {
        operand->Add(labels, prometheus::Histogram::BucketBoundaries{0, 1, 2});
        return true;
    } catch(const std::exception& e) {
        return false;
    }
}

//--- Update_metric ----------------------------------------------------------------------------
bool update_metric::operator()( Family<prometheus::Counter>* operand) const {
    try {
        prometheus::Counter& counter = operand->Add(labels);
        counter.Increment();
        return true;
    } catch(const std::exception& e) {
        return false;
    }
}

bool update_metric::operator()( Family<prometheus::Gauge>* operand) const {
    try {
        prometheus::Gauge& gauge = operand->Add(labels);
        gauge.Set(value);
        return true;
    } catch(const std::exception& e) {
        return false;
    }
}
bool update_metric::operator()( Family<prometheus::Summary>* operand) const {
    try {
        auto quantile = Summary::Quantiles{{0.5, 0.05}, {0.7, 0.03}, {0.90, 0.01}};
        operand->Add(labels, quantile);
        return true;
    } catch(const std::exception& e) {
        return false;
    }
}
bool update_metric::operator()( Family<prometheus::Histogram>* operand) const {
    try {
        operand->Add(labels, prometheus::Histogram::BucketBoundaries{0, 1, 2});
        return true;
    } catch(const std::exception& e) {
        return false;
    }
}

// TODO StatisticVariable only need mean
