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
FamilyConfig::FamilyConfig(MetricType iType, string iName, string iHelp, map<string, string> iLabels,
                unsigned long num, vector<MetricConfig*> iMetrics) : 
            type (iType),
            name (iName),
            help (iHelp),
            labels (iLabels),
            numMetrics (num),
            metrics (iMetrics) 
{

}

//---------FamilyConfig---------------------------------------------------------------------------------
MetricConfig::MetricConfig(string path, string type, map<string,string> inputLabels) : 
    dataPath (path),
    dataType (type),
    labels (inputLabels)
{

}

//------------------------------------------------------------------------------------------------
//---------Mapper---------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------

Mapper::Mapper(std::string configFile) {
    string configFilename = "../";
    configFilename.append(configFile);
    YAML::Node config = YAML::LoadFile(configFilename);
    if (config.IsNull()) {
        throw YAML::BadFile(configFilename);
    }

    for (YAML::const_iterator it = config.begin(); it != config.end(); ++it) {
        string name = it->second["name"].as<string>();
        MetricType type = whatType(it->second["type"].as<string>());
        string help = it->second["description"].as<string>();
        map<string, string> labelsMap = it->second["labels"].as<map<string, string>>(); 

        YAML::Node metrics = it->second["metrics"];
        unsigned long num = metrics.size();
        std::vector<MetricConfig*> metrics_vec;
        if (metrics.IsMap() && num > 0) {
            for (YAML::const_iterator me = metrics.begin(); me != metrics.end(); ++me) {
                string dataPath = me->second["data"].as<string>();
                string dataType = me->second["type"].as<string>();
                map<string, string> metricLabels = me->second["labels"].as<map<string, string>>();
                // TODO-- Deallocation
                MetricConfig* metricConfig = new MetricConfig(dataPath, dataType, metricLabels); 
                metrics_vec.push_back(metricConfig);
            }
        } else{
            // TODO-- should push to logging but I have no idea how
            std::cout << "YAML config: Did not specify metrics or wrong format\n";
            num = 0;
        }
        // TODO-- Deallocation
        FamilyConfig* famConfig = new FamilyConfig(type, name, help, labelsMap, num, metrics_vec);
        configMap[name] = famConfig;
    }
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
                                    {{"label", "value"}}, registry);
    add_metric adder;
    adder.labels = {{"processor", "1"}};
    boost::apply_visitor(adder, temp);
    familyMap["call_on_data_available_total"] = temp;

    for (map<string, FamilyConfig*>::const_iterator cit = configMap.begin();
        cit != configMap.end(); ++cit) {
        
        FamilyConfig* fam = cit->second;
        std::cout << "fam->name: " << fam->name << endl;
        std::cout << "fam->numMetrics: " << fam->numMetrics << endl;
        temp = createFamily(fam, registry);
        for (int i = 0; i < fam->numMetrics; ++i) {
            adder.labels = fam->metrics[i]->labels;
            boost::apply_visitor(adder, temp); 
        }
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

double Mapper::getData(const dds::core::xtypes::DynamicData& data, string path) {
    std::vector<string> results;
    boost::split(results, path, [](char c){return c == ':';});
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


int Mapper::updateMetrics(const dds::core::xtypes::DynamicData& data) {
    Family<Counter>* counter_fam = 
        boost::get<Family<Counter>*>(familyMap["call_on_data_available_total"]);
    Counter& counter = counter_fam->Add({{"processor", "1"}});
    counter.Increment();

    for (map<string, FamilyConfig*>::const_iterator cit = configMap.begin(); cit != configMap.end(); ++cit) {
        for (int i = 0; i < cit->second->metrics.size(); ++i) {
            MetricConfig* me_ptr = cit->second->metrics[i];
            double var;
            try{
                var = Mapper::getData(data, me_ptr->dataPath);
                // DEBUG
                std::cout << "getData with " << me_ptr->dataPath;
                std::cout << " return " << var << endl;
            } catch(std::exception& e) {
                std::cout << "getData error: set to 0" << endl; 
                var = 0;
            }
            update_metric updater;
            updater.value = var;
            updater.labels = me_ptr->labels;
            boost::apply_visitor(updater, familyMap[cit->first]);
        }
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
