#include "rafty/rpcService.hpp"

#include <grpcpp/grpcpp.h>
#include "raft.grpc.pb.h"

using grpc::Server;
using grpc::ServerContext;
using grpc::Status;

#include "rafty/raft.hpp"

namespace rafty
{
    RaftServiceImpl::RaftServiceImpl(rafty::Raft &raftNode) : raftpb::RaftService::Service(), raftNode_(raftNode) {}

    Status RaftServiceImpl::AppendEntries(ServerContext *context,
                                          const raftpb::AppendEntriesRequest *request,
                                          raftpb::AppendEntriesReply *reply)
    {
        raftNode_.append_entries(request, reply);

        return grpc::Status::OK;
    }

    Status RaftServiceImpl::RequestVote(ServerContext *context,
                                        const raftpb::RequestVoteRequest *request,
                                        raftpb::RequestVoteReply *reply)
    {
        raftNode_.reply_vote(request, reply);
        return Status::OK;
    }

}
