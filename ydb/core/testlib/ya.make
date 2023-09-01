LIBRARY()

SRCS(
    actor_helpers.cpp
    actor_helpers.h
    common_helper.cpp
    cs_helper.cpp
    fake_coordinator.cpp
    fake_coordinator.h
    fake_scheme_shard.h
    minikql_compile.h
    mock_pq_metacache.h
    tablet_flat_dummy.cpp
    tablet_helpers.cpp
    tablet_helpers.h
    tenant_runtime.cpp
    tenant_runtime.h
    test_client.cpp
    test_client.h
    tx_helpers.cpp
    tx_helpers.h
)

PEERDIR(
    library/cpp/actors/core
    library/cpp/actors/interconnect
    library/cpp/grpc/client
    library/cpp/grpc/server
    library/cpp/grpc/server/actors
    library/cpp/regex/pcre
    library/cpp/testing/gmock_in_unittest
    library/cpp/testing/unittest
    ydb/core/driver_lib/run
    ydb/core/base
    ydb/core/blobstorage/base
    ydb/core/blobstorage/pdisk
    ydb/core/client
    ydb/core/client/metadata
    ydb/core/client/minikql_compile
    ydb/core/client/server
    ydb/core/cms/console
    ydb/core/engine
    ydb/core/engine/minikql
    ydb/core/formats
    ydb/core/fq/libs/init
    ydb/core/fq/libs/mock
    ydb/core/fq/libs/shared_resources
    ydb/core/grpc_services
    ydb/core/health_check
    ydb/core/kesus/proxy
    ydb/core/kesus/tablet
    ydb/core/keyvalue
    ydb/core/kqp
    ydb/core/kqp/federated_query
    ydb/core/metering
    ydb/core/mind
    ydb/core/mind/address_classification
    ydb/core/mind/bscontroller
    ydb/core/mind/hive
    ydb/core/node_whiteboard
    ydb/core/persqueue
    ydb/core/protos
    ydb/core/security
    ydb/core/sys_view/processor
    ydb/core/sys_view/service
    ydb/core/testlib/actors
    ydb/core/testlib/basics
    ydb/core/tx/columnshard
    ydb/core/tx/coordinator
    ydb/core/tx/long_tx_service
    ydb/core/tx/mediator
    ydb/core/tx/replication/controller
    ydb/core/tx/schemeshard
    ydb/core/tx/sequenceproxy
    ydb/core/tx/sequenceshard
    ydb/core/tx/time_cast
    ydb/library/aclib
    ydb/library/folder_service/mock
    ydb/library/mkql_proto/protos
    ydb/library/persqueue/topic_parser
    ydb/library/security
    ydb/library/yql/minikql/comp_nodes/llvm
    ydb/library/yql/public/udf/service/exception_policy
    ydb/public/lib/base
    ydb/public/lib/deprecated/kicli
    ydb/public/sdk/cpp/client/ydb_driver
    ydb/public/sdk/cpp/client/ydb_persqueue_public/codecs
    ydb/public/sdk/cpp/client/ydb_table
    ydb/services/auth
    ydb/services/cms
    ydb/services/datastreams
    ydb/services/discovery
    ydb/services/ext_index/service
    ydb/core/tx/conveyor/service
    ydb/services/fq
    ydb/services/kesus
    ydb/services/persqueue_cluster_discovery
    ydb/services/persqueue_v1
    ydb/services/rate_limiter
    ydb/services/monitoring
    ydb/services/metadata/ds_table
    ydb/services/bg_tasks/ds_table
    ydb/services/bg_tasks
    ydb/services/ydb

    ydb/core/http_proxy
)

YQL_LAST_ABI_VERSION()

IF (GCC)
    CFLAGS(
        -fno-devirtualize-speculatively
    )
ENDIF()

END()

RECURSE(
    actors
    basics
    default
    pg
)
