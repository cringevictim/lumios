#include "editor_app.h"

int main() {
    lumios::editor::EditorApp app;
    if (!app.init()) return 1;
    app.run();
    app.shutdown();
    return 0;
}
