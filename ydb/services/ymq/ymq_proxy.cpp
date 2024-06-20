#include "ymq_proxy.h"
// #include "shard_iterator.h"
// #include "next_token.h"

#include <ydb/core/grpc_services/service_ymq.h>
#include <ydb/core/grpc_services/grpc_request_proxy.h>
#include <ydb/core/grpc_services/rpc_deferrable.h>
#include <ydb/core/grpc_services/rpc_scheme_base.h>
#include <ydb/core/persqueue/partition.h>
#include <ydb/core/persqueue/pq_rl_helpers.h>
#include <ydb/core/persqueue/write_meta.h>

#include <ydb/public/api/protos/ydb_topic.pb.h>
#include <ydb/services/lib/actors/pq_schema_actor.h>
#include <ydb/services/lib/sharding/sharding.h>
#include <ydb/services/persqueue_v1/actors/persqueue_utils.h>

#include <util/folder/path.h>
#include <ydb/core/ymq/actor/auth_factory.h>

#include <iterator>

using namespace NActors;
using namespace NKikimrClient;

using grpc::Status;



namespace NKikimr::NYmq::V1 {

    using namespace NGRpcService;
    using namespace NGRpcProxy::V1;

    namespace {

        template <class TRequest>
        const TRequest* GetRequest(NGRpcService::IRequestOpCtx *request)
        {
            return dynamic_cast<const TRequest*>(request->GetRequest());
        }
    }

    class TYmqReplyCallback : public NSQS::IReplyCallback {
        public:
            TYmqReplyCallback(std::shared_ptr<NKikimr::NGRpcService::IRequestOpCtx> request)
                : Request(request)
            {
                Cerr << "KLACK TYmqReplyCallback::TYmqReplyCallback()\n";
            }
            void DoSendReply(const NKikimrClient::TSqsResponse& resp) {
                if (resp.GetGetQueueUrl().HasError()) {
                    NYql::TIssue issue(resp.GetGetQueueUrl().GetError().GetErrorCode());
                    issue.SetCode(3, NYql::ESeverity::TSeverityIds_ESeverityId_S_ERROR);
                    issue.SetMessage(resp.GetGetQueueUrl().GetError().GetMessage());
                    Cerr << "KLACK resp.GetGetQueueUrl().GetError().GetErrorCode() == " << resp.GetGetQueueUrl().GetError().GetErrorCode() << "  \n";
                    Cerr << "KLACK resp.GetGetQueueUrl().GetError().GetMessage() == " << resp.GetGetQueueUrl().GetError().GetMessage() << "  \n";
                    Cerr << "KLACK resp.GetGetQueueUrl().GetError().GetStatus() == " << resp.GetGetQueueUrl().GetError().GetStatus() << "  \n";
                    Request->RaiseIssue(issue);
                } else {
                    Ydb::Ymq::V1::GetQueueUrlResult result;
                    result.Setqueue_url(resp.GetGetQueueUrl().GetQueueUrl());
                    Request->SendResult(result, Ydb::StatusIds::StatusCode::StatusIds_StatusCode_SUCCESS);
                }
                Cerr << "KLACK TYmqReplyCallback::DoSendReply(): " << resp.AsJSON() << "\n";
                //TODO: возвращать правильный ответ
            }
        private:
            std::shared_ptr<NKikimr::NGRpcService::IRequestOpCtx> Request;
    };

    class TGetQueueUrlActor : public TRpcRequestWithOperationParamsActor<TGetQueueUrlActor, TEvYmqGetQueueUrlRequest, true> {
        using TBase = TRpcRequestWithOperationParamsActor<TGetQueueUrlActor, TEvYmqGetQueueUrlRequest, true>;

    public:
        TGetQueueUrlActor(NKikimr::NGRpcService::IRequestOpCtx* request);
        ~TGetQueueUrlActor() = default;

        void Bootstrap(const NActors::TActorContext& ctx);

    private:
        const TString FolderId;
        const TString CloudId;
        const TString UserSid;
    };

