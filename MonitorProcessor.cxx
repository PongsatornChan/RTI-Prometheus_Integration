/*
 * (c) 2018 Copyright, Real-Time Innovations, Inc.  All rights reserved.
 *
 * RTI grants Licensee a license to use, modify, compile, and create derivative
 * works of the Software.  Licensee has the right to distribute object form
 * only for use with RTI products.  The Software is provided "as is", with no
 * warranty of any type, including any warranty for fitness for any purpose.
 * RTI is under no obligation to maintain or support the Software.  RTI shall
 * not be liable for any incidental or consequential damages arising out of the
 * use or inability to use the software.
 */
#include <iterator>
#include <stdio.h>
#include <stdlib.h>

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include <dds/core/corefwd.hpp>

#include <dds/core/xtypes/DynamicData.hpp>
#include <dds/core/xtypes/StructType.hpp>
#include <rti/routing/processor/Processor.hpp>
#include <rti/routing/processor/ProcessorPlugin.hpp>

#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

#include "yaml-cpp/yaml.h"

#include "MonitorProcessor.hpp"

// #include "Mapper.hpp"
#include "Mapper.cxx"

using namespace rti::routing;
using namespace rti::routing::processor;
using namespace rti::routing::adapter;
using namespace dds::core::xtypes;
using namespace dds::sub::status;

using namespace prometheus;

/*
 * --- MonitorExposer ---------------------------------------------------------
 */
const std::string DEFAULT_ADDRESS = "127.0.0.1:8080";

/* 
*  initalize all the family and metrics 
*  These would be handle by MAPPER in the future 
*/
MonitorExposer::MonitorExposer(std::string input_filename, 
                               prometheus::Exposer& input_exposer,
                               std::shared_ptr<prometheus::Registry> input_registry) : 
        mapper (Mapper(input_filename)),
        exposer (input_exposer),
        registry (input_registry)
{
        filename = input_filename;
        std::cout << "MonitorExposer(Processor) is created" << '\n';
        std::cout << "with mapping filename: " << filename << '\n'; 
        std::cout << "____________________________________" << '\n';
}

MonitorExposer::~MonitorExposer()
{
}

void MonitorExposer::on_input_enabled(
        rti::routing::processor::Route &route,
        rti::routing::processor::Input &input)
{
    // The type this processor works with is the ShapeType, which shall
    // be the type the input as well as the two outputs. Hence we can use
    // the input type to initialize the output data buffer
    output_data_ = input.get<DynamicData>().create_data();
    DynamicType* topic_type = static_cast<dds::core::xtypes::DynamicType*>
                                (input.stream_info().type_info().type_representation());
    std::cout << "MonitorExposer::on_input_enabled is called." << '\n';
    std::cout << "topic_type of " << topic_type->name() << endl;
    std::cout << "____________________________________" << '\n';

    mapper.config_user_specify_metrics(*topic_type);    
    if (mapper.is_auto_mapping()) {
        string name = topic_type->name();
        name = boost::replace_all_copy(name, "::", "_");
        name.append("_");
        MetricConfig metric_config(name, "");
        //DEBUG
        std::cout << "Before auto_map" << endl;
        mapper.auto_map(*topic_type, metric_config);
        //DEBUG
        std::cout << "Auto_map success" << endl;
    } 
    std::cout << "register metrics...." << endl;
    mapper.register_metrics(registry);
    std::cout << "register completed!" << endl << endl;
    exposer.RegisterCollectable(registry);
}

void MonitorExposer::on_data_available(rti::routing::processor::Route &route)
{
    std::cout << "MonitorExposer::on_data_available is called." << '\n';

    // Split input shapes  into mono-dimensional output shapes
    auto input_samples = route.input<DynamicData>(0).take();
    for (auto sample : input_samples) {
        if (sample.info().valid()) { 
            // output_data_ = sample.data();
            // mapper.updateMetrics((output_data_.get()));
            mapper.update_metrics(sample.data(), sample.info());
        } else {
            // output_data_ = sample.data();
            // mapper.updateMetrics((output_data_.get()));
            mapper.update_metrics(sample.data(), sample.info());
        }
    }
    std::cout << "____________________________________" << '\n';
}

/*
 * --- MonitorProcessorPlugin --------------------------------------------------
 */

MonitorProcessorPlugin::MonitorProcessorPlugin(const rti::routing::PropertySet &properties)
/* user can provide HTTP Address for exposer to use or DEFAULT_ADDRESS of 127.0.0.1:8080
   Note: user will need to tell prometheus about the custom address in prometheus.yml */
: exposer {(properties.find("exposer") != properties.end() ? properties.find("exposer")->second: DEFAULT_ADDRESS) , 1},
  registry (std::make_shared<Registry>())
{       
}


rti::routing::processor::Processor *MonitorProcessorPlugin::create_processor(
        rti::routing::processor::Route &,
        const rti::routing::PropertySet &properties)
{
    const std::string property_name = "mapping"; 
    std::string filename = properties.find(property_name)->second;
    return new MonitorExposer(filename, exposer, registry);
}

void MonitorProcessorPlugin::delete_processor(
        rti::routing::processor::Route &,
        rti::routing::processor::Processor *processor)
{
    delete processor;
}

void printDebug(std::string string) {
        std::cout << "DEBUG-------------------\n";
        std::cout << string << '\n';
        std::cout << "END_DEBUG---------------\n";
}

RTI_PROCESSOR_PLUGIN_CREATE_FUNCTION_DEF(MonitorProcessorPlugin);