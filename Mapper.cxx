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

#include "Mapper.hpp"

using namespace dds::core::xtypes;

Mapper::Mapper(std::string configFile) {
    filename = configFile;
}

void Mapper::registerMetric(std::shared_ptr<Registry> registry) {
    return;
}

int Mapper::updateMetric(dds::core::xtypes::DynamicData data) {
    return 1;
}
