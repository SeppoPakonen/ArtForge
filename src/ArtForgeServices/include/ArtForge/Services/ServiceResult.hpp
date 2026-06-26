#pragma once

#include <string>
#include <vector>

namespace ArtForge::Services {

enum class ServiceSeverity {
    Info,
    Warning,
    Error,
};

struct ServiceDiagnostic {
    ServiceSeverity severity{ServiceSeverity::Info};
    std::string code;
    std::string message;
};

struct ServiceStatus {
    bool ok{true};
    std::string summary;
    std::vector<ServiceDiagnostic> diagnostics;
};

struct ServiceCommandResult {
    ServiceStatus status;
    std::string commandName;
    std::string outputPath;
    std::string debugSummary;
};

ServiceStatus MakeOkStatus(std::string summary);
ServiceStatus MakeErrorStatus(std::string code, std::string message);

std::string DescribeServiceStatus(const ServiceStatus& status);

}
