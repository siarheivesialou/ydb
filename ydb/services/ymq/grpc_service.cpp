#include "grpc_service.h"

#include <ydb/core/base/counters.h>
#include <ydb/core/base/appdata.h>
#include <ydb/core/base/ticket_parser.h>
#include <ydb/core/grpc_services/grpc_helper.h>
#include <ydb/core/grpc_services/base/base.h>
#include <ydb/core/tx/scheme_board/cache.h>

namespace {

using namespace NKikimr;

void YdsProcessAttr(const TSchemeBoardEvents::TDescribeSchemeResult& schemeData, NGRpcService::ICheckerIface* checker) {
    static const std::vector<TString> allowedAttributes = {"folder_id", "service_account_id", "database_id"};
    //full list of permissions for compatibility. remove old permissions later.
    static const TVector<TString> permissions = {
        "ydb.streams.write",
        "ydb.databases.list",
        "ydb.databases.create",
        "ydb.databases.connect"
    };
    TVector<std::pair<TString, TString>> attributes;
    attributes.reserve(schemeData.GetPathDescription().UserAttributesSize());
    for (const auto& attr : schemeData.GetPathDescription().GetUserAttributes()) {
        if (std::find(allowedAttributes.begin(), allowedAttributes.end(), attr.GetKey()) != allowedAttributes.end()) {
            attributes.emplace_back(attr.GetKey(), attr.GetValue());
        }
    }
    if (!attributes.empty()) {
        checker->SetEntries({{permissions, attributes}});
    }
}

}

namespace NKikimr::NGRpcService {

void TGRpcYmqService::SetupIncomingRequests(NYdbGrpc::TLoggerPtr logger)
{
    auto getCounterBlock = CreateCounterCb(Counters_, ActorSystem_);
    using std::placeholders::_1;
    using std::placeholders::_2;

#ifdef ADD_REQUEST
#error ADD_REQUEST macro already defined
#endif
//TODO: убрать дублирование с DataStreams
#define ADD_REQUEST(NAME, CB, ATTR, LIMIT_TYPE) \
    MakeIntrusive<TGRpcRequest<Ydb::YMQ::NAME##Request, Ydb::YMQ::NAME##Response, TGRpcYmqService>> \
        (this, &Service_, CQ_,                                                                                                      \
            [this](NYdbGrpc::IRequestContextBase *ctx) {                                                                               \
                NGRpcService::ReportGrpcReqToMon(*ActorSystem_, ctx->GetPeer());                                                    \
                ActorSystem_->Send(GRpcRequestProxyId_,                                                                             \
                    new TGrpcRequestOperationCall<Ydb::YMQ::NAME##Request, Ydb::YMQ::NAME##Response>        \
                        (ctx, CB, TRequestAuxSettings{RLSWITCH(TRateLimiterMode::LIMIT_TYPE), ATTR}));                              \
            }, &Ydb::YMQ::V1::YmqService::AsyncService::Request ## NAME,                                            \
            #NAME, logger, getCounterBlock("data_streams", #NAME))->Run();

    ADD_REQUEST(GetQueueUrl, Do, nullptr, Off)

#undef ADD_REQUEST
}

}
