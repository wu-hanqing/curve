#include "curvefs/proto/metaserver.pb.h"

#include <string>
#include <vector>
#include <brpc/channel.h>
#include <brpc/controller.h>

std::vector<std::string> cs = {
    "10.182.2.33:16701",
    "10.182.2.33:16702",
    "10.182.2.33:16703"
};

using namespace curvefs::metaserver;

void Create() {
    for (auto& ip : cs) {
        CreateDentryRequest request;
        request.set_poolid(1234);
        request.set_copysetid(1234);
        request.set_partitionid(1234);

        Dentry dentry;
        dentry.set_fsid(1);
        dentry.set_parentinodeid(1);
        dentry.set_inodeid(2);
        dentry.set_name("hello");
        dentry.set_txid(1);

        *request.mutable_dentry() = std::move(dentry);

        CreateDentryResponse response;

        brpc::Channel channel;
        if (channel.Init(ip.c_str(), nullptr) != 0) {
            std::cerr << "init channel to " << ip << " failed\n";
            continue;
        }

        brpc::Controller cntl;
        MetaServerService_Stub stub(&channel);
        stub.CreateDentry(&cntl, &request, &response,  nullptr);

        if (cntl.Failed()) {
            std::cerr << "rpc failed, error: " << cntl.ErrorText() << "\n";
            continue;
        }

        if (response.statuscode() == MetaStatusCode::REDIRECTED) {
            std::cerr << ip << " is not leader\n";
            continue;
        }

        if (response.statuscode() != MetaStatusCode::OK) {
            std::cerr << "request failed, " << response.ShortDebugString()
                      << "\n";
            continue;
        }

        std::cout << "Create Inode reply: " << response.DebugString()
                  << std::endl;

        return;
    }

}

int main(int argc, char* argv[]) {
    Create();

    return 0;
}
