LIBRARY() 
 
OWNER(g:kikimr) 
 
SRCS( 
    grpc_service.cpp 
) 
 
PEERDIR( 
    library/cpp/grpc/server
    ydb/core/grpc_services
    ydb/core/mind
    ydb/public/api/grpc
    ydb/public/lib/operation_id
) 
 
END() 
