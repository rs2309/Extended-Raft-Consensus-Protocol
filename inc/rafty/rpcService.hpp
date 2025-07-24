#ifndef RPC_SERVICE_H
#define RPC_SERVICE_H

#include <grpcpp/grpcpp.h>
#include "raft.grpc.pb.h"

using grpc::Server;
using grpc::ServerContext;
using grpc::Status;

//cyclic
namespace rafty {
    class Raft;
}

namespace rafty {
class RaftServiceImpl final: public raftpb::RaftService::Service{
    public:
        // RaftServiceImpl()
        //     : raftpb::RaftService::Service(){}
        explicit RaftServiceImpl(Raft& raftNode);

    private:
        Status AppendEntries(ServerContext* context,
                            const raftpb::AppendEntriesRequest* request,
                            raftpb::AppendEntriesReply* reply) override;
        Status RequestVote(ServerContext* context,
                            const raftpb::RequestVoteRequest* request,
                            raftpb::RequestVoteReply* reply) override;

        // grpc::Server* grpcServerPtr;
        Raft& raftNode_; 
};
}

#endif // RPC_SERVICE_H
