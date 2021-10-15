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

void CreatePartition() {
    for (auto& ip : cs) {
        CreatePartitionRequest request;
        auto* partition = request.mutable_partition();
        partition->set_fsid(1);
        partition->set_poolid(1234);
        partition->set_copysetid(1234);
        partition->set_partitionid(1234);
        partition->set_start(1);
        partition->set_end(UINT_MAX);
        partition->set_txid(1);

        CreatePartitionResponse response;

        brpc::Channel channel;
        if (channel.Init(ip.c_str(), nullptr) != 0) {
            std::cerr << "init channel to " << ip << " failed\n";
            continue;
        }

        brpc::Controller cntl;
        MetaServerService_Stub stub(&channel);
        stub.CreatePartition(&cntl, &request, &response,  nullptr);

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

        return;
    }

}

int main(int argc, char* argv[]) {
    CreatePartition();

    return 0;
}
