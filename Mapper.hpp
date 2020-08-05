/**
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
#include <dds/sub/Sample.hpp>
#include <dds/sub/SampleInfo.hpp>

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
using namespace dds::core::xtypes;

/**
 * All possible type of Family
 */
typedef boost::variant<
        boost::blank, 
        Family<Counter>*, 
        Family<Gauge>*, 
        Family<Histogram>*, 
        Family<Summary>*> 
    Family_variant;

/**
 * All possible type of metric
 */
typedef boost::variant<
        boost::blank,
        Counter*,
        Gauge*,
        Histogram*,
        Summary*>
    Metric_variant;

typedef string DataPath;

typedef string MetricName;

typedef string MemberName;

typedef string Filename;

typedef string LabelKey;

typedef map<string, string> Label;

/**
 *   Represent a YAML Node contains all nessesary information to 
 *   construct Family metrics with.
 */
struct MetricConfig {
    /** 
     * family type (Counter, Gauge, Histogram, Summary)
     */
    prometheus::MetricType type;

    /**
     * name of family that will appear in prometheus
     */ 
    MetricName name;

    /**
     * helpful description of this family, what it represents.
     */ 
    std::string help;

    /** 
     * path to targeted value in DDS Sample 
     * each level seperate by "."
     */ 
    DataPath data_path;

    /**
     * type of data that data_path lead to 
     */
    TypeKind data_type;
    /**
     * label name of the name of keyed member
     * label value of data_path to that member  
     */ 
    std::map<MemberName, DataPath> key_map;
    /**
     * label name of the name of the array of sequence
     * label value of data_path to that member
     */
    std::map<MemberName, DataPath> collection_map;

    /**
     * @param I_NAME name of this metric
     * @param I_HELP helpful description of this family
     * @param I_TYPE type of this metric
     * @param I_DATA_PATH data_path to a member that this metric represents
     * @param I_DATA_TYPE type of the member that this metric represents
     * @param I_KEY_MAP key equals name of a keyed member, 
     *                  value equals data_path to akeyed member
     * @param I_COLLECTION_MAP key equals name of collection type memeber
     *                         value equals data_path to the member 
     */ 
    MetricConfig(
            MetricName i_name, 
            string i_help,
            MetricType i_type=MetricType::Gauge, 
            DataPath i_data_path="",
            TypeKind i_data_type=TypeKind::FLOAT_64_TYPE, 
            map<string, string> i_key_map={}, 
            map<string, string> i_collection_map={});

    MetricConfig(const MetricConfig&);

    ~MetricConfig();
};      

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
    Mapper(Filename config_file);

    ~Mapper();

    void find_key_n_collection(const DynamicType& type, MetricConfig &config);

    /**
     * Call when is not auto mapping 
     * to provide name to specify metrics config
     * Note: because the specify configs are created at processor initailization
     *       we don't know the name of topic yet 
     *       so the specify configs did not contain name  
     *  
     * @param NAME topic name to be attached to specific config, each level divide by _
     */
    void config_user_specify_metrics(const DynamicType& type); 

    /**
     *  Auto map all primative members of TYPE
     * 
     *  @param TYPE DynamicType to create metrics from
     *  @param CONFIG enable recursive, contain information from 
     *                higher level
     */
    void auto_map(const dds::core::xtypes::DynamicType& type, MetricConfig config);

    /** 
     *  Mapper will create a /metric based on config FILENAME 
     *  Then register it to the one and only REGISTRY 
     *  
     * @param REGISTRY to register Family to
     */
    void register_metrics(std::shared_ptr<Registry> registry);

    /** 
     *  Uppon receiving samples (on_data_available) processor 
     *  should pass the sample to this function to update metric 
     *  
     * @param DATA the DDS sample
     * @return 1 if success, 0 otherwise
     */
    int update_metrics(const dds::core::xtypes::DynamicData&, const dds::sub::SampleInfo&);

    bool is_auto_mapping();

    bool use_key_hash_label();

    static LabelKey data_path_to_label_name(DataPath data_path);

    static void format_key_label(Label& key_label);
    /**
     *  Utility function to determine type of metric 
     *  based on sting given
     * 
     * @param TYPE string depicting type
     * @return MetricType (prometheus::Counter, Gauge, Histogram, Summary)
     */
    static MetricType what_type(std::string type);

    /**
     *  Utility function to retrive value from DATA sample
     *  using PATH. 
     * 
     * @param DATA data from topic sample, used DynamicData as a wrap.
     * @param DATA_ATH path to target value, divided by ":" in each level
     *              left to right. 
     * @param TYPE_KIND TypeKind of the member data
     * @return double value from map-able member pointed by path
     */
    static double get_value(
            const DynamicData& data,
            DataPath data_path,
            TypeKind type_kind); 
    
    /**
     *  Utility function to determine if KIND is one of primitive kinds
     * 
     *  @param KIND TypeKind
     *  @return BOOL true if KIND is one of primitive kind, false otherwise
     */
    static bool is_primitive_kind(TypeKind kind);

    /**
     *  Utility function to retrive string representation of keyed
     *  members point to bt path
     * 
     *  @param KEY_LABEL label that will contain the results key map
     *  @param DATA data from topic sample
     *  @param DATA_PATH path lead to the keyed member
     *  Return map with each keys equals data_path to a keyed member and
     *         each values equals to value of a keyed member 
     */
    static void get_key_labels(Label& key_label, const DynamicData& data, DataPath data_path);

    /**
     *  Utility function to get value(s) and label(s) to update a metric
     *  @param SET_LABELS assume set_labels vector is always empty at the start
     *                    each map in the vector is a label for one value in vars
     *  @param VARS assume vars vector is always empty at the start
     *              if there is a list in path to the member, vars will contain 
     *  @param DATA data from topic sample
     *  @param CONFIG contain all the information need about a metric
     */
    static void get_data(
            vector<Label> &set_labels,
            vector<double> &vars,
            const DynamicData& data,
            MetricConfig config);
private:

    bool is_auto_map;

    bool use_key_hash;

    string topic_name;
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
    map<string, Family_variant> family_map;
    
    /*
    * Contain data_path to key members that differentiate one instance
    * from another. This list is user specify.
    */
    vector<DataPath> instance_identifiers;
    
    // keep members that will be ignore from mapping process
    vector<DataPath> ignore_list; 

    /*
    * Map that keeps track of configuration 
    * for easy access and keep YAML file abstract 
    * away from the logic of mapping.
    * Act as YAML file in a way
    */
    map<string, MetricConfig*> config_map;

    /*
    * Create and register a family of METRIC_TYPE with name NAME,
    * helpful description of DETAIL, starter labels LABELS, and 
    * register it to REGISTRY
    */ 
    Family_variant create_metric(MetricType, MetricName name, string detail, 
                    const Label& labels, shared_ptr<Registry> registry);
    Family_variant create_metric(MetricConfig*, shared_ptr<Registry>);

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
    Label labels;
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
    Label labels;
    double value;
};


#endif