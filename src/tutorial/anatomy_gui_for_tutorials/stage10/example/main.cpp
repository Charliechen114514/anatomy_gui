// Stage 10 Demo: File Browser with Async Thumbnails
//
// Demonstrates that the GUI remains responsive while thumbnails are loaded
// in the background using Application::runAsync + ThreadPool.
//
// Press Escape to quit.

#include "application.h"
#include "button.h"
#include "color_widget.h"
#include "container.h"
#include "geometry.h"
#include "label.h"
#include "root_window.h"

#include <atomic>
#include <cstdio>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// ThumbnailWidget — a colored rectangle with a label below it.
// When the thumbnail is "loaded", its color changes and the label updates.
// ---------------------------------------------------------------------------
class ThumbnailWidget : public miniui::Widget {
public:
    ThumbnailWidget(const std::string& name, int index)
        : name_(name), index_(index), loaded_(false) {
        color_ = miniui::Color::fromRGB(0.85, 0.85, 0.85); // gray placeholder
    }

    void setLoaded(const miniui::Color& c) {
        color_ = c;
        loaded_ = true;
    }

    bool isLoaded() const { return loaded_; }

    // --- Widget overrides ---
    miniui::Size measure(const miniui::Size& available) override {
        miniui::Size sz{120, 100};
        frame_.width  = sz.width;
        frame_.height = sz.height;
        return sz;
    }

    void paint(miniui::Painter& p) override {
        // Colored rectangle
        p.setSourceColor(color_);
        p.rectangle(frame_);
        p.fill();

        // Border
        if (loaded_) {
            p.setSourceRGB(0.2, 0.6, 0.2);
        } else {
            p.setSourceRGB(0.5, 0.5, 0.5);
        }
        p.setLineWidth(1.5);
        p.rectangle(frame_);
        p.stroke();

        // "Loading..." text or checkmark
        p.setSourceRGB(0.0, 0.0, 0.0);
        p.selectFontFace("sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_NORMAL);
        p.setFontSize(11.0);

        if (!loaded_) {
            // Spinning indicator
            auto ext = p.textExtents("Loading...");
            double tx = (frame_.width - ext.width) / 2.0;
            double ty = (frame_.height + ext.height) / 2.0;
            p.moveTo(tx, ty);
            p.showText("Loading...");
        } else {
            auto ext = p.textExtents("Ready");
            double tx = (frame_.width - ext.width) / 2.0;
            double ty = (frame_.height + ext.height) / 2.0;
            p.moveTo(tx, ty);
            p.showText("Ready");
        }

        // Name label below the rectangle
        p.setFontSize(10.0);
        auto nameExt = p.textExtents(name_.c_str());
        double nx = (frame_.width - nameExt.width) / 2.0;
        p.moveTo(nx, frame_.height - 4);
        p.showText(name_.c_str());
    }

private:
    std::string name_;
    int index_;
    bool loaded_;
    miniui::Color color_;
};

// ---------------------------------------------------------------------------
// Thumbnail data (simulated result from a background worker)
// ---------------------------------------------------------------------------
struct ThumbnailResult {
    int index;
    miniui::Color color;
    std::string label;
};

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    miniui::Application app;
    app.createWindow("Stage 10 — Async Thumbnail Browser", 800, 600);

    // --- Build the UI tree ---

    auto root = std::make_shared<miniui::RootWindow>();

    // Main vertical layout: title | grid area | status bar
    auto mainVBox = std::make_shared<miniui::VBoxContainer>(miniui::VBoxLayout(8.0));

    // Title label
    auto titleLabel = std::make_shared<miniui::Label>(
        "Async Thumbnail Browser — Stage 10 Demo");
    titleLabel->setFontSize(18.0);
    titleLabel->setColor(miniui::Color::fromRGB(0.15, 0.15, 0.5));
    mainVBox->addChild(titleLabel);

    // Grid area: use an HBox of VBox columns for a 4-column grid
    constexpr int COLS = 4;
    constexpr int ROWS = 2;
    constexpr int NUM_THUMBNAILS = COLS * ROWS;

    auto gridHBox = std::make_shared<miniui::HBoxContainer>(miniui::HBoxLayout(8.0));

