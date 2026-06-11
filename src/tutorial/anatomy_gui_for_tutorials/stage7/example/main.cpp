/// Stage 7 demo — Double Buffer + Dirty Regions + poll-based Event Loop + AsyncFile
///
/// Creates a window with several buttons and labels.
/// Uses postTask to simulate periodic updates (button text changes).
/// Uses AsyncFile for async file reading demonstration.
/// Press Escape to quit.

#include "application.h"
#include "async_file.h"
#include "button.h"
#include "geometry.h"
#include "label.h"
#include "layout_strategy.h"
#include "container.h"
#include "signal.h"
#include "widget.h"

#include <cstdio>
#include <memory>
#include <string>

using namespace miniui;

// ── Layout aliases ──────────────────────────────────────────
using HBox = Container<HBoxLayout>;
using VBox = Container<VBoxLayout>;

int main() {
    auto& app = Application::instance();
    app.init(600, 400);

    // ── Build UI ──
    auto root = std::make_shared<VBox>();

    // Title
    auto title = std::make_shared<Label>();
    title->text() = "Stage 7 — Double Buffer + AsyncIO";
    title->setFontSize(20);
    title->setColor(Color::rgb(0.2, 0.2, 0.6));
    root->addChild(title);

    // Row of buttons
    auto row = std::make_shared<HBox>();
    auto btn1 = std::make_shared<Button>();
    btn1->text() = "Click Me";
    btn1->setFontSize(16);
    auto btn2 = std::make_shared<Button>();
    btn2->text() = "Async Read";
    btn2->setFontSize(16);
    auto btn3 = std::make_shared<Button>();
    btn3->text() = "Quit";
    btn3->setFontSize(16);
    row->addChild(btn1);
    row->addChild(btn2);
    row->addChild(btn3);
    root->addChild(row);

    // Status label
    auto status = std::make_shared<Label>();
    status->text() = "Ready. Click buttons or press Escape.";
    status->setFontSize(14);
    status->setColor(Color::rgb(0.3, 0.3, 0.3));
    root->addChild(status);

    app.setRoot(root);

    // ── Wire signals ──
    std::vector<ScopedConnection> conns;
    int clickCount = 0;
    conns.push_back(btn1->clicked().connect([&]() {
        clickCount++;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Clicked %d times!", clickCount);
        btn1->text() = buf;
    }));

    conns.push_back(btn2->clicked().connect([&]() {
        status->text() = "Reading /etc/hostname asynchronously...";
        AsyncFile::readAll("/etc/hostname",
            [&status](std::vector<uint8_t> data) {
                if (data.empty()) {
                    status->text() = "Async read failed or empty.";
                } else {
                    std::string content(data.begin(), data.end());
                    // Remove trailing newline
                    while (!content.empty() && content.back() == '\n') content.pop_back();
                    status->text() = "Read: " + content;
                }
            });
    }));

    conns.push_back(btn3->clicked().connect([&]() {
        app.quit();
    }));

    // ── Periodic update via postTask (simulates timer) ──
    int tick = 0;
    std::function<void()> timerTick;
    timerTick = [&app, &tick, &title, &timerTick]() {
        tick++;
        char buf[96];
        std::snprintf(buf, sizeof(buf), "Stage 7 — Double Buffer (tick %d)", tick);
        title->text() = buf;

        // Schedule next tick in ~500ms (approximated by 30 frames at 60fps)
        // Simple approach: just post again immediately, the poll timeout handles pacing
        if (tick < 100) {
            app.postTask(timerTick);
        }
    };
    // Kick off a delayed tick
    app.postTask(timerTick);

    std::printf("Stage 7 — Render Pipeline + AsyncIO demo\n");
    std::printf("Click buttons. Press Escape to quit.\n");

    return app.run();
}
