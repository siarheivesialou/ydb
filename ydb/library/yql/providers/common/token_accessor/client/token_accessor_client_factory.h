#pragma once 
 
#include <ydb/public/sdk/cpp/client/ydb_types/credentials/credentials.h>
#include <util/datetime/base.h> 
 
namespace NYql { 
 
std::shared_ptr<NYdb::ICredentialsProviderFactory> CreateTokenAccessorCredentialsProviderFactory( 
    const TString& tokenAccessorEndpoint, 
    bool useSsl, 
    const TString& serviceAccountId, 
    const TString& serviceAccountIdSignature, 
    const TDuration& refreshPeriod = TDuration::Hours(1), 
    const TDuration& requestTimeout = TDuration::Seconds(10) 
); 
 
} 
 
