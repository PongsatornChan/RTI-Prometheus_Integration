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
                .Name("domainParticipant_process_statistics")
                .Help("Tell the current position of the squre shape")
                .Labels({{"job", "RTI_Shape"}})
                .Register(*registry)),
        user_cpu_time (gauge_family.Add(
                {{"process", "user_cpu_time"}})),
        kernel_cpu_time (gauge_family.Add(
                {{"process", "kernel_cpu_time"}}))

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
            output_data_ = sample.data();

            /****************************************
            How to get value from pushed_sample_count
            process: 
                user_cpu_time: 
                        sec: 351
                        nanosec: 764159998
            *****************************************/

           double var = (double) output_data_.get().value<DynamicData>("process")
                .value<DynamicData>("user_cpu_time")
                .value<int64_t>("sec");
           user_cpu_time.Set(var);
           var = (double) output_data_.get().value<DynamicData>("process")
                .value<DynamicData>("kernel_cpu_time")
                .value<int64_t>("sec");
           kernel_cpu_time.Set(var);
        } else {
            // set x and y data for prometheus
            output_data_ = sample.data();
            double var = (double) output_data_.get().value<DynamicData>("process")
                .value<DynamicData>("user_cpu_time")
                .value<int64_t>("sec");
            user_cpu_time.Set(var);
            var = (double) output_data_.get().value<DynamicData>("process")
                .value<DynamicData>("kernel_cpu_time")
                .value<int64_t>("sec");
            kernel_cpu_time.Set(var);
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