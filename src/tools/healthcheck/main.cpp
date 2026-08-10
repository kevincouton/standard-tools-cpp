#include <cstdlib>
#include <cstring>
#include <curl/curl.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "health.grpc.pb.h"
#include "health.pb.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

size_t WriteCallback(void*, size_t size, size_t nmemb, void*) {
    return size * nmemb;
}

bool ProbeHttp(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    CURLcode res = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    return res == CURLE_OK && code >= 200 && code < 300;
}

bool ProbeGrpc(const std::string& addr) {
    auto channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    auto stub = grpc::health::v1::Health::NewStub(channel);
    grpc::health::v1::HealthCheckRequest req;
    grpc::health::v1::HealthCheckResponse resp;
    grpc::ClientContext ctx;
    ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    auto status = stub->Check(&ctx, req, &resp);
    return status.ok() && resp.status() == grpc::health::v1::HealthCheckResponse::SERVING;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: healthcheck <http-url>|<host:port> [...]\n";
        return 2;
    }

    int failed = 0;
    for (int i = 1; i < argc; ++i) {
        std::string target(argv[i]);
        bool ok = false;
        if (target.rfind("http://", 0) == 0 || target.rfind("https://", 0) == 0) {
            ok = ProbeHttp(target);
        } else {
            ok = ProbeGrpc(target);
        }
        if (!ok) {
            std::cerr << "healthcheck failed for " << target << "\n";
            ++failed;
        }
    }

    if (failed > 0) {
        return 1;
    }
    std::cout << "healthcheck ok\n";
    return 0;
}
