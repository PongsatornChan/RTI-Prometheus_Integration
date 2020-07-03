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

/**
 *   Represent a YAML Node contains all nessesary information to 
 *   construct Family metrics with.
 */
struct FamilyConfig {
    /** 
     * family type (Counter, Gauge, Histogram, Summary)
     */
    prometheus::MetricType type;

    /**
     * name of family that will appear in prometheus
     */ 
    std::string name;

    /**
     * helpful description of this family, what it represents.
     */ 
    std::string help;

    /** 
     * path to targeted value in DDS Sample 
     * each level seperate by "."
     */ 
    string dataPath;

    /**
     * starter labels of this family
     * They will appear in all metrics of this family  
     */ 
    std::map<std::string, std::string> labels;

    /// number of metrics this family contains
    unsigned long numMetrics;

    /**
     * list of metric configuration to create metrics with
     */
    std::vector<MetricConfig*> metrics; 
    
    /**
     * @param I_TYPE type of this family, I_NAME name of this family,
     *        I_HELP helpful description of this family,
     *        I_LABELS starter labels,
     *        NUM number of metrics this family contained
     *        I_METRICS list of metrics this family contained  
     */ 
    FamilyConfig(MetricType iType, string iName, string iHelp, map<string, string> iLabels,
                unsigned long num, vector<MetricConfig*> iMetrics);

    ~FamilyConfig();
};      

/**
 * All possible type of Family
 */
typedef boost::variant<boost::blank, 
                       Family<Counter>*, 
                       Family<Gauge>*, 
                       Family<Histogram>*, 
                       Family<Summary>*> 
        Family_variant;

/**
 * All possible type of metric
 */
typedef boost::variant<boost::blank,
                       Counter*,
                       Gauge*,
                       Histogram*,
                       Summary*>
        Metric_variant;

/**
 * This class handle all mapping behavior of DDS-to-prometheus
 */
class Mapper {
    public:
        /**
         * Initlize Mapper
         * 
         * Note: You have to run routing service from within build dir.
         * The yml file should be in the root directory of build. 
         * 
         * @param CONFIG_FILE as path to .yml file which specify how to make metrics
         */
        Mapper(std::string configFile);

        ~Mapper();

        /** 
         *  Mapper will create a /metric based on config FILENAME 
         *  Then register it to the one and only REGISTRY 
         *  
         * @param REGISTRY to register Family to
         */
        void registerMetrics(std::shared_ptr<Registry> registry);

        /** 
         *  Uppon receiving samples (on_data_available) processor 
         *  should pass the sample to this function to update metric 
         *  
         * @param DATA the DDS sample
         * @return 1 if success, 0 otherwise
         */
        int updateMetrics(const dds::core::xtypes::DynamicData& data);

        /**
         *  Utility function to determine type of metric 
         *  based on sting given
         * 
         * @param TYPE string depicting type
         * @return MetricType (prometheus::Counter, Gauge, Histogram, Summary)
         */
        static MetricType whatType(std::string type);

        /**
         *  Utility function to retrive value from DATA sample
         *  using PATH. 
         * 
         * @param DATA data from topic sample, used DynamicData as a wrap.
         *        PATH path to target value, divided by ":" in each level
         *              left to right.  
         */
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

/**
 * visitor to Family_variant that add 
 * a metric to a giving family with LABELS 
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

/**
 * visitor to Family_variant that update metric uniquely 
 * identified by LABELS with VALUE
 */
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