#include <iostream>
#include <atomic>
#include <memory>
#include <brpc/controller.h>
#include <brpc/channel.h>
#include <brpc/server.h>
#include "nebd/proto/client.pb.h"
#include "src/common/concurrent/count_down_event.h"

DEFINE_bool(tcp, true, "use tcp or unix");
DEFINE_int32(port, 12345, "tcp listen port");
DEFINE_string(unix_sock, "/tmp/unix.pingpong.sock", "unix socket");

class TestImpl : public nebd::client::NebdFileService {
 public:
    TestImpl() = default;
    ~TestImpl() = default;

    virtual void OpenFile(google::protobuf::RpcController* cntl_base,
                          const nebd::client::OpenFileRequest* request,
                          nebd::client::OpenFileResponse* response,
                          google::protobuf::Closure* done) {}

    virtual void Write(google::protobuf::RpcController* cntl_base,
                       const nebd::client::WriteRequest* request,
                       nebd::client::WriteResponse* response,
                       google::protobuf::Closure* done) {
                           response->set_retcode(nebd::client::RetCode::kOK);
                           done->Run();
                       }

    virtual void Read(google::protobuf::RpcController* cntl_base,
                      const nebd::client::ReadRequest* request,
                      nebd::client::ReadResponse* response,
                      google::protobuf::Closure* done) {}

    virtual void GetInfo(google::protobuf::RpcController* cntl_base,
                         const nebd::client::GetInfoRequest* request,
                         nebd::client::GetInfoResponse* response,
                         google::protobuf::Closure* done){}

    virtual void Flush(google::protobuf::RpcController* cntl_base,
                       const nebd::client::FlushRequest* request,
                       nebd::client::FlushResponse* response,
                       google::protobuf::Closure* done){}

    virtual void CloseFile(google::protobuf::RpcController* cntl_base,
                           const nebd::client::CloseFileRequest* request,
                           nebd::client::CloseFileResponse* response,
                           google::protobuf::Closure* done) {}

    virtual void Discard(google::protobuf::RpcController* cntl_base,
                         const nebd::client::DiscardRequest* request,
                         nebd::client::DiscardResponse* response,
                         google::protobuf::Closure* done) {}

    virtual void ResizeFile(google::protobuf::RpcController* cntl_base,
                            const nebd::client::ResizeRequest* request,
                            nebd::client::ResizeResponse* response,
                            google::protobuf::Closure* done) {}

    virtual void InvalidateCache(google::protobuf::RpcController* cntl_base,
                            const nebd::client::InvalidateCacheRequest* request,
                            nebd::client::InvalidateCacheResponse* response,
                            google::protobuf::Closure* done) {}
};

int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);

    brpc::Server server;
    TestImpl testImpl;

    if (server.AddService(&testImpl, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        std::cout << "Fail to add service";
        return -1;
    }

    if (FLAGS_tcp) {
        if (server.Start(FLAGS_port, nullptr) != 0) {
            std::cout << "Fail to start server";
            return -1;
        }
    } else {
        if (server.StartAtSockFile(FLAGS_unix_sock.c_str(), nullptr) != 0) {
            std::cout << "Fail to start server";
            return -1;
        }

    }

    server.RunUntilAskedToQuit();
    return 0;
}