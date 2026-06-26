#include "ArtForge/Services/ServiceResult.hpp"

#include <sstream>

namespace ArtForge::Services {

ServiceStatus MakeOkStatus(std::string summary)
{
    ServiceStatus status;
    status.ok = true;
    status.summary = std::move(summary);
    return status;
}

ServiceStatus MakeErrorStatus(std::string code, std::string message)
{
    ServiceStatus status;
    status.ok = false;
    status.summary = message;
    status.diagnostics.push_back({ServiceSeverity::Error, std::move(code), std::move(message)});
    return status;
}

std::string DescribeServiceStatus(const ServiceStatus& status)
{
    std::ostringstream output;
    output << "Service status: " << (status.ok ? "OK" : "failed") << "\n";
    if (!status.summary.empty()) {
        output << "Summary: " << status.summary << "\n";
    }
    for (const auto& diagnostic : status.diagnostics) {
        output << "Diagnostic: ";
        switch (diagnostic.severity) {
        case ServiceSeverity::Info:
            output << "info";
            break;
        case ServiceSeverity::Warning:
            output << "warning";
            break;
        case ServiceSeverity::Error:
            output << "error";
            break;
        }
        if (!diagnostic.code.empty()) {
            output << " " << diagnostic.code;
        }
        output << " " << diagnostic.message << "\n";
    }
    return output.str();
}

}
