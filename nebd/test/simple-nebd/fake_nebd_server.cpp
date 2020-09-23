#include <brpc/channel.h>
#include <brpc/closure_guard.h>
#include <brpc/controller.h>
#include <brpc/server.h>
#include <bvar/bvar.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include <memory>

#include "nebd/proto/client.pb.h"
#include "nebd/proto/heartbeat.pb.h"
#include "proto/chunk.pb.h"

DEFINE_bool(forward, true, "forward request to chunkserver");

enum class OpType { Write, Read };

using nebd::client::RetCode;

std::vector<std::unique_ptr<brpc::Channel>> channels;

std::vector<std::string> hosts = {"10.182.26.46", "10.182.26.47",
                                  "10.182.26.48"};

std::vector<int> ports = {8200, 8201, 8202, 8203, 8204,
                          8205, 8206, 8207, 8208, 8209};

bvar::LatencyRecorder rpcLatency("rpc_lat");
bvar::LatencyRecorder writeSize("write_size");
bvar::LatencyRecorder readSize("read_size");

void init_channels() {
    for (const auto& host : hosts) {
        for (const auto port : ports) {
            std::string address = host + ":" + std::to_string(port);
            auto channel = std::make_unique<brpc::Channel>();
            if (channel->Init(address.c_str(), nullptr) != 0) {
                LOG(FATAL) << "init channel to: " << address << " failed";
            }

            channels.emplace_back(std::move(channel));
        }
    }
}

thread_local static unsigned int rand_seed = time(nullptr);

brpc::Channel* random_choice_one_channel() {
    int idx = rand_r(&rand_seed) % channels.size();
    return channels[idx].get();
}

class ForwardClosure : public google::protobuf::Closure {
 public:
    ForwardClosure(OpType type, google::protobuf::RpcController* cntl_base,
                   const google::protobuf::Message* request,
                   google::protobuf::Message* response,
                   google::protobuf::Closure* done)
        : type(type),
          orig_cntl(cntl_base),
          orig_req(request),
          orig_resp(response),
          orig_done(done) {}

    void Run() override {
        std::unique_ptr<ForwardClosure> self_guard(this);
        brpc::ClosureGuard done_guard(orig_done);

        if (cntl.Failed()) {
            LOG(ERROR) << "rpc failed, error: " << cntl.ErrorText();
        } else {
            rpcLatency << cntl.latency_us();
        }

        if (OpType::Write == type) {
            static_cast<nebd::client::WriteResponse*>(orig_resp)->set_retcode(
                nebd::client::RetCode::kOK);
        } else {
            static_cast<nebd::client::ReadResponse*>(orig_resp)->set_retcode(
                nebd::client::RetCode::kOK);

            // append read data to origin request
            static_cast<brpc::Controller*>(orig_cntl)
                ->response_attachment()
                .append(cntl.response_attachment());
        }
    }

    OpType type;
    brpc::Controller cntl;
    curve::chunkserver::ChunkRequest req;
    curve::chunkserver::ChunkResponse resp;

    google::protobuf::RpcController* orig_cntl;
    const google::protobuf::Message* orig_req;
    google::protobuf::Message* orig_resp;
    google::protobuf::Closure* orig_done;
};

void ForwardRequest(ForwardClosure* done) {
    done->cntl.set_timeout_ms(-1);
    done->req.set_optype(
        done->type == OpType::Write
            ? curve::chunkserver::CHUNK_OP_TYPE::CHUNK_OP_WRITE
            : curve::chunkserver::CHUNK_OP_TYPE::CHUNK_OP_READ);
    done->req.set_logicpoolid(10000);
    done->req.set_copysetid(10000);
    done->req.set_chunkid(10000);
    done->req.set_offset(4 * 1024 * 16);
    done->req.set_size(4 * 1024 * 16);

    if (OpType::Write == done->type) {
        // append write data
        done->cntl.request_attachment().append(
            static_cast<brpc::Controller*>(done->orig_cntl)
                ->request_attachment());
    }

    curve::chunkserver::ChunkService_Stub stub(random_choice_one_channel());
    stub.ReadChunk(&done->cntl, &done->req, &done->resp, done);
}

namespace nebd {
namespace server {

class NebdHeartbeatServiceImpl : public nebd::client::NebdHeartbeatService {
 public:
    NebdHeartbeatServiceImpl() = default;
    ~NebdHeartbeatServiceImpl() = default;

    void KeepAlive(google::protobuf::RpcController* cntl_base,
                   const nebd::client::HeartbeatRequest* request,
                   nebd::client::HeartbeatResponse* response,
                   google::protobuf::Closure* done) override {
        brpc::ClosureGuard guard(done);
        response->set_retcode(nebd::client::RetCode::kOK);
    }
};

class NebdFileServiceImpl : public nebd::client::NebdFileService {
 public:
    NebdFileServiceImpl() = default;
    ~NebdFileServiceImpl() = default;

