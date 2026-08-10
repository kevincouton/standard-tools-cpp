#pragma once

#include "health.grpc.pb.h"

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include <memory>
#include <string>

namespace standard_tools::api {

// Owns the gRPC health service implementation and the server.
class GrpcHealthServer {
public:
    GrpcHealthServer(std::unique_ptr<grpc::health::v1::Health::Service> service,
                     std::unique_ptr<grpc::Server> server);
    ~GrpcHealthServer();

    GrpcHealthServer(const GrpcHealthServer&) = delete;
    GrpcHealthServer& operator=(const GrpcHealthServer&) = delete;

    void Wait();
    void Shutdown();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

std::unique_ptr<GrpcHealthServer> StartGrpcHealthServer(const std::string& addr);

}  // namespace standard_tools::api
