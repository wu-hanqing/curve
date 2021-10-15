#include <brpc/channel.h>
#include <gflags/gflags.h>

#include <iostream>
#include <string>
#include <vector>

#include "curvefs/proto/copyset.pb.h"

DEFINE_int32(startid, 0, "");
DEFINE_int32(count, 100, "");

using namespace curvefs::metaserver::copyset;

std::vector<std::string> cs = {"10.182.2.33:26701", "10.182.2.33:26702",
                               "10.182.2.33:26703"};

uint32_t poolId = 1234;
uint32_t copysetId = 1234;
std::string peer1 = "10.182.2.33:26701:0";
std::string peer2 = "10.182.2.33:26702:0";
std::string peer3 = "10.182.2.33:26703:0";

void Create() {
    // create copyset
    CreateCopysetRequest request;
    for (int i = FLAGS_startid; i < FLAGS_startid + FLAGS_count; ++i) {
        auto* copyset = request.add_copysets();
        copyset->set_poolid(i);
        copyset->set_copysetid(i);
        copyset->add_peers()->set_address(peer1);
        copyset->add_peers()->set_address(peer2);
        copyset->add_peers()->set_address(peer3);
    }

    for (auto& ip : cs) {
        // CreateCopysetRequest request;
        // auto* copyset = request.add_copysets();

        // copyset->set_poolid(poolId);
        // copyset->set_copysetid(poolId);
        // copyset->add_peers()->set_address(peer1);
        // copyset->add_peers()->set_address(peer2);
        // copyset->add_peers()->set_address(peer3);

        CreateCopysetResponse response;
        brpc::Channel channel;
        if (channel.Init(ip.c_str(), nullptr) != 0) {
            std::cerr << "init channel to " << ip << " failed\n";
            return;
        }

        brpc::Controller cntl;
        cntl.set_timeout_ms(-1);
        CopysetService_Stub stub(&channel);
        stub.CreateCopysetNode(&cntl, &request, &response, nullptr);

        if (cntl.Failed()) {
            std::cerr << "rpc failed, error: " << cntl.ErrorText() << "\n";
            return;
        }

        if (response.status() != COPYSET_OP_STATUS_SUCCESS) {
            std::cerr << "response failed, error: "
                      << response.ShortDebugString() << "\n";
            return;
        }
    }

    std::cout << "success\n";
}

int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);

    Create();

    return 0;
}
