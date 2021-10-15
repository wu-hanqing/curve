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

void Task() {
    for (auto& ip : cs) {
        GetDentryRequest request;
        request.set_poolid(1234);
        request.set_copysetid(1234);
        request.set_partitionid(1234);
        request.set_fsid(1);
        request.set_parentinodeid(1);
        request.set_name("hello");
        request.set_txid(1);
        // request.set_appliedindex(1);

        GetDentryResponse response;

        brpc::Channel channel;
        if (channel.Init(ip.c_str(), nullptr) != 0) {
            std::cerr << "init channel to " << ip << " failed\n";
            continue;
        }

        brpc::Controller cntl;
        MetaServerService_Stub stub(&channel);
        stub.GetDentry(&cntl, &request, &response,  nullptr);

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

        std::cout << "Get Dentry reply: " << response.ShortDebugString()
                  << std::endl;

        return;
    }

}

int main(int argc, char* argv[]) {
    Task();

    return 0;
}
