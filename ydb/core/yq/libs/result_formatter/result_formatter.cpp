#include "result_formatter.h"

#include <ydb/library/yql/providers/common/schema/mkql/yql_mkql_schema.h>
#include <ydb/library/yql/providers/common/schema/expr/yql_expr_schema.h>
#include <ydb/library/yql/providers/common/codec/yql_codec.h>
#include <ydb/library/yql/public/udf/udf_data_type.h>
#include <ydb/library/yql/ast/yql_expr.h>
#include <ydb/library/yql/minikql/mkql_node.h>
#include <ydb/library/yql/minikql/computation/mkql_computation_node_holders.h>

#include <ydb/core/engine/mkql_proto.h>
#include <ydb/public/sdk/cpp/client/ydb_proto/accessor.h>

#include <library/cpp/json/yson/json2yson.h>

namespace NYq {

using namespace NKikimr::NMiniKQL;
using NYql::NUdf::TUnboxedValuePod;

namespace {

const NYql::TTypeAnnotationNode* MakePrimitiveType(NYdb::TTypeParser& parser, NYql::TExprContext& ctx)
{
    auto dataSlot = NYql::NUdf::GetDataSlot(TStringBuilder() << parser.GetPrimitive());
    return ctx.MakeType<NYql::TDataExprType>(dataSlot);
}

const NYql::TTypeAnnotationNode* MakeDecimalType(NYdb::TTypeParser& parser, NYql::TExprContext& ctx)
{
    auto decimal = parser.GetDecimal();
    auto dataSlot = NYql::NUdf::GetDataSlot(TStringBuilder() << "Decimal(" << (ui32)decimal.Precision << ',' << (ui32)decimal.Scale << ")");
    return ctx.MakeType<NYql::TDataExprType>(dataSlot);
}

NKikimr::NMiniKQL::TType* MakePrimitiveType(NYdb::TTypeParser& parser, NKikimr::NMiniKQL::TTypeEnvironment& env)
{
    auto dataSlot = NYql::NUdf::GetDataSlot(TStringBuilder() << parser.GetPrimitive());
    return TDataType::Create(GetDataTypeInfo(dataSlot).TypeId, env);
}

NKikimr::NMiniKQL::TType* MakeDecimalType(NYdb::TTypeParser& parser, NKikimr::NMiniKQL::TTypeEnvironment& env)
{
    auto decimal = parser.GetDecimal();
    return TDataDecimalType::Create((ui8)decimal.Precision, (ui8)decimal.Scale, env);
}

const NYql::TTypeAnnotationNode* MakeOptionalType(const NYql::TTypeAnnotationNode* underlying, NYql::TExprContext& ctx)
{
    return ctx.MakeType<NYql::TOptionalExprType>(underlying);
}

NKikimr::NMiniKQL::TType* MakeOptionalType(NKikimr::NMiniKQL::TType* underlying, NKikimr::NMiniKQL::TTypeEnvironment& env)
{
    return TOptionalType::Create(underlying, env);
}

const NYql::TTypeAnnotationNode* MakeListType(const NYql::TTypeAnnotationNode* underlying, NYql::TExprContext& ctx)
{
    return ctx.MakeType<NYql::TListExprType>(underlying);
}

NKikimr::NMiniKQL::TType* MakeListType(NKikimr::NMiniKQL::TType* underlying, NKikimr::NMiniKQL::TTypeEnvironment& env)
{
    return TListType::Create(underlying, env);
}

const NYql::TTypeAnnotationNode* MakeStructType(
    const TVector<std::pair<TString, const NYql::TTypeAnnotationNode*>>& i,
    NYql::TExprContext& ctx)
{
    TVector<const NYql::TItemExprType*> items;
    items.reserve(i.size()); 
    for (const auto& [k, v] : i) {
        items.push_back(ctx.MakeType<NYql::TItemExprType>(k, v));
    }
    return ctx.MakeType<NYql::TStructExprType>(items);
}

NKikimr::NMiniKQL::TType* MakeStructType(
    const TVector<std::pair<TString, NKikimr::NMiniKQL::TType*>>& items,
    NKikimr::NMiniKQL::TTypeEnvironment& env)
{
    return TStructType::Create(&items[0], items.size(), env);
}

const NYql::TTypeAnnotationNode* MakeTupleType(
    const TVector<const NYql::TTypeAnnotationNode*>& items,
    NYql::TExprContext& ctx)
{
    return ctx.MakeType<NYql::TTupleExprType>(items);
}

NKikimr::NMiniKQL::TType* MakeTupleType(
    const TVector<NKikimr::NMiniKQL::TType*>& items,
    NKikimr::NMiniKQL::TTypeEnvironment& env)
{
    return TTupleType::Create(items.size(), &items[0], env);
}

const NYql::TTypeAnnotationNode* MakeDictType(
    const NYql::TTypeAnnotationNode* key,
    const NYql::TTypeAnnotationNode* payload,
    NYql::TExprContext& ctx)
{
    return ctx.MakeType<NYql::TDictExprType>(key, payload);
}

NKikimr::NMiniKQL::TType* MakeDictType(
    NKikimr::NMiniKQL::TType* key,
    NKikimr::NMiniKQL::TType* payload,
    NKikimr::NMiniKQL::TTypeEnvironment& env)
{
    return TDictType::Create(key, payload, env);
}

const NYql::TTypeAnnotationNode* MakeVoidType(NYql::TExprContext& ctx)
{
    return ctx.MakeType<NYql::TVoidExprType>();
}

NKikimr::NMiniKQL::TType* MakeVoidType(NKikimr::NMiniKQL::TTypeEnvironment& env)
{
    return env.GetTypeOfVoid();
}

const NYql::TTypeAnnotationNode* MakeNullType(NYql::TExprContext& ctx)
{
    return ctx.MakeType<NYql::TNullExprType>();
}

NKikimr::NMiniKQL::TType* MakeNullType(NKikimr::NMiniKQL::TTypeEnvironment& env)
{
    return env.GetTypeOfNull();
}

const NYql::TTypeAnnotationNode* MakeEmptyListType(NYql::TExprContext& ctx)
{
    return ctx.MakeType<NYql::TEmptyListExprType>();
}

NKikimr::NMiniKQL::TType* MakeEmptyListType(NKikimr::NMiniKQL::TTypeEnvironment& env)
{
    return env.GetTypeOfEmptyList();
}

const NYql::TTypeAnnotationNode* MakeEmptyDictType(NYql::TExprContext& ctx)
{
    return ctx.MakeType<NYql::TEmptyDictExprType>();
}

NKikimr::NMiniKQL::TType* MakeEmptyDictType(NKikimr::NMiniKQL::TTypeEnvironment& env)
{
    return env.GetTypeOfEmptyDict();
}

const NYql::TTypeAnnotationNode* MakeVariantType(const NYql::TTypeAnnotationNode* underlyingType, NYql::TExprContext& ctx)
{
    return ctx.MakeType<NYql::TVariantExprType>(underlyingType);
}

NKikimr::NMiniKQL::TType* MakeVariantType(NKikimr::NMiniKQL::TType* underlyingType, NKikimr::NMiniKQL::TTypeEnvironment& env)
{
    return TVariantType::Create(underlyingType, env);
}

const NYql::TTypeAnnotationNode* MakeTaggedType(const TString& tag, const NYql::TTypeAnnotationNode* underlyingType, NYql::TExprContext& ctx)
{
    return ctx.MakeType<NYql::TTaggedExprType>(underlyingType, tag);
}

NKikimr::NMiniKQL::TType* MakeTaggedType(const TString& tag, NKikimr::NMiniKQL::TType* underlyingType, NKikimr::NMiniKQL::TTypeEnvironment& env)
{
    return TTaggedType::Create(underlyingType, tag, env);
}

template<typename TType, typename TContext>
TType MakeType(NYdb::TTypeParser& parser, TContext& env)
{
    switch (parser.GetKind()) {
    case NYdb::TTypeParser::ETypeKind::Primitive: {
        return MakePrimitiveType(parser, env);
    }
    case NYdb::TTypeParser::ETypeKind::Decimal: {
        return MakeDecimalType(parser, env);
    }
    case NYdb::TTypeParser::ETypeKind::Optional: {
        parser.OpenOptional();
        auto underlying = MakeType<TType>(parser, env);
        if (!underlying) {
            return nullptr;
        }
        parser.CloseOptional();
        return MakeOptionalType(underlying, env);
    }
    case NYdb::TTypeParser::ETypeKind::List: {
        parser.OpenList();
        auto underlying = MakeType<TType>(parser, env);
        auto node = MakeListType(underlying, env);
        parser.CloseList();
        return node;
    }
    case NYdb::TTypeParser::ETypeKind::Struct: {
        TVector<std::pair<TString, TType>> items;
        parser.OpenStruct();
        TType node = nullptr;
        while (parser.TryNextMember()) {
            auto colName = parser.GetMemberName();
            node = MakeType<TType>(parser, env);
            if (!node) {
                break;
            }
            items.push_back({colName, node});
        }
        parser.CloseStruct();
        if (!node) {
            return nullptr;
        }

        return MakeStructType(items, env);
    }
    case NYdb::TTypeParser::ETypeKind::Tuple: {
        TVector<TType> items;
        TType node = nullptr;
        parser.OpenTuple();
        while (parser.TryNextElement()) {
            node = MakeType<TType>(parser, env);
            if (!node) {
                break;
            }
            items.push_back(node);
        }
        if (!node) {
            return nullptr;
        }
        parser.CloseTuple();
        return MakeTupleType(items, env);
    }
    case NYdb::TTypeParser::ETypeKind::Dict: {
        parser.OpenDict();
        parser.DictKey();
        auto* key = MakeType<TType>(parser, env);
        if (!key) {
            return nullptr;
        }
        parser.DictPayload();
        auto* payload = MakeType<TType>(parser, env);
        if (!payload) {
            return nullptr;
        }
        parser.CloseDict();
        return MakeDictType(key, payload, env);
    }
    case NYdb::TTypeParser::ETypeKind::Variant: {
        parser.OpenVariant();
        auto node = MakeVariantType(MakeType<TType>(parser, env), env);
        parser.CloseVariant();
        return node;
    }
    case NYdb::TTypeParser::ETypeKind::Tagged: {
        parser.OpenTagged();
        auto tag = parser.GetTag();
        auto node = MakeTaggedType(tag, MakeType<TType>(parser, env), env);
        parser.CloseTagged();
        return node;
    }
    case NYdb::TTypeParser::ETypeKind::Void: {
        return MakeVoidType(env);
    }
    case NYdb::TTypeParser::ETypeKind::Null: {
        return MakeNullType(env);
    }
    case NYdb::TTypeParser::ETypeKind::EmptyList: {
        return MakeEmptyListType(env);
    }
    case NYdb::TTypeParser::ETypeKind::EmptyDict: {
        return MakeEmptyDictType(env);
    }
    default:
        return nullptr;
    }
}

struct TTypePair {
    NKikimr::NMiniKQL::TType* MiniKQLType = nullptr; 
    const NYql::TTypeAnnotationNode* TypeAnnotation = nullptr; 
};

TTypePair FormatColumnType( 
    NJson::TJsonValue& root,
    NYdb::TType type,
    NKikimr::NMiniKQL::TTypeEnvironment& typeEnv,
    NYql::TExprContext& ctx)
{
    TTypePair result; 
    NYdb::TTypeParser parser(type);
    result.MiniKQLType = MakeType<NKikimr::NMiniKQL::TType*>(parser, typeEnv);
    result.TypeAnnotation = MakeType<const NYql::TTypeAnnotationNode*>(parser, ctx);
    // TODO: use
    // NKikimr::NMiniKQL::TType* BuildType(const TTypeAnnotationNode& annotation, NKikimr::NMiniKQL::TProgramBuilder& pgmBuilder, IOutputStream& err, bool withTagged = false);

    if (!result.MiniKQLType) {
        root = "Null";
        return result;
    }

    //NJson::ReadJsonTree(
    //    NJson2Yson::ConvertYson2Json(NYql::NCommon::WriteTypeToYson(result.MiniKQLType)),
    //    &root);

    NJson::ReadJsonTree(
        NJson2Yson::ConvertYson2Json(NYql::NCommon::WriteTypeToYson(result.TypeAnnotation)),
        &root);

    return result;
}

void FormatColumnValue(
    NJson::TJsonValue& root,
    const NYdb::TValue& value,
    NKikimr::NMiniKQL::TType* type,
    const THolderFactory& holderFactory)
{
    const Ydb::Value& rawProtoValue = NYdb::TProtoAccessor::GetProto(value);

    NYql::NUdf::TUnboxedValue unboxed = ImportValueFromProto(
        type,
        rawProtoValue,
        holderFactory);

    NJson::ReadJsonTree(
        NJson2Yson::ConvertYson2Json(NYql::NCommon::WriteYsonValue(unboxed, type)),
        &root);
}

} // namespace

TString FormatSchema(const YandexQuery::Schema& schema) 
{ 
    NYql::TExprContext ctx; 
    TVector<std::pair<TString, const NYql::TTypeAnnotationNode*>> typedColumns; 
    typedColumns.reserve(schema.column().size()); 
    for (const auto& c : schema.column()) { 
        NYdb::TTypeParser parser(NYdb::TType(c.type())); 
        auto typeAnnotation = MakeType<const NYql::TTypeAnnotationNode*>(parser, ctx); 
        typedColumns.emplace_back(c.name(), typeAnnotation); 
    } 
 
    return NYql::NCommon::WriteTypeToYson(MakeStructType(typedColumns, ctx), NYson::EYsonFormat::Text);
} 
 
void FormatResultSet(NJson::TJsonValue& root, const NYdb::TResultSet& resultSet)
{
    NYql::TExprContext ctx;
    NKikimr::NMiniKQL::TScopedAlloc alloc;
    NKikimr::NMiniKQL::TTypeEnvironment typeEnv(alloc);

    TMemoryUsageInfo memInfo("BuildYdbResultSet");
    THolderFactory holderFactory(alloc.Ref(), memInfo);


    NJson::TJsonValue& columns = root["columns"];
    const auto& columnsMeta = resultSet.GetColumnsMeta();

    TVector<TTypePair> columnTypes;
    columnTypes.resize(columnsMeta.size());

    int i = 0;
    for (const NYdb::TColumn& columnMeta : columnsMeta) {
        NJson::TJsonValue& column = columns.AppendValue(NJson::TJsonValue());
        column["name"] = columnMeta.Name;
        columnTypes[i++] = FormatColumnType(column["type"], columnMeta.Type, typeEnv, ctx);
    }

    NJson::TJsonValue& data = root["data"];
    data.SetType(NJson::JSON_ARRAY);

    NYdb::TResultSetParser rsParser(resultSet);
    while (rsParser.TryNextRow()) {
        NJson::TJsonValue& row = data.AppendValue(NJson::TJsonValue());
        for (size_t columnNum = 0; columnNum < columnsMeta.size(); ++columnNum) {
            const NYdb::TColumn& columnMeta = columnsMeta[columnNum];
            FormatColumnValue(
                row[columnMeta.Name],
                rsParser.GetValue(columnNum),
                columnTypes[columnNum].MiniKQLType,
                holderFactory);
        }
    }
}

} // namespace NYq
