#include <ydb/services/lib/sharding/sharding.h>
#include <ydb/services/ydb/ydb_common_ut.h>
#include <ydb/services/ydb/ydb_keys_ut.h>

#include <ydb/public/sdk/cpp/client/ydb_ymq/ymq.h>
// #include <ydb/public/sdk/cpp/client/ydb_topic/topic.h>
// #include <ydb/public/sdk/cpp/client/ydb_persqueue_public/persqueue.h>
// #include <ydb/public/sdk/cpp/client/ydb_types/status_codes.h>
// #include <ydb/public/sdk/cpp/client/ydb_table/table.h>
// #include <ydb/public/sdk/cpp/client/ydb_scheme/scheme.h>
#include <ydb/public/api/grpc/draft/ydb_ymq_v1.grpc.pb.h>
#include <ydb/public/sdk/cpp/client/ydb_ymq/ymq.h>

#include <library/cpp/json/json_reader.h>
#include <library/cpp/digest/md5/md5.h>

#include <util/system/tempfile.h>

#include <random>


using namespace NYdb;
using namespace NYdb::NTable;
namespace YMQ_V1 = Ydb::Ymq::V1;
struct WithSslAndAuth : TKikimrTestSettings {
    static constexpr bool SSL = true;
    static constexpr bool AUTH = true;
};
using TKikimrWithGrpcAndRootSchemaSecure = NYdb::TBasicKikimrWithGrpcAndRootSchema<WithSslAndAuth>;

static constexpr const char NON_CHARGEABLE_USER[] = "superuser@builtin";
static constexpr const char NON_CHARGEABLE_USER_X[] = "superuser_x@builtin";
static constexpr const char NON_CHARGEABLE_USER_Y[] = "superuser_y@builtin";

static constexpr const char DEFAULT_CLOUD_ID[] = "somecloud";
static constexpr const char DEFAULT_FOLDER_ID[] = "somefolder";

template<class TKikimr, bool secure>
class TYmqTestServer {
public:
    TYmqTestServer() {
        NKikimrConfig::TAppConfig appConfig;
        appConfig.MutablePQConfig()->SetTopicsAreFirstClassCitizen(true);
        appConfig.MutablePQConfig()->SetEnabled(true);
        // NOTE(shmel1k@): KIKIMR-14221
        appConfig.MutablePQConfig()->SetCheckACL(false);
        appConfig.MutablePQConfig()->SetRequireCredentialsInNewProtocol(false);

        auto cst = appConfig.MutablePQConfig()->AddClientServiceType();
        cst->SetName("data-transfer");
        cst = appConfig.MutablePQConfig()->AddClientServiceType();
        cst->SetName("data-transfer2");


        appConfig.MutablePQConfig()->MutableQuotingConfig()->SetEnableQuoting(true);
        appConfig.MutablePQConfig()->MutableQuotingConfig()->SetQuotaWaitDurationMs(300);
        appConfig.MutablePQConfig()->MutableQuotingConfig()->SetPartitionReadQuotaIsTwiceWriteQuota(true);
        appConfig.MutablePQConfig()->MutableBillingMeteringConfig()->SetEnabled(true);
        appConfig.MutablePQConfig()->MutableBillingMeteringConfig()->SetFlushIntervalSec(1);
        appConfig.MutablePQConfig()->AddClientServiceType()->SetName("data-streams");
        appConfig.MutablePQConfig()->AddNonChargeableUser(NON_CHARGEABLE_USER);
        appConfig.MutablePQConfig()->AddNonChargeableUser(NON_CHARGEABLE_USER_X);
        appConfig.MutablePQConfig()->AddNonChargeableUser(NON_CHARGEABLE_USER_Y);

        appConfig.MutablePQConfig()->AddValidWriteSpeedLimitsKbPerSec(128);
        appConfig.MutablePQConfig()->AddValidWriteSpeedLimitsKbPerSec(512);
        appConfig.MutablePQConfig()->AddValidWriteSpeedLimitsKbPerSec(1_KB);

        auto limit = appConfig.MutablePQConfig()->AddValidRetentionLimits();
        limit->SetMinPeriodSeconds(0);
        limit->SetMaxPeriodSeconds(TDuration::Days(1).Seconds());
        limit->SetMinStorageMegabytes(0);
        limit->SetMaxStorageMegabytes(0);

        limit = appConfig.MutablePQConfig()->AddValidRetentionLimits();
        limit->SetMinPeriodSeconds(0);
        limit->SetMaxPeriodSeconds(TDuration::Days(7).Seconds());
        limit->SetMinStorageMegabytes(50_KB);
        limit->SetMaxStorageMegabytes(1_MB);

        MeteringFile = MakeHolder<TTempFileHandle>();
        appConfig.MutableMeteringConfig()->SetMeteringFilePath(MeteringFile->Name());

        if (secure) {
            appConfig.MutablePQConfig()->SetRequireCredentialsInNewProtocol(true);
        }
        KikimrServer = std::make_unique<TKikimr>(std::move(appConfig));
        ui16 grpc = KikimrServer->GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        auto driverConfig = TDriverConfig().SetEndpoint(location).SetLog(CreateLogBackend("cerr", TLOG_DEBUG));
        if (secure) {
            driverConfig.UseSecureConnection(TString(NYdbSslTestData::CaCrt));
        } else {
            driverConfig.SetDatabase("/Root/");
        }

        Driver = std::make_unique<TDriver>(std::move(driverConfig));
        YmqClient = std::make_unique<NYdb::Ymq::V1::TYmqClient>(*Driver);

        {
            NYdb::NScheme::TSchemeClient schemeClient(*Driver);
            NYdb::NScheme::TPermissions permissions("user@builtin", {"ydb.generic.read", "ydb.generic.write"});

            auto result = schemeClient.ModifyPermissions("/Root",
                NYdb::NScheme::TModifyPermissionsSettings().AddGrantPermissions(permissions)
            ).ExtractValueSync();
            Cerr << result.GetIssues().ToString() << "\n";
            UNIT_ASSERT(result.IsSuccess());
        }

        TClient client(*(KikimrServer->ServerSettings));
        UNIT_ASSERT_VALUES_EQUAL(NMsgBusProxy::MSTATUS_OK,
                                 client.AlterUserAttributes("/", "Root", {{"folder_id", DEFAULT_FOLDER_ID},
                                                                          {"cloud_id", DEFAULT_CLOUD_ID},
                                                                          {"database_id", "root"}}));
    }

public:
    std::unique_ptr<TKikimr> KikimrServer;
    std::unique_ptr<TDriver> Driver;
    std::unique_ptr<NYdb::Ymq::V1::TYmqClient> YmqClient;
    std::unique_ptr<NYdb::Ymq::V1::TYmqClient> UnauthenticatedClient;
    THolder<TTempFileHandle> MeteringFile;
};

