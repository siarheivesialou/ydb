#pragma once
#include <memory>

namespace NActors {
struct TActorId;
}

namespace NKikimr {
namespace NGRpcService {

class IRequestOpCtx;
class IFacilityProvider;

void DoYmqGetQueueUrlRequest(std::unique_ptr<IRequestOpCtx> p, const IFacilityProvider& f);

}
}
