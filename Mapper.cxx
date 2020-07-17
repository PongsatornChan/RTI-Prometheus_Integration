#include <map>
#include <string>

#include <dds/core/corefwd.hpp>

#include <dds/core/Optional.hpp>
#include <dds/core/xtypes/DynamicData.hpp>
#include <dds/core/xtypes/StructType.hpp>
#include <dds/core/xtypes/UnionType.hpp>
#include <dds/core/xtypes/MemberType.hpp>
#include <dds/core/xtypes/AliasType.hpp>
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

template<>
struct convert<string*> {
    static Node encode(const string* &string_array) {
        YAML::Node node;
        node = YAML::Load("[]");
        for (int i = 0; i < string_array->size(); ++i) {
            node.push_back(string_array[i]);
        }
        return node;
    }

    static bool decode(const Node &node, string* list) {
        if (!node.IsSequence()) {
            return false;
        }
        string temp[node.size()];
        for (int i = 0; i < node.size(); ++i) {
            temp[i] = node[i].as<string>();
        }
        list = temp;
        return true;
    }
}; 
}

//---------FamilyConfig---------------------------------------------------------------------------------
FamilyConfig::FamilyConfig(string i_name, string i_help,
                        MetricType i_type, 
                        string i_data_path,
                        TypeKind i_data_type, 
                        map<string, string> i_key_map, 
                        map<string, string> i_collection_map, 
                        unsigned long num) : 
        type (i_type),
        name (i_name),
        help (i_help),
        data_type (i_data_type),
        key_map (i_key_map),
        collection_map (i_collection_map),
        num_metrics (num),
        data_path (i_data_path)
{

}

FamilyConfig::FamilyConfig(const FamilyConfig& fam_config) :
    type (fam_config.type),
    name (fam_config.name),
    help (fam_config.help),
    data_type (fam_config.data_type),
    key_map (fam_config.key_map),
    collection_map (fam_config.collection_map),
    num_metrics (fam_config.num_metrics),
    data_path (fam_config.data_path)
{

}

FamilyConfig::~FamilyConfig() {}

//------------------------------------------------------------------------------------------------
//---------Mapper---------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
// Deal with Configuration file
Mapper::Mapper(std::string config_file) {
    string config_filename = "../";
    config_filename.append(config_file);
    YAML::Node topic = YAML::LoadFile(config_filename);
    YAML::Node config = topic["Topic"];
    if (config.IsNull()) {
        throw YAML::BadFile(config_filename);
    }
    
    if (config["ignore"].IsSequence()) {
        ignore_list = config["ignore"].as<string*>();
    } else {
        ignore_list = NULL;
    }

    for (YAML::const_iterator it = config["Metrics"].begin(); it != config["Metrics"].end(); ++it) {
        string name = "rti_dds_monitoring_domainParticipantEntityStatistics_";
        string data_path = it->second["data"].as<string>();
        name.append(boost::replace_all_copy(data_path, ".", "_"));

        MetricType type;
        if (it->second["type"]) {
            type = what_type(it->second["type"].as<string>());
        } else {
            type = MetricType::Gauge;
        }

        string help = it->second["description"].as<string>();
        // TODO-- get key member before getting sample!?
        map<string, string> labels_map = {{"key", "Member_ID"}}; 

        FamilyConfig* fam_config = new FamilyConfig(name, help, type, data_path, 
                                    TypeKind::INT_64_TYPE, labels_map, {}, 0);
        config_map[name] = fam_config;
    }
}

/**
 * Deallocate metrics_vec (vector<MetricConfig*>)
 * configMap (map<string, FamilyConfig*>)
 */ 
