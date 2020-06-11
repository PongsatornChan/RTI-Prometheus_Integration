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

#include "MonitorProcessor.hpp"

using namespace rti::routing;
using namespace rti::routing::processor;
using namespace rti::routing::adapter;
using namespace dds::core::xtypes;
using namespace dds::sub::status;

using namespace prometheus;

/*
 * --- MonitorExposer ---------------------------------------------------------
 */

prometheus::Exposer exposer{"127.0.0.1:8080", 1};
std::shared_ptr<Registry> registry = std::make_shared<Registry>();

MonitorExposer::MonitorExposer() : 
        counter_family (BuildCounter()
                .Name("call_on_data_available_total")
                .Help("How many times this processor call on_data_available()")
                .Labels({{"label", "value"}})
                .Register(*registry)),
        second_counter (counter_family.Add(
                {{"label1", "value1"}})),
        gauge_family (BuildGauge()
                .Name("data_writer_status")
                .Help("Tell the current position of the squre shape")
                .Labels({{"job", "RTI_Shape"}})
                .Register(*registry)),
        current_pushed_samples (gauge_family.Add(
                {{"Count", "samples"}})),
        current_pushed_bytes (gauge_family.Add(
                {{"Count", "bytes"}}))

{
        exposer.RegisterCollectable(registry);
        std::cout << "MonitorExposer(Processor) is created" << '\n';
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
    std::cout << "MonitorExposer::on_input_enabled is called." << '\n';
    std::cout << "____________________________________" << '\n';
}

void MonitorExposer::on_data_available(rti::routing::processor::Route &route)
{
    std::cout << "MonitorExposer::on_data_available is called." << '\n';
    std::cout << "____________________________________" << '\n';
    // Prometheus count how many time on_data_available() is called
    second_counter.Increment();

    // Split input shapes  into mono-dimensional output shapes
    auto input_samples = route.input<DynamicData>(0).take();
    for (auto sample : input_samples) {
        if (sample.info().valid()) { // what valid?
            // set x and y data for prometheus
            output_data_ = sample.data();

            /****************************************
            How to get value from pushed_sample_count
            datawriter_protocol_status: 
                status: 
                        pushed_sample_count: 174
            *****************************************/

           std::cout << typeid(output_data_).name() << '\n';
           std::cout << typeid(sample.data()).name() << '\n';
           std::cout << "----------------------" << '\n';
        //     double var = (double) output_data_.get()
        //         .value<int32_t>("datawriter_protocol_status");
        //     x_gauge.Set(var);
        //     var = (double) output_data_.get().value<int32_t>("y");
        //     y_gauge.Set(var);
        } else {
            // set x and y data for prometheus
            output_data_ = sample.data();
        //     double var = output_data_.get().value<double>("y");
        //     x_gauge.Set(var);
        //     var = output_data_.get().value<double>("x");
        //     y_gauge.Set(var);
        }
    }
}

/*
 * --- MonitorProcessorPlugin --------------------------------------------------
 */

MonitorProcessorPlugin::MonitorProcessorPlugin(const rti::routing::PropertySet &)
{
}


rti::routing::processor::Processor *MonitorProcessorPlugin::create_processor(
        rti::routing::processor::Route &,
        const rti::routing::PropertySet &properties)
{
    return new MonitorExposer();
}

void MonitorProcessorPlugin::delete_processor(
        rti::routing::processor::Route &,
        rti::routing::processor::Processor *processor)
{
    delete processor;
}


RTI_PROCESSOR_PLUGIN_CREATE_FUNCTION_DEF(MonitorProcessorPlugin);