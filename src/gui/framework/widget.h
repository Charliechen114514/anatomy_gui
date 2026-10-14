/**
 * @file widget.h
 * @brief A node in the widget tree.
 */

#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "framework/event.h"
#include "framework/geometry.h"

/**
 * @brief A node in the widget tree.
 *
 * This layer answers pain point 2. A control used to cost three scattered
 * edits: an id constant, a CreateWindowExW call and a branch in the message
 * handler. Now it costs one node.
 *
 * Layer rule: nothing under framework/ includes windows.h. Anything that owns
 * a native handle lives in the platform layer and derives from this class.
 */
class Layout;

class Widget {
public:
    /**
     * @brief The signature a node uses to install its event handler.
     */
    using EventHandler = std::function<bool(Event&)>;

    Widget();
    virtual ~Widget();
    Widget(const Widget&)            = delete;
    Widget& operator=(const Widget&) = delete;

    /**
     * @brief Take ownership of a child node.
     *
     * @param[in] child The node to attach.
     * @return A reference to the attached child, for chaining.
     */
    Widget& add_child(std::unique_ptr<Widget> child);

    /**
     * @brief Read the node that owns this one.
     *
     * @return The parent, or nullptr for the root.
     */
    [[nodiscard]] Widget* parent() const { return parent_; }

    /**
     * @brief Read the children.
     *
     * @return The owned children, in insertion order.
     */
    [[nodiscard]] const std::vector<std::unique_ptr<Widget>>& children() const { return children_; }

    /**
     * @brief Read the node's identity.
     *
     * @return An id handed out at construction, unique inside the process.
     */
    [[nodiscard]] int id() const { return id_; }

    /**
     * @brief Find a node by id.
     *
     * @param[in] node_id The id to look for.
     * @return The matching node, or nullptr. Searches this subtree depth first.
     */
    [[nodiscard]] Widget* find_by_id(int node_id);

    /**
     * @brief Read the node's rectangle.
     *
     * @return The rectangle, in the parent's coordinate space.
     */
    [[nodiscard]] const Rect& bounds() const { return bounds_; }

    /**
     * @brief Move and resize the node.
     *
     * @param[in] bounds The new rectangle, in the parent's coordinate space.
     */
    void set_bounds(const Rect& bounds);

    /**
     * @brief Install the handler this node runs for events it accepts.
     *
     * @param[in] handler Called with every accepted event; returns true when
     * the handler dealt with it.
     */
    void set_handler(EventHandler handler);

    /**
     * @brief Answer whether this node wants the event.
     *
     * @param[in] event The event on its way through the tree.
     * @return true to let the event reach on_event.
     */
    [[nodiscard]] virtual bool accepts(const Event& event) const;

    /**
     * @brief Handle an accepted event.
     *
     * @param[in] event The event to handle.
     * @return true when this node handled it and bubbling stops.
     */
    virtual bool on_event(Event& event);

    /**
     * @brief Install the strategy that arranges this node's children.
     *
     * @param[in] layout The strategy, or nullptr to remove the current one.
     */
    void set_layout(std::unique_ptr<Layout> layout);

    /**
     * @brief Arrange the children.
     *
     * Runs on its own after every bounds change, so a container never has
     * to remember to lay out again.
     */
    void layout_children();

    /**
     * @brief Read the size this node asks for.
     *
     * @return The hint; a zero component means "whatever the layout decides".
     */
    [[nodiscard]] const Size& preferred_size() const { return preferred_size_; }

    /**
     * @brief Ask for a size.
     *
     * @param[in] size The hint; a zero component means "whatever the layout
     * decides".
     */
    void set_preferred_size(const Size& size) { preferred_size_ = size; }

    /**
     * @brief Ask for a repaint.
     *
     * Walks to the root and calls its repaint hook, so a node that owns a
     * property can refresh itself without knowing the platform exists.
     */
    void invalidate();

    /**
     * @brief Install the hook that turns a repaint request into a real redraw.
     *
     * Set on the root; the window decides what repainting means in its terms.
     *
     * @param[in] handler Called when some node below asks for a repaint.
     */
    void set_repaint_handler(std::function<void()> handler) { repaint_ = std::move(handler); }

protected:
    /**
     * @brief Runs after the bounds change.
     *
     * A subclass overrides this to move whatever native object it owns.
     */
    virtual void on_bounds_changed() {}

private:
    static int              next_id_;
    EventHandler            handler_;
    std::unique_ptr<Layout> layout_;
    Size                    preferred_size_{};
    std::function<void()>   repaint_;

    int                                  id_     = 0;
    Widget*                              parent_ = nullptr;
    std::vector<std::unique_ptr<Widget>> children_;
    Rect                                 bounds_{};
};