    std::vector<std::shared_ptr<ThumbnailWidget>> thumbnails;

    const char* fileNames[] = {
        "cat.jpg",    "dog.png",     "tree.bmp",    "sky.tiff",
        "mountain.jpg","river.png",  "sunset.bmp",  "ocean.tiff"
    };

    for (int col = 0; col < COLS; ++col) {
        auto colVBox = std::make_shared<miniui::VBoxContainer>(miniui::VBoxLayout(8.0));
        for (int row = 0; row < ROWS; ++row) {
            int idx = col * ROWS + row;
            auto thumb = std::make_shared<ThumbnailWidget>(
                fileNames[idx], idx);
            thumbnails.push_back(thumb);
            colVBox->addChild(thumb);
        }
        gridHBox->addChild(colVBox);
    }
    mainVBox->addChild(gridHBox);

    // Status bar: horizontal layout with status label + refresh button
    auto statusBar = std::make_shared<miniui::HBoxContainer>(miniui::HBoxLayout(12.0));

    auto statusLabel = std::make_shared<miniui::Label>("Status: Idle");
    statusLabel->setFontSize(13.0);
    statusLabel->setColor(miniui::Color::fromRGB(0.3, 0.3, 0.3));
    statusBar->addChild(statusLabel);

    auto refreshBtn = std::make_shared<miniui::Button>("Refresh");
    statusBar->addChild(refreshBtn);
    mainVBox->addChild(statusBar);

    root->setContent(mainVBox);
    app.setRoot(root);

    // --- Loading state ---
    auto loadedCount = std::make_shared<std::atomic<int>>(0);
    auto totalToLoad = std::make_shared<int>(NUM_THUMBNAILS);

    // --- Function to kick off async thumbnail loading ---
    auto startLoading = [&app, thumbnails, statusLabel, loadedCount, totalToLoad, root]() {
        // Reset state
        loadedCount->store(0);

        // Generate random colors for thumbnails
        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<double> dist(0.2, 0.9);

        for (int i = 0; i < static_cast<int>(thumbnails.size()); ++i) {
            // Each thumbnail gets its own async load
            auto thumb = thumbnails[i];

            // Simulated "work": generate a random color + sleep
            double r = dist(rng);
            double g = dist(rng);
            double b = dist(rng);

            // The delay range: 500ms to 2500ms to show concurrency
            int delayMs = 500 + (i * 317) % 2000;

            app.runAsync(
                // --- Background work ---
                [i, r, g, b, delayMs]() -> ThumbnailResult {
                    // Simulate image processing
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(delayMs));

                    return ThumbnailResult{
                        i,
                        miniui::Color::fromRGB(r, g, b),
                        "Loaded"
                    };
                },
                // --- GUI-thread callback ---
                [thumb, statusLabel, loadedCount, totalToLoad, root]
                (ThumbnailResult result) {
                    // Update the thumbnail
                    thumb->setLoaded(result.color);
                    thumb->repaint();

                    // Update status bar
                    int done = loadedCount->fetch_add(1) + 1;
                    int total = *totalToLoad;

                    char buf[128];
                    if (done < total) {
                        std::snprintf(buf, sizeof(buf),
                            "Status: Loading thumbnails... %d/%d complete",
                            done, total);
                    } else {
                        std::snprintf(buf, sizeof(buf),
                            "Status: All %d thumbnails loaded!",
                            total);
                    }
                    statusLabel->text() = std::string(buf);

                    // Force a repaint of the entire window
                    root->repaint();
                }
            );
        }

        // Update status immediately
        statusLabel->text() = "Status: Loading thumbnails... 0/" +
            std::to_string(NUM_THUMBNAILS);
        root->repaint();
    };

    // --- Connect the Refresh button ---
    refreshBtn->clicked().connect([startLoading]() {
        std::fprintf(stderr, "[DBG] Refresh clicked callback fired!\n");
        startLoading();
    });

    // --- Start loading on first paint ---
    // We use a one-shot flag to trigger loading after the window is shown.
    // A simple approach: post a task to the GUI thread queue.
    app.postTask([startLoading]() {
        startLoading();
    });

    // --- Run the event loop ---
    return app.run();
}
