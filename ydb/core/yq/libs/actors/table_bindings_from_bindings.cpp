#include "table_bindings_from_bindings.h" 
 
#include <ydb/library/yql/providers/common/provider/yql_provider_names.h>
#include <ydb/core/yq/libs/result_formatter/result_formatter.h>
#include <util/generic/vector.h> 
 
namespace NYq {
 
using namespace NYql;

namespace { 

void FillBinding(NSQLTranslation::TTranslationSettings& sqlSettings, const YandexQuery::Binding& binding, const THashMap<TString, YandexQuery::Connection>& connections) { 
    TString clusterType; 
    TString path; 
    TString format; 
    TString compression; 
    TString schema; 
    switch (binding.content().setting().binding_case()) { 
    case YandexQuery::BindingSetting::kDataStreams: { 
        clusterType = PqProviderName; 
        auto yds = binding.content().setting().data_streams(); 
        path = yds.stream_name(); 
        format = yds.format(); 
        compression = yds.compression(); 
        schema = FormatSchema(yds.schema()); 
        break; 
    } 
    case YandexQuery::BindingSetting::kObjectStorage: { 
        clusterType = S3ProviderName; 
        const auto s3 = binding.content().setting().object_storage(); 
        if (s3.subset().empty()) { 
            throw yexception() << "No subsets in Object Storage binding " << binding.meta().id(); 
        } 
 
        const auto& s = s3.subset(0); 
        path = s.path_pattern(); 
        format = s.format(); 
        compression = s.compression(); 
        schema = FormatSchema(s.schema()); 
        break; 
    } 
 
    case YandexQuery::BindingSetting::BINDING_NOT_SET: { 
        throw yexception() << "BINDING_NOT_SET case for binding " << binding.meta().id() << ", name " << binding.content().name(); 
    } 
    // Do not add default. Adding a new binding should cause a compilation error 
    } 
 
    auto connectionPtr = connections.FindPtr(binding.content().connection_id()); 
    if (!connectionPtr) { 
        throw yexception() << "Unable to resolve connection for binding " << binding.meta().id() << ", name " << binding.content().name() << ", connection id " << binding.content().connection_id(); 
    } 
 
    NSQLTranslation::TTableBindingSettings bindSettings; 
    bindSettings.ClusterType = clusterType; 
    bindSettings.Settings["cluster"] = connectionPtr->content().name(); 
    bindSettings.Settings["path"] = path; 
    bindSettings.Settings["format"] = format; 
    // todo: fill format parameters 
    if (compression) { 
        bindSettings.Settings["compression"] = compression; 
    } 
    bindSettings.Settings["schema"] = schema; 
 
    // todo: use visibility to fill either PrivateBindings or ScopedBindings 
    sqlSettings.PrivateBindings[binding.content().name()] = std::move(bindSettings); 
} 
 
} //namespace
 
void AddTableBindingsFromBindings(const TVector<YandexQuery::Binding>& bindings, const THashMap<TString, YandexQuery::Connection>& connections, NSQLTranslation::TTranslationSettings& sqlSettings) { 
    for (const auto& binding : bindings) { 
        FillBinding(sqlSettings, binding, connections); 
    } 
} 

} //NYq
