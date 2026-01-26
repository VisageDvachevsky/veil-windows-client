#include <variant>
#include <iostream>
#include <string>

struct StatusResponse {
    std::string status = "default_status";
};

struct DiagnosticsResponse {
    std::string diagnostics = "default_diagnostics";
};

using Response = std::variant<StatusResponse, DiagnosticsResponse>;

void print_response(const Response& resp) {
    if (std::holds_alternative<StatusResponse>(resp)) {
        std::cout << "Holds StatusResponse: " << std::get<StatusResponse>(resp).status << std::endl;
    } else if (std::holds_alternative<DiagnosticsResponse>(resp)) {
        std::cout << "Holds DiagnosticsResponse: " << std::get<DiagnosticsResponse>(resp).diagnostics << std::endl;
    }
}

int main() {
    // Test 1: Default construction
    std::cout << "Test 1: Default construction" << std::endl;
    Response response1;
    print_response(response1);
    
    // Test 2: Assign DiagnosticsResponse
    std::cout << "\nTest 2: Assign DiagnosticsResponse" << std::endl;
    DiagnosticsResponse diag;
    diag.diagnostics = "test_diagnostics";
    response1 = diag;
    print_response(response1);
    
    // Test 3: Fresh Response
    std::cout << "\nTest 3: Fresh Response assigned in same scope" << std::endl;
    Response response2;
    [&response2]() {
        DiagnosticsResponse d;
        d.diagnostics = "lambda_diagnostics";
        response2 = d;
    }();
    print_response(response2);
    
    return 0;
}
