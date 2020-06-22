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
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

#include "yaml-cpp/yaml.h"

#include <variant>

#include "boost/variant.hpp"
#include "boost/any.hpp"

using namespace std;
using namespace prometheus;

typedef boost::variant<Family<Counter>, Family<Gauge>, Family<Histogram>, Family<Summary>> METRIC_VARIANT;


class Mapper {
    public:
        Mapper(std::string configFile);

        /* 
        *  Mapper will create a /metric based on config FILENAME 
        *  Then register it to the one and only REGISTRY 
        */
        void registerMetric(std::shared_ptr<Registry> registry);

        /* 
        *  Uppon receiving samples (on_data_available) processor 
        *  should pass the sample to this function to update metric 
        *  Return: 1 if success, 0 otherwise
        */
        int updateMetric(const dds::core::xtypes::DynamicData& data);

    private:
        string filename;
        dds::core::optional<dds::core::xtypes::DynamicData> ddsData;

        // problem with size of Histogram
       // map<string, prometheus::Family<prometheus::Counter>*> metricsMap;
       // map<string, METRIC_VARIANT &> metricsMap;
       
       map<string, boost::any> metricsMap;
};

#endif