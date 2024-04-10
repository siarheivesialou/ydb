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

    class TGetQueueUrlActor : public TActorBootstrapped<TGetQueueUrlActor> {
        using TBase = TActorBootstrapped<TGetQueueUrlActor>;

    public:
        TGetQueueUrlActor(NKikimr::NGRpcService::IRequestOpCtx* request);
        ~TGetQueueUrlActor() = default;

        void Bootstrap(const NActors::TActorContext& ctx);
    };

    TGetQueueUrlActor::TGetQueueUrlActor(NKikimr::NGRpcService::IRequestOpCtx* request)
        : TBase()
    {
        Y_UNUSED(request);
    }

    void TGetQueueUrlActor::Bootstrap(const NActors::TActorContext& ctx) {
        Y_UNUSED(ctx);
    }
}
