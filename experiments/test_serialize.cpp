#include <variant>
#include <iostream>
#include "../src/common/ipc/ipc_protocol.h"

int main() {
    veil::ipc::DiagnosticsResponse diag_resp;
    diag_resp.diagnostics.protocol.packets_sent = 100;
    diag_resp.diagnostics.protocol.packets_received = 200;
    
    veil::ipc::Response response = diag_resp;
    
    veil::ipc::Message msg;
    msg.type = veil::ipc::MessageType::kResponse;
    msg.id = 1;
    msg.payload = response;
    
    std::string serialized = veil::ipc::serialize_message(msg);
    std::cout << "Serialized: " << serialized << std::endl;
    
    return 0;
}
