/*
*   Author: Pongsatorn Chanpanichravee
*   The purpose of this class is to handle converting DDS topic to /metrics
*   based on YAML mapping configuration. 
*   This class is writen as a part of Prometheus Integration Summer Project
*   with RTI 
*/
#ifndef MAPPER_HPP
#define MAPPER_HPP

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

#include "yaml-cpp/yaml.h"

#include <variant>

#include "boost/variant.hpp"
#include "boost/any.hpp"
#include "boost/algorithm/string.hpp"

using namespace std;
using namespace prometheus;

typedef boost::variant<boost::blank, 
                       Family<Counter>*, 
                       Family<Gauge>*, 
                       Family<Histogram>*, 
                       Family<Summary>*> 
        Family_variant;

typedef boost::variant<boost::blank,
                       Counter*,
                       Gauge*,
                       Histogram*,
                       Summary*>
        Metric_variant;

class Mapper {
    public:
        Mapper(std::string configFile);

        /* 
        *  Mapper will create a /metric based on config FILENAME 
        *  Then register it to the one and only REGISTRY 
        */
        void registerMetrics(std::shared_ptr<Registry> registry);

        /* 
        *  Uppon receiving samples (on_data_available) processor 
        *  should pass the sample to this function to update metric 
        *  Return: 1 if success, 0 otherwise
        */
        int updateMetrics(const dds::core::xtypes::DynamicData& data);

    private:
        YAML::Node config;

        /*
        * map which keeps track of metric Family created
        * KEY: name of the given metric as a key, 
        *   defined by user via yaml configuration file
        * VALUE: boost::variant hold prometheus::Family<> *
        *   the reason for using boost::variant is to hold 
        *   any type of Family<Counter, Gauge, Histogram, Summary>* 
        *   based on ymal file.
        *   NOTE: boost::vairant instead of std::variant for older c++ version
        */
        map<string, Family_variant> metricsMap;

        Family_variant createFamily(MetricType, string, string, 
                        const map<string, string>&, shared_ptr<Registry>);
};

struct MetricConfig {
    std::string dataPath;
    std::string dataType;
    std::map<std::string, std::string> labels; 
};

struct FamilyConfig {
    std::string name;
    std::string help;
    prometheus::MetricType type;
    std::map<std::string, std::string> labels;
    MetricConfig* metrics; 
};

class add_metric: public boost::static_visitor<bool> {
public:
    bool operator()( Family<prometheus::Counter>* operand) const;
    bool operator()( Family<prometheus::Gauge>* operand) const;
    bool operator()( Family<prometheus::Summary>* operand) const;
    bool operator()( Family<prometheus::Histogram>* operand) const;
    bool operator()( boost::blank operand) const
    { return false;}
    map<string, string> labels;
};

MetricType whatType(std::string type);


#endif