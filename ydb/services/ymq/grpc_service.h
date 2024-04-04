#pragma once

#include <ydb/library/actors/core/actorsystem.h>
#include <ydb/library/grpc/server/grpc_server.h>
#include <ydb/public/api/protos/draft/ymq.pb.h>
#include <ydb/core/grpc_services/base/base_service.h>

namespace NKikimr::NGRpcService {

    class TGRpcYmqService : public TGrpcServiceBase<Ydb::YMQ::>
    {
    public:
        using TGrpcServiceBase<Ydb::DataStreams::V1::DataStreamsService>::TGrpcServiceBase;
    private:
        void SetupIncomingRequests(NYdbGrpc::TLoggerPtr logger);
    };

}