Mapper::~Mapper() {
     for (map<string, FamilyConfig*>::iterator it = config_map.begin(); it != config_map.end(); ++it) {
         delete it->second;
     }
     config_map.clear();
}
// ignore list
void Mapper::auto_map(const dds::core::xtypes::DynamicType& topic_type, FamilyConfig config) {
    TypeKind kind = topic_type.kind();
    switch (kind.underlying()) {
    case TypeKind::PRIMITIVE_TYPE: {
        if (kind == TypeKind::BOOLEAN_TYPE 
        || kind == TypeKind::CHAR_8_TYPE
        || kind == TypeKind::UINT_8_TYPE) {
            // Not mappable
            break;
        } else {
            config.data_type = kind;
        }
        config_map[config.name] = &config;
    }
        break;
    case TypeKind::UNION_TYPE: {
        const UnionType& union_type =
                static_cast<const UnionType&> (topic_type);
        for (int i = 0; i < union_type.member_count(); ++i) {
            const UnionMember& member = union_type.member(i); 
            FamilyConfig new_config(config);
            new_config.name.append(member.name());
            new_config.name.append("_");
            new_config.data_path.append(member.name());
            new_config.data_path.append(".");
            for (string s: ignore_list) {
                if (new_config.data_path.find(s) != string::npos) {
                    return;
                }
            }
            auto_map(member.type(), new_config);
        }
    }
        break;
    case TypeKind::STRUCTURE_TYPE: {
        const StructType& struct_type =
                static_cast<const StructType&> (topic_type);
        for (int i = 0; i < struct_type.member_count(); ++i) {
            const Member& member = struct_type.member(i);
            if (member.is_key()) {
                // data path to this member 
                // TODO-- check 
                // assuming key members always at the top
                string data_path = config.data_path;
                data_path.append(member.name());
                config.key_map[member.name()] = data_path;
            } else {
                FamilyConfig new_config(config);
                new_config.name.append(member.name());
                new_config.name.append("_");
                new_config.data_path.append(member.name());
                new_config.data_path.append(".");
                for (string s: ignore_list) {
                if (new_config.data_path.find(s) != string::npos) {
                    return;
                }
            }
                auto_map(member.type(), new_config);
            }
        }
    }
        break;
    case TypeKind::ARRAY_TYPE: {
        const ArrayType& array_type =
                static_cast<const ArrayType &>(topic_type);

        FamilyConfig new_config(config);
        // how to access array element from DynamicData
        // LoanedDynamicData or vector<>
        std::vector<string> results;
        boost::split(results, new_config->data_path, [](char c){return c == '.';});
        new_config.collection_map[results.end()] = new_config.data_path;
        auto_map(array_type.content_type(), new_config);
    }
        break;
    case TypeKind::SEQUENCE_TYPE: {
        const SequenceType& seq_type = 
                static_cast<const SequenceType &>(topic_type);
                
        FamilyConfig new_config(config);
        auto_map(seq_type.content_type(), new_config);
    }
        break;
    case TypeKind::ALIAS_TYPE: {
        const AliasType& alias_type =
                static_cast<const AliasType &>(topic_type);
        FamilyConfig new_config(config);
        auto_map(resolve_alias(alias_type), new_config);
    }
        break;
    default:
        break;
    }
    
}

/*
* Limitation of current implementation:
*   - Seqence
*   - Evolving Extendable type
*/
void Mapper::register_metrics(std::shared_ptr<Registry> registry) {
    // call_on_data_avaialable_total is default metric
    // TODO this can de the pusedo-metric
    Family_variant temp = create_family(MetricType::Counter, 
                                    "call_on_data_available_total", 
                                    "How many times this processor call on_data_available()",
                                    {{"Test", "on_data_available"}}, registry);
    add_metric adder;
    // TODO-- get topic name this processor associate with
    std::vector<string> results;
    boost::split(results, config_map[0]->data_path, [](char c){return c == '.';});
    adder.labels = {{"Topic", results[0]}};
    boost::apply_visitor(adder, temp);
    family_map["call_on_data_available_total"] = temp;

    for (map<string, FamilyConfig*>::const_iterator cit = config_map.begin();
        cit != config_map.end(); ++cit) {
        
        FamilyConfig* fam = cit->second;
        // DEBUG
        std::cout << "fam->name: " << fam->name << endl;
        std::cout << "fam->numMetrics: " << fam->num_metrics << endl;
        temp = create_family(fam, registry);
        family_map[fam->name] = temp;
    }
}

