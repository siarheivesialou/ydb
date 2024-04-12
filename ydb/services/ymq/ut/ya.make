UNITTEST_FOR(ydb/services/ymq)

FORK_SUBTESTS()

SIZE(MEDIUM)

TIMEOUT(600)

SRCS(
    ymq_ut.cpp
)

PEERDIR(
    library/cpp/getopt
    ydb/library/grpc/client
    library/cpp/svnversion
    ydb/core/testlib/default
    ydb/services/ymq
    ydb/public/sdk/cpp/client/ydb_ymq
)

YQL_LAST_ABI_VERSION()

REQUIREMENTS(ram:11)

END()
