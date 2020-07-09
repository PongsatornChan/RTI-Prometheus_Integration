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

#ifndef MONITOR_PROCESSOR_HPP_
#define MONITOR_PROCESSOR_HPP_

#include <iterator>
#include <stdio.h>
#include <stdlib.h>

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include <dds/core/corefwd.hpp>

#include <dds/core/Optional.hpp>
#include <dds/core/xtypes/DynamicData.hpp>
#include <dds/core/xtypes/StructType.hpp>
#include <rti/routing/processor/Processor.hpp>
#include <rti/routing/processor/ProcessorPlugin.hpp>

/*
* library from Prometheus C++ Client library
*/
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

using namespace prometheus;

#include "Mapper.hpp"


class MonitorExposer : public rti::routing::processor::NoOpProcessor {
public:
    void on_data_available(rti::routing::processor::Route &) override;

    void on_input_enabled(
            rti::routing::processor::Route &route,
            rti::routing::processor::Input &input) override;

    MonitorExposer(std::string input_filename, 
                   prometheus::Exposer& input_exposer, 
                   std::shared_ptr<prometheus::Registry> input_registry);

    ~MonitorExposer();

private:
    // Optional member for deferred initialization: this object can be created
    // only when the output is enabled.
    // You can use std::optional if supported in your platform
    dds::core::optional<dds::core::xtypes::DynamicData> output_data_;

    // Yaml filename
    std::string filename;

    // Mapper that handle mapping topic to matric and register it
    Mapper mapper;

    // Exposer own by ProcessorPlugin which passes on to processor
    prometheus::Exposer& exposer;
    // Registry own by ProcessorPlugin 
    std::shared_ptr<prometheus::Registry> registry;

};

class MonitorProcessorPlugin : public rti::routing::processor::ProcessorPlugin {
public:

    rti::routing::processor::Processor *create_processor(
            rti::routing::processor::Route &route,
            const rti::routing::PropertySet &properties) override;

    void delete_processor(
            rti::routing::processor::Route &route,
            rti::routing::processor::Processor *processor) override;

    MonitorProcessorPlugin(const rti::routing::PropertySet &properties);

private:
    // Exposer for Prometheus 
    prometheus::Exposer exposer;
    std::shared_ptr<prometheus::Registry> registry;
};


/**
 * This macro defines a C-linkage symbol that can be used as create function
 * for plug-in registration through XML.
 *
 * The generated symbol has the name:
 *
 * \code
 * ShapesProcessorPlugin_create_processor_plugin
 * \endcode
 */
RTI_PROCESSOR_PLUGIN_CREATE_FUNCTION_DECL(MonitorProcessorPlugin);

#endif