    void OpenFile(google::protobuf::RpcController* cntl_base,
                  const nebd::client::OpenFileRequest* request,
                  nebd::client::OpenFileResponse* response,
                  google::protobuf::Closure* done) override {
        brpc::ClosureGuard guard(done);
        response->set_retcode(RetCode::kOK);
        response->set_fd(1);
    }

    void Write(google::protobuf::RpcController* cntl_base,
               const nebd::client::WriteRequest* request,
               nebd::client::WriteResponse* response,
               google::protobuf::Closure* done) override {
        if (FLAGS_forward) {
            // just forward request to chunkserver
            ForwardClosure* forward = new ForwardClosure(OpType::Write, cntl_base,
                                                        request, response, done);
            writeSize << request->size();
            ForwardRequest(forward);
        } else {
            brpc::ClosureGuard guard(done);
            response->set_retcode(RetCode::kOK);
        }
    }

    void Read(google::protobuf::RpcController* cntl_base,
              const nebd::client::ReadRequest* request,
              nebd::client::ReadResponse* response,
              google::protobuf::Closure* done) override {
        // just forward request to chunkserver
        // ForwardClosure* forward = new ForwardClosure(OpType::Read, cntl_base,
        //                                              request, response, done);
        // ForwardRequest(forward);
        brpc::ClosureGuard guard(done);
        response->set_retcode(nebd::client::RetCode::kOK);
        readSize << request->size();
        static_cast<brpc::Controller*>(cntl_base)->response_attachment().resize(
            request->size(), 0);
    }

    void GetInfo(google::protobuf::RpcController* cntl_base,
                 const nebd::client::GetInfoRequest* request,
                 nebd::client::GetInfoResponse* response,
                 google::protobuf::Closure* done) override {
        brpc::ClosureGuard doneGuard(done);
        response->set_retcode(RetCode::kOK);

        nebd::client::FileInfo* info = new nebd::client::FileInfo();
        info->set_size(100ull * 1024 * 1024 * 1024ull);
        info->set_objsize(100 * 1024);
        info->set_objnums(1024 * 1024);
        response->set_allocated_info(info);
    }

    void Flush(google::protobuf::RpcController* cntl_base,
               const nebd::client::FlushRequest* request,
               nebd::client::FlushResponse* response,
               google::protobuf::Closure* done) {
        brpc::ClosureGuard guard(done);
        response->set_retcode(RetCode::kOK);
    }

    void CloseFile(google::protobuf::RpcController* cntl_base,
                   const nebd::client::CloseFileRequest* request,
                   nebd::client::CloseFileResponse* response,
                   google::protobuf::Closure* done) override {
        brpc::ClosureGuard guard(done);
        response->set_retcode(RetCode::kOK);
    }

    void Discard(google::protobuf::RpcController* cntl_base,
                 const nebd::client::DiscardRequest* request,
                 nebd::client::DiscardResponse* response,
                 google::protobuf::Closure* done) {
        brpc::ClosureGuard guard(done);
        response->set_retcode(RetCode::kOK);
    }

    void ResizeFile(google::protobuf::RpcController* cntl_base,
                    const nebd::client::ResizeRequest* request,
                    nebd::client::ResizeResponse* response,
                    google::protobuf::Closure* done) {
        brpc::ClosureGuard guard(done);
        response->set_retcode(RetCode::kOK);
    }

    void InvalidateCache(google::protobuf::RpcController* cntl_base,
                         const nebd::client::InvalidateCacheRequest* request,
                         nebd::client::InvalidateCacheResponse* response,
                         google::protobuf::Closure* done) override {
        brpc::ClosureGuard guard(done);
        response->set_retcode(RetCode::kOK);
    }
};

}  // namespace server
}  // namespace nebd

int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);

    init_channels();

    int ret = brpc::StartDummyServerAt(20000);
    if (ret != 0) {
        LOG(FATAL) << "Start Dummy Server failed";
    }

    // start server
    brpc::ServerOptions opts;
    brpc::Server server;

    ret = server.AddService(new nebd::server::NebdFileServiceImpl(),
                            brpc::SERVER_OWNS_SERVICE);
    if (ret != 0) {
        LOG(FATAL) << "Add service failed";
    }

    ret = server.AddService(new nebd::server::NebdHeartbeatServiceImpl(),
                            brpc::SERVER_OWNS_SERVICE);
    if (ret != 0) {
        LOG(FATAL) << "Add service failed";
    }

    ret = server.StartAtSockFile("/tmp/fake.nebd.sock", &opts);
    if (ret != 0) {
        LOG(FATAL) << "Start Server failed";
    }

    server.RunUntilAskedToQuit();

    return 0;
}
