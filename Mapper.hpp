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

/*
*   Represent a metric
*   It is always a part of a family 
*/
struct MetricConfig {
    /* 
    * path to targeted value in DDS Sample 
    * each level seperate by ":"
    */ 
    string dataPath;

    /* Data type of the target value */
    string dataType;

    /* 
    * labels that uniquely identify this metric from
    * all other metrics in its family
    */
    std::map<string, string> labels; 
    
    MetricConfig(string path, string type, map<string,string> inputLabels);
};

/*
*   Represent a YAML Node contains all nessesary information to 
*   construct Family metrics with.
*/
struct FamilyConfig {
    /* 
    * family type (Counter, Gauge, Histogram, Summary)
    */
    prometheus::MetricType type;

    /*
    * name of family that will appear in prometheus
    */ 
    std::string name;

    /*
    * helpful description of this family, what it represents.
    */ 
    std::string help;

    /*
    * starter labels of this family
    * They will appear in all metrics of this family  
    */ 
    std::map<std::string, std::string> labels;

    /* number of metrics this family contains */
    unsigned long numMetrics;

    /*
    * list of metric configuration to create metrics with
    */
    std::vector<MetricConfig*> metrics; 
    
    FamilyConfig(MetricType iType, string iName, string iHelp, map<string, string> iLabels,
                unsigned long num, vector<MetricConfig*> iMetrics);
};      

/*
* All possible type of Family
*/
typedef boost::variant<boost::blank, 
                       Family<Counter>*, 
                       Family<Gauge>*, 
                       Family<Histogram>*, 
                       Family<Summary>*> 
        Family_variant;

/*
* All possible type of metric
*/
typedef boost::variant<boost::blank,
                       Counter*,
                       Gauge*,
                       Histogram*,
                       Summary*>
        Metric_variant;

/*
* This class handle all mapping behavior of DDS-to-prometheus
*/
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

        /* 
        *  Utility function to determine type of metric 
        *  based on sting given
        */
        static MetricType whatType(std::string type);

        static double getData(const dds::core::xtypes::DynamicData& data, string path); 
    private:
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
        map<string, Family_variant> familyMap;

        /*
        * map that keeps track of configuration 
        * for easy access and keep YAML file abstract 
        * away from the logic of mapping.
        * Act as YAML file in a way
        */
        map<string, FamilyConfig*> configMap;

        /*
        * Create and register a family of METRIC_TYPE with name NAME,
        * helpful description of DETAIL, starter labels LABELS, and 
        * register it to REGISTRY
        */ 
        Family_variant createFamily(MetricType, string name, string detail, 
                        const map<string, string>& labels, shared_ptr<Registry> registry);
        Family_variant createFamily(FamilyConfig*, shared_ptr<Registry>);


};

/*
* visitor to Family_variant that add a metric to a giving family 
*/
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

class update_metric: public boost::static_visitor<bool> {
public:
    bool operator()( Family<prometheus::Counter>* operand) const;
    bool operator()( Family<prometheus::Gauge>* operand) const;
    bool operator()( Family<prometheus::Summary>* operand) const;
    bool operator()( Family<prometheus::Histogram>* operand) const;
    bool operator()( boost::blank operand) const
    { return false;}
    map<string, string> labels;
    double value;
};


#endif