#include "standard_tools/api/grpc_server.hpp"

#include "health.grpc.pb.h"
#include "health.pb.h"

#include <memory>
#include <string>

namespace standard_tools::api {

namespace {

class HealthServiceImpl final : public grpc::health::v1::Health::Service {
public:
    grpc::Status Check(
        grpc::ServerContext* context,
        const grpc::health::v1::HealthCheckRequest* request,
        grpc::health::v1::HealthCheckResponse* response) override {
        (void)context;
        (void)request;
        response->set_status(grpc::health::v1::HealthCheckResponse::SERVING);
        return grpc::Status::OK;
    }

    grpc::Status Watch(
        grpc::ServerContext* context,
        const grpc::health::v1::HealthCheckRequest* request,
        grpc::ServerWriter<grpc::health::v1::HealthCheckResponse>* writer) override {
        (void)context;
        (void)request;
        grpc::health::v1::HealthCheckResponse response;
        response.set_status(grpc::health::v1::HealthCheckResponse::SERVING);
        writer->Write(response);
        return grpc::Status::OK;
    }
};

}  // namespace

class GrpcHealthServer::Impl {
public:
    std::unique_ptr<grpc::health::v1::Health::Service> service;
    std::unique_ptr<grpc::Server> server;
};

GrpcHealthServer::GrpcHealthServer(std::unique_ptr<grpc::health::v1::Health::Service> service,
                                   std::unique_ptr<grpc::Server> server)
    : impl_(std::make_unique<Impl>()) {
    impl_->service = std::move(service);
    impl_->server = std::move(server);
}

GrpcHealthServer::~GrpcHealthServer() = default;

void GrpcHealthServer::Wait() {
    impl_->server->Wait();
}

void GrpcHealthServer::Shutdown() {
    impl_->server->Shutdown();
}

std::unique_ptr<GrpcHealthServer> StartGrpcHealthServer(const std::string& addr) {
    auto service = std::make_unique<HealthServiceImpl>();

    grpc::ServerBuilder builder;
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(service.get());
    auto server = builder.BuildAndStart();
    if (!server) {
        return nullptr;
    }

    return std::make_unique<GrpcHealthServer>(std::move(service), std::move(server));
}

}  // namespace standard_tools::api