using TInsecureYmqTestServer = TYmqTestServer<TKikimrWithGrpcAndRootSchema, false>;
using TSecureYmqTestServer = TYmqTestServer<TKikimrWithGrpcAndRootSchemaSecure, true>;


#define Y_UNIT_TEST_NAME this->Name_;


#define SET_YDS_LOCALS                               \
    auto& kikimr = testServer.KikimrServer->Server_; \
    Y_UNUSED(kikimr);                                \
    auto& driver = testServer.Driver;                \
    Y_UNUSED(driver);                                \


Y_UNIT_TEST_SUITE(Ymq) {

    Y_UNIT_TEST(TestGetQueueUrl) {
        TInsecureYmqTestServer testServer;
        const TString streamName = TStringBuilder() << "stream_" << Y_UNIT_TEST_NAME;
        const TString streamName2 = TStringBuilder() << "tdir/stream_" << Y_UNIT_TEST_NAME;
        const TString streamName3 = TStringBuilder() << "tdir/table/feed_" << Y_UNIT_TEST_NAME;
        const TString tableName = "tdir/table";
        const TString feedName = TStringBuilder() << "feed_" << Y_UNIT_TEST_NAME;

        {
            NYdb::NTopic::TTopicClient pqClient(*testServer.Driver);
            auto result = pqClient.CreateTopic(streamName2).ExtractValueSync();
            UNIT_ASSERT_VALUES_EQUAL(result.IsTransportError(), false);
            UNIT_ASSERT_VALUES_EQUAL(result.GetStatus(), EStatus::SUCCESS);
        }

        {
            NYdb::NTable::TTableClient tableClient(*testServer.Driver);
            tableClient.RetryOperationSync([&](TSession session)
                {
                    NYdb::NTable::TTableBuilder builder;
                    builder.AddNonNullableColumn("key", NYdb::EPrimitiveType::String).SetPrimaryKeyColumn("key");

                    auto result = session.CreateTable("/Root/" + tableName, builder.Build()).ExtractValueSync();
                    UNIT_ASSERT_VALUES_EQUAL(result.IsTransportError(), false);
                    Cerr << result.GetIssues().ToString() << "\n";
                    UNIT_ASSERT_VALUES_EQUAL(result.GetStatus(), EStatus::SUCCESS);

                    auto result2 = session.AlterTable("/Root/" + tableName, NYdb::NTable::TAlterTableSettings()
                                    .AppendAddChangefeeds(NYdb::NTable::TChangefeedDescription(feedName,
                                                                                           NYdb::NTable::EChangefeedMode::Updates,
                                                                                           NYdb::NTable::EChangefeedFormat::Json))
                                                     ).ExtractValueSync();
                    Cerr << result2.GetIssues().ToString() << "\n";
                    UNIT_ASSERT_VALUES_EQUAL(result2.IsTransportError(), false);
                    UNIT_ASSERT_VALUES_EQUAL(result2.GetStatus(), EStatus::SUCCESS);
                    return result2;
                }
            );
        }

        // Trying to delete stream that doesn't exist yet

        {
            NYdb::Ymq::V1::TGetQueueUrlSettings settings;
            auto result = testServer.YmqClient->GetQueueUrl("foobar", settings).ExtractValueSync();
            Cerr << result.GetIssues().ToString() << "\n";
            Cerr << std::to_string(static_cast<size_t>(result.GetStatus())) << "\n";
            UNIT_ASSERT_VALUES_EQUAL(result.IsTransportError(), false);
            UNIT_ASSERT_VALUES_EQUAL(result.GetStatus(), EStatus::SUCCESS);
        }
    }
}