/*
* Create Family (container of metrics) 
* and register it to REGISTRY
* Return: Family<T>* for T = Counter, Gauge, Histogram, or Summary
* and return boost::blank if fail
*/
Family_variant Mapper::create_family(MetricType type, string name, string detail, 
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

Family_variant Mapper::create_family(FamilyConfig* famConfig, std::shared_ptr<Registry> registry) {
    return create_family(famConfig->type, famConfig->name, famConfig->help, {}, registry);
}

/* 
*  Utility function to determine type of metric 
*  based on sting TYPE given
*  Return: MetricType 
*/
MetricType Mapper::what_type(std::string type) {
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

/* 
*  Utility function to get data from a sample
*  Only work with basic data with no array or sequence
*  Return: double 
*/
double Mapper::get_value(const dds::core::xtypes::DynamicData& data, vector<string> path, TypeKind kind) {
    DynamicData temp = data;
    for (int i = 0; i < path.size(); ++i) {
        if (i == path.size()-1) {
            if (kind == TypeKind::INT_16_TYPE ) {
                return (double) temp.value<int16_t>(path[i]);
            } else if (kind == TypeKind::UINT_16_TYPE) {
                return (double) temp.value<uint16_t>(path[i]);
            } else if (kind == TypeKind::INT_32_TYPE) {
                return (double) temp.value<int32_t>(path[i]);
            } else if (kind == TypeKind::UINT_32_TYPE) {
                return (double) temp.value<uint32_t>(path[i]);
            } else if (kind == TypeKind::INT_64_TYPE) {
                return (double) temp.value<int64_t>(path[i]);
            } else if (kind == TypeKind::UINT_64_TYPE) {
                return (double) temp.value<uint64_t>(path[i]);
            } else if (kind == TypeKind::FLOAT_32_TYPE) {
                return (double) temp.value<float>(path[i]);
            } else if (kind == TypeKind::FLOAT_64_TYPE) {
                return temp.value<double>(path[i]);
            // } else if (kind == TypeKind::FLOAT_128_TYPE) {
            //     return (double) temp.value<rti::core::LongDouble>(path[i]);
            } else {
                return (double) temp.value<int>(path[i]);
            }
        }
        temp = temp.value<DynamicData>(path[i]);
    }
    throw new std::runtime_error("can't find value in " + path);
    return 0;
}

// get the string representation of keyed member at data_path
// traverse basic data structure only 
// Return NULL if finding array or sequence
string Mapper:get_string(const dds::core::xtypes::DynamicData& data, vector<string> path) {
    DynamicData temp = data;
    try {
        for (int i = 0; i < path.size(); ++i) {
            if (temp.is_member_key(path[i])) { 
                TypeKind kind = temp.value<DynamicData>(path[i]).type_kind();
                if (kind == TypeKind::PRIMATIVE_TYPE ) {
                    vector<string> new_path = {path[i]};
                    return to_string(get_value(temp, new_path, kind));
                } else if (kind == TypeKind::STRING_TYPE) {
                    return temp.value<string>(path[i]);
                } else if (kind == TypeKind::WSTRING_TYPE) {
                    return temp.value<string>(path[i]);
                }
            }
            temp = temp.value<DynamicData>(path[i]);
        }
    } catch (dds::core::Exception e) {
        return "";
    }
    throw new std::runtime_error("can't find value in " + path);
    return 0;
}

// deal with array or sequence
void Mapper:get_data(vector<map<string, string>>* set_labels, vector<double>* vars, 
                const dds::core::xtypes::DynamicData& data, FamilyConfig config) {
    
    // no list in path (base case)
    if (config.collection_map.empty()) {
        // labels
        map<string, string> key_labels = {};
        for (map<string, string>::const_iterator cit = config.key_map.begin();
        cit != config.key_map.end(); ++cit) {
            std::vector<string> path_key;
            string str_path_key = cit->second;
            boost::split(path_key, str_path_key, [](char c){return c == '.';});
            string str_rep = get_string(data, path_key);
            key_labels[cit->first] = str_rep;
        }    
        set_labels->push_back(key_labels);

        // value
        std::vector<string> path_value;
        boost::split(path_value, config.data_path, [](char c){return c == '.';});
        double var = get_value(data, path_value, config.type);
        vars->push_back(var);

    } else {

        std::vector<string> path_list;
        string str_path_list = config.collection_map.begin()->second;
        boost::split(path_list, str_path_list, [](char c){return c == '.';});

        // get key labels before list
        map<string, string> key_labels = {};
        // key_map for recursive 
        map<string, string> new_key_map = config.key_map;
        for (map<string, string>::const_iterator cit = config.key_map.begin(); cit != config.key_map.end(); ++cit) {
            std::vector<string> path_key;
            string str_path_key = cit->second;
            boost::split(path_key, str_path_key, [](char c){return c == '.';});
            // key before before list 
            // key after list depend on what element of list
            if (path_key.size() < path_list.size()) {
                string str_rep = get_string(data, path_key);
                key_labels[cit->first] = str_rep;
                new_key_map.erase(cit->first);
            } else {
                // stop when key is a member of element of list
                break;
            }
        }

        // TODO modify config
        FamilyConfig new_config(config);
        // only the unseen keys
        new_config.key_map = new_key_map;
        for (map<string, string>::const_iterator cit = new_config.key_map.begin(); cit != new_config.key_map.end(); ++cit) {
            cit->second.erase(0, str_path_list.length() + 1);
        }
        // recursively call on members of the first list
        new_config.collection_map.erase(config.collection_map.begin()->first);
        for (map<string, string>::const_iterator cit = new_config.collection_map.begin(); cit != new_config.collection_map.end(); ++cit) {
            cit->second.erase(0, str_path_list.length() + 1);
        }
        new_config.data_path.erase(0, str_path_list.length() + 1);
        // Traverse to list
        DynamicData temp = data;
        try {
            // traverse to the array or sequence parent
            for (int i = 0; i < path_list.size() - 1; ++i) {
                temp = temp.value<DynamicData>(path_list[i]);
            }
            string name_list = path_list[path_list.size()];
            vector<DynamicData> vec = temp.get_values<DynamicData>(name_list);
            name_list.append("_index");
            // recursice on all element in the list
            for (int i = 0; i < vec.size(); ++i) {
                // assume vector can contain multiple empty maps
                vector<map<string, string>> recur_set_labels = {};
                vector<double> recur_vars = {};
                get_data(&recur_set_labels, &recur_vars, vec[i], new_config);
                // append parent key labels to the each child labels
                for (int j = 0; j < recur_set_labels.size(); ++j) {
                    recur_set_labels[j].insert(key_labels.begin(), key_labels.end());
                    recur_set_labels[j][name_list] = to_string(i);
                }
                // DEBUG
                if (recur_set_labels.size() != recur_vars.size()) {
                    cout << "labels not match values. ABORT!" << endl;
                    throw new dds::core::Error("labels not match values");
                }
                set_labels->insert(set_labels->end(), recur_set_labels.begin(), recur_set_labels.end());
                vars->insert(vars->end(), recur_vars.begin(), recur_vars.end());
            }

        } catch (dds::core::Exception e) {
            cout << "Traverse fail" << path_list << endl;
        }
        
    }
}

// labels we need:
//  {keyed_member_name, string_rep} for members of keyed struct
//  {index, ...} for members that is array or sequnce type
// 
int Mapper::update_metrics(const dds::core::xtypes::DynamicData& data, 
                           const dds::sub::SampleInfo& info) {
    Family<Counter>* counter_fam = 
        boost::get<Family<Counter>*>(family_map["call_on_data_available_total"]);
    Counter& counter = counter_fam->Add({{"Topic", "topic name"}});
    counter.Increment();

    for (map<string, FamilyConfig*>::const_iterator cit = config_map.begin(); cit != config_map.end(); ++cit) {
        
        vector<map<string,string>> labels_list;
        vector<double> vars;
        try{
            Mapper::get_data(&set_labels, &vars, data, (cit->second)*);
            // DEBUG
            std::cout << "get_data with " << data_path;
            std::cout << " return " << vars << endl;
            std::cont << " with labels set " << set_labels << endl;
        } catch(std::exception& e) {
            std::cout << "get_data error: set to 0" << endl; 
            vars.push_back(0.0);
        }

        // update all time series associated with this metric
        update_metric updater;
        for (int i = 0; i < vars.size(); ++i) {
            updater.value = vars[i];
            if (labels_list.empty()) {
                std::stringstream ss;
                ss << info.instance_handle();
                updater.labels = {{"Instance_ID", ss.str()}} ;
                boost::apply_visitor(updater, family_map[cit->first]);
            } else {
                updater.labels = labels_list[i];
                boost::apply_visitor(updater, family_map[cit->first]);
            }
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

// TODO StatisticVariable only need mean
