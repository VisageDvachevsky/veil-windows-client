#include <variant>
#include <iostream>

struct DiagnosticsData {
    int value = 0;
};

struct DiagnosticsResponse {
    DiagnosticsData diagnostics;
};

struct SuccessResponse {
    std::string message;
};

using Response = std::variant<DiagnosticsResponse, SuccessResponse>;

int main() {
    Response response;
    std::cout << "Default variant index: " << response.index() << std::endl;
    std::cout << "Holds DiagnosticsResponse: " << std::holds_alternative<DiagnosticsResponse>(response) << std::endl;
    std::cout << "Holds SuccessResponse: " << std::holds_alternative<SuccessResponse>(response) << std::endl;
    
    DiagnosticsResponse diag_resp;
    diag_resp.diagnostics.value = 42;
    response = diag_resp;
    
    std::cout << "\nAfter assignment:" << std::endl;
    std::cout << "Variant index: " << response.index() << std::endl;
    std::cout << "Holds DiagnosticsResponse: " << std::holds_alternative<DiagnosticsResponse>(response) << std::endl;
    std::cout << "Value: " << std::get<DiagnosticsResponse>(response).diagnostics.value << std::endl;
    
    return 0;
}
