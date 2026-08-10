#include "health.grpc.pb.h"
#include "health.pb.h"

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: grpc_health_check <host:port>\n";
        return 2;
    }

    std::string addr(argv[1]);
    auto channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    auto stub = grpc::health::v1::Health::NewStub(channel);

    grpc::health::v1::HealthCheckRequest req;
    grpc::health::v1::HealthCheckResponse resp;
    grpc::ClientContext ctx;
    ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    auto status = stub->Check(&ctx, req, &resp);
    if (!status.ok()) {
        std::cerr << "grpc health check failed: " << status.error_message() << "\n";
        return 1;
    }
    if (resp.status() != grpc::health::v1::HealthCheckResponse::SERVING) {
        std::cerr << "grpc health status: " << resp.status() << "\n";
        return 1;
    }
    std::cout << "grpc health: serving\n";
    return 0;
}