    TGetQueueUrlActor::TGetQueueUrlActor(NKikimr::NGRpcService::IRequestOpCtx* request)
        : TBase(request)
        , FolderId(request->GetPeerMetaValues("folderId").GetOrElse(""))
        , CloudId(request->GetPeerMetaValues("cloudId").GetOrElse(""))
        , UserSid(request->GetPeerMetaValues("userSid").GetOrElse(""))
    {
        Cerr << "KLACK TGetQueueUrlActor::TGetQueueUrlActor(): FolderId == " << FolderId << ", CloudId == " << CloudId << ", UserSid == " << UserSid << "\n";
        Y_UNUSED(request);
    }

    void TGetQueueUrlActor::Bootstrap(const NActors::TActorContext& ctx) {
        Y_UNUSED(ctx);
        Cerr << "KLACK TGetQueueUrlActor::Bootstrap()\n";
        auto requestHolder = MakeHolder<TSqsRequest>();
        //TODO: заменить
        requestHolder->SetRequestId("changeRequestid"); // добавить в прото?
        requestHolder->MutableGetQueueUrl()->SetQueueName(GetProtoRequest()->queue_name());
        requestHolder->MutableGetQueueUrl()->MutableAuth()->SetUserName(CloudId);
        requestHolder->MutableGetQueueUrl()->MutableAuth()->SetFolderId(FolderId);
        requestHolder->MutableGetQueueUrl()->MutableAuth()->SetUserSID(UserSid);
        // this->Request_->ReplyWithYdbStatus(Ydb::StatusIds::SUCCESS);

        // NSQS::TAuthActorData data {
        //     .SQSRequest = std::move(requestHolder),
        //     .HTTPCallback = std::move(httpCallback),
        //     .UserSidCallback = [](const TString& userSid) { Y_UNUSED(userSid); },
        //     //TODO: что тут должно быть?
        //     .EnableQueueLeader = true,
        //     .Action = NSQS::GetQueueUrl,
        //     .ExecutorPoolID = Parent_->PoolId_,
        //     .CloudID = "cloud4", // добавить в прото?
        //     .ResourceID = GetProtoRequest()->queue_name(),
        //     // .Counters = Parent_->CloudAuthCounters_.Get(),
        //     .Counters = Parent_->CloudAuthCounters_.Get(),
        //     .AWSSignature = std::move(AwsSignature_),
        //     .IAMToken = IamToken_, // добавить в прото?
        //     .FolderID = "folder4"// добавить в прото?
        // };

        // AppData(ctx.ActorSystem())->SqsAuthFactory->RegisterAuthActor(
        //     *ctx.ActorSystem(),
        //     std::move(data));

        auto replyCallback = MakeHolder<TYmqReplyCallback>(Request_);
        auto actor = CreateProxyActionActor(*requestHolder.Release(), std::move(replyCallback), true);
        //TODO: executorPool == 0 верно?
        ctx.ActorSystem()->Register(actor, NActors::TMailboxType::HTSwap, 0);
    }
}

namespace NKikimr::NGRpcService {

using namespace NYmq::V1;

#define DECLARE_RPC(name) template<> IActor* TEvYmq##name##Request::CreateRpcActor(NKikimr::NGRpcService::IRequestOpCtx* msg) { \
    return new T##name##Actor(msg);\
}\
void DoYmq##name##Request(std::unique_ptr<IRequestOpCtx> p, const IFacilityProvider&) {\
    TActivationContext::AsActorContext().Register(new T##name##Actor(p.release())); \
}

#define DECLARE_RPC_NI(name) template<> IActor* TEvYmq##name##Request::CreateRpcActor(NKikimr::NGRpcService::IRequestOpCtx* msg) { \
    return new TNotImplementedRequestActor<NKikimr::NGRpcService::TEvYmq##name##Request>(msg);\
}\
void DoDataStreams##name##Request(std::unique_ptr<IRequestOpCtx> p, const IFacilityProvider&) {\
    TActivationContext::AsActorContext().Register(new TNotImplementedRequestActor<NKikimr::NGRpcService::TEvYmq##name##Request>(p.release()));\
}

DECLARE_RPC(GetQueueUrl);

}
