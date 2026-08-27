#include "core/Application.h"
#include "core/Log.h"

int main() {
    twin::LogInfo("Lahore Urban Heat Digital Twin — Phase 0 (project foundation)");

    twin::Application app(1280, 720, "Lahore Urban Heat Digital Twin");
    if (!app.Init()) {
        twin::LogError("Application failed to initialize");
        return 1;
    }

    app.Run();
    return 0;
}
