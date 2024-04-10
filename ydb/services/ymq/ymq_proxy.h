#pragma once

// #include "events.h"

#include <ydb/public/api/grpc/draft/ydb_ymq_v1.pb.h>

#include <ydb/core/client/server/grpc_base.h>
#include <ydb/core/grpc_services/rpc_calls.h>

#include <ydb/library/grpc/server/grpc_server.h>
#include <ydb/library/actors/core/actor_bootstrapped.h>
#include <ydb/library/actors/core/actorsystem.h>


namespace NKikimr {
namespace NGRpcService {

using TEvYmqGetQueueUrlRequest = TGrpcRequestOperationCall<Ydb::Ymq::V1::GetQueueUrlRequest, Ydb::Ymq::V1::GetQueueUrlResponse>;

}
}
