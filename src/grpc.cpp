//
// Created by Zwagoth on 5/19/2018.
//

#include "logging.hpp"

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include "messages.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;

class StatusClient {
public:
    StatusClient(std::shared_ptr<Channel> channel) :
            stub_(StatusSystem::NewStub(channel)) {

    }

    void StatusRunner() {
        ClientContext context;

        std::shared_ptr<ClientReaderWriter<MessageIn, MessageOut> > stream(stub_->StatusSystem(&context));

        MessageOut message;
        while (stream->Read(&message)) {
            LOG(WARNING) << "Status Message Type: " << message.OutMessage_case();
        }
        Status status = stream->Finish();
        if(!status.ok()) {
            LOG(WARNING) << "Stream failed.";
        }
    }
private:
    std::unique_ptr<StatusSystem::Stub> stub_;
};

void startgrpc() {
    auto client = StatusClient(grpc::CreateChannel("unix:///tmp/statusd.sock", grpc::InsecureChannelCredentials()));
}