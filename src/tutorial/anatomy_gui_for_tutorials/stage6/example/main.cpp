/// @file main.cpp
/// @brief Mini Calculator — Stage 6 demo showcasing Signal-Slot + Property
///        with an MVP (Model-View-Presenter) architecture.
///
/// Layout:
///   +---------------------------+
///   |           42              |  <- Label (display)
///   +---------------------------+
///   | [7] [8] [9] [+]          |
///   | [4] [5] [6] [-]          |
///   | [1] [2] [3] [*]          |
///   | [C] [0] [=] [/]          |
///   +---------------------------+

#include "button.h"
#include "color_widget.h"
#include "container.h"
#include "geometry.h"
#include "label.h"
#include "layout_strategy.h"
#include "painter.h"
#include "property.h"
#include "root_window.h"
#include "signal.h"
#include "widget.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// ===========================================================================
//  Model — pure arithmetic logic, no GUI dependency
// ===========================================================================

class CalculatorModel {
public:
    std::string evaluate(const std::string& expression) {
        if (expression.empty()) return "0";

        try {
            auto tokens = tokenize(expression);
            if (tokens.empty()) return "0";

            // First pass: handle * and /
            std::vector<std::string> pass1;
            int i = 0;
            while (i < static_cast<int>(tokens.size())) {
                if (i + 2 < static_cast<int>(tokens.size()) &&
                    (tokens[i + 1] == "*" || tokens[i + 1] == "/")) {
                    double left  = toNumber(tokens[i]);
                    double right = toNumber(tokens[i + 2]);
                    double result = 0;
                    if (tokens[i + 1] == "*") {
                        result = left * right;
                    } else {
                        if (right == 0.0) return "Error";
                        result = left / right;
                    }
                    tokens[i + 2] = formatNumber(result);
                    i += 2;
                } else {
                    pass1.push_back(tokens[i]);
                    i++;
                }
            }

            // Second pass: handle + and -
            if (pass1.empty()) return "0";
            double acc = toNumber(pass1[0]);
            i = 1;
            while (i + 1 < static_cast<int>(pass1.size())) {
                double right = toNumber(pass1[i + 1]);
                if (pass1[i] == "+")      acc += right;
                else if (pass1[i] == "-") acc -= right;
                i += 2;
            }

            return formatNumber(acc);
        } catch (...) {
            return "Error";
        }
    }

private:
    static std::vector<std::string> tokenize(const std::string& expr) {
        std::vector<std::string> tokens;
        std::string current;

        for (char c : expr) {
            if (c == '+' || c == '-' || c == '*' || c == '/') {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
                // Handle unary minus
                if (c == '-' &&
                    (tokens.empty() ||
                     (tokens.back() == "+" || tokens.back() == "-" ||
                      tokens.back() == "*" || tokens.back() == "/"))) {
                    current += c;
                } else {
                    tokens.push_back(std::string(1, c));
                }
            } else if (c == '.' || (c >= '0' && c <= '9')) {
                current += c;
            }
        }
        if (!current.empty()) {
            tokens.push_back(current);
        }
        return tokens;
    }

    static double toNumber(const std::string& s) {
        return std::stod(s);
    }

    static std::string formatNumber(double v) {
        if (std::floor(v) == v && std::abs(v) < 1e15) {
            return std::to_string(static_cast<long long>(v));
        }
        std::ostringstream oss;
        oss << v;
        return oss.str();
    }
};

// ===========================================================================
//  View — creates widgets, exposes display text and button signals
// ===========================================================================

class CalculatorView {
public:
    CalculatorView() {
        display_ = std::make_shared<miniui::Label>();
        display_->setFontSize(28);
        display_->setColor(miniui::Color::rgb(0.1, 0.1, 0.1));
        display_->text() = "0";

        const char* layout[][4] = {
            {"7", "8", "9", "+"},
            {"4", "5", "6", "-"},
            {"1", "2", "3", "*"},
            {"C", "0", "=", "/"},
        };

        for (int r = 0; r < 4; ++r) {
            auto row = std::make_shared<miniui::Container<miniui::HBoxLayout>>();
            for (int c = 0; c < 4; ++c) {
                auto btn = std::make_shared<miniui::Button>();
                btn->text() = layout[r][c];
                btn->setFontSize(20);
                buttons_.push_back(btn);
                row->addChild(btn);
            }
            rows_.push_back(row);
        }
    }

    miniui::Property<std::string>& displayText() { return display_->text(); }

    std::shared_ptr<miniui::Button> findButton(const std::string& label) const {
        for (auto& btn : buttons_) {
            if (btn->text().get() == label) return btn;
        }
        return nullptr;
    }

    void attachTo(miniui::RootWindow& root) {
        auto vbox = std::make_shared<miniui::Container<miniui::VBoxLayout>>();

        // Display area with a light background
        auto displayBg = std::make_shared<miniui::ColorWidget>(
            miniui::Color::rgb(0.95, 0.95, 0.95));
        displayBg->addChild(display_);
        vbox->addChild(displayBg);

        for (auto& row : rows_) {
            vbox->addChild(row);
        }

        root.addChild(vbox);
    }

private:
    std::shared_ptr<miniui::Label> display_;
    std::vector<std::shared_ptr<miniui::Button>> buttons_;
    std::vector<std::shared_ptr<miniui::Container<miniui::HBoxLayout>>> rows_;
};

// ===========================================================================
//  Presenter — wires button signals to model logic and updates the view
// ===========================================================================

class CalculatorPresenter {
public:
    CalculatorPresenter(CalculatorModel& model, CalculatorView& view)
        : model_(model), view_(view) {
        connectButtons();
    }

private:
    void connectButtons() {
        for (char d = '0'; d <= '9'; ++d) {
            std::string label(1, d);
            auto btn = view_.findButton(label);
            if (btn) {
                connections_.push_back(btn->clicked().connect([this, label]() {
                    onDigit(label);
                }));
            }
        }

        for (auto op : {"+", "-", "*", "/"}) {
            auto btn = view_.findButton(op);
            if (btn) {
                connections_.push_back(btn->clicked().connect([this, op]() {
                    onOperator(op);
                }));
            }
        }

        auto eq = view_.findButton("=");
        if (eq) {
            connections_.push_back(eq->clicked().connect([this]() {
                onEquals();
            }));
        }

        auto clear = view_.findButton("C");
        if (clear) {
            connections_.push_back(clear->clicked().connect([this]() {
                onClear();
            }));
        }
    }

    void onDigit(const std::string& digit) {
        if (justEvaluated_) {
            expression_.clear();
            justEvaluated_ = false;
        }
        if (expression_ == "0" && digit != "0") {
            expression_.clear();
        } else if (expression_ == "0" && digit == "0") {
            return;
        }
        expression_ += digit;
        view_.displayText() = expression_;
    }

    void onOperator(const std::string& op) {
        if (expression_.empty()) return;
        justEvaluated_ = false;

        if (!expression_.empty()) {
            char last = expression_.back();
            if (last == '+' || last == '-' || last == '*' || last == '/') {
                expression_.pop_back();
            }
        }
        expression_ += op;
        view_.displayText() = expression_;
    }

    void onEquals() {
        if (expression_.empty()) return;
        auto result = model_.evaluate(expression_);
        view_.displayText() = result;
        expression_ = result;
        justEvaluated_ = true;
    }

    void onClear() {
        expression_.clear();
        justEvaluated_ = false;
        view_.displayText() = "0";
    }

    CalculatorModel& model_;
    CalculatorView&  view_;
    std::string      expression_;
    bool             justEvaluated_ = false;
    std::vector<miniui::ScopedConnection> connections_;
};

// ===========================================================================
//  main
// ===========================================================================

int main() {
    constexpr int WIN_W = 320;
    constexpr int WIN_H = 420;

    miniui::RootWindow root(WIN_W, WIN_H, "MiniUI Calculator");

    CalculatorModel model;
    CalculatorView  view;
    CalculatorPresenter presenter(model, view);

    view.attachTo(root);
    root.layout();
    root.repaint();

    root.run();
    return 0;
}
