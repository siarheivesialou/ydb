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

    class TGetQueueUrlActor : public TRpcRequestWithOperationParamsActor<TGetQueueUrlActor, TEvYmqGetQueueUrlRequest, true> {
        using TBase = TRpcRequestWithOperationParamsActor<TGetQueueUrlActor, TEvYmqGetQueueUrlRequest, true>;

    public:
        TGetQueueUrlActor(NKikimr::NGRpcService::IRequestOpCtx* request);
        ~TGetQueueUrlActor() = default;

        void Bootstrap(const NActors::TActorContext& ctx);
    };

    TGetQueueUrlActor::TGetQueueUrlActor(NKikimr::NGRpcService::IRequestOpCtx* request)
        : TBase(request)
    {
        Y_UNUSED(request);
    }

    void TGetQueueUrlActor::Bootstrap(const NActors::TActorContext& ctx) {
        // auto requestHolder = MakeHolder<TSqsRequest>();
        // requestHolder->SetRequestId(); // добавить в прото?
        // requestHolder->MutableGetQueueUrl()->SetQueueName(GetProtoRequest()->queue_name());
        // requestHolder->MutableGetQueueUrl()->MutableAuth()->SetUserName(GetProtoRequest()->); // добавить в прото?
        // this->Request_->ReplyWithYdbStatus(Ydb::StatusIds::SUCCESS);

        // TAuthActorData data {
        //     .SQSRequest = std::move(requestHolder),
        //     .HTTPCallback = std::move(httpCallback),
        //     .UserSidCallback = [this](const TString& userSid) { UserSid_ = userSid; },
        //     .EnableQueueLeader = enableQueueLeader,
        //     .Action = Action_,
        //     .ExecutorPoolID = Parent_->PoolId_,
        //     .CloudID = AccountName_, // добавить в прото?
        //     .ResourceID = QueueName_, // знаем (в случае других action - извлекать из queueUrl)
        //     .Counters = Parent_->CloudAuthCounters_.Get(),
        //     .AWSSignature = std::move(AwsSignature_),
        //     .IAMToken = IamToken_, // добавить в прото?
        //     .FolderID = FolderId_ // добавить в прото?
        // };

        // AppData(Parent_->ActorSystem_)->SqsAuthFactory->RegisterAuthActor(
        //     *Parent_->ActorSystem_,
        //     std::move(data));
        // }
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
