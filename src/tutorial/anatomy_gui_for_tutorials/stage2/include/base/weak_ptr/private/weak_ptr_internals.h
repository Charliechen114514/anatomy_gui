/**
 * @file    base/include/base/weak_ptr/private/weak_ptr_internals.h
 * @brief   Internal implementation details for WeakPtr system.
 *
 * This header contains the internal implementation details for the WeakPtr
 * system. External code should not directly include or depend on this file.
 * WeakPtr and WeakPtrFactory share the WeakReferenceFlag to track liveness.
 *
 * @author  N/A
 * @date    N/A
 * @version N/A
 * @since   N/A
 * @ingroup none
 */

#pragma once

#include <atomic>
#include <memory>

namespace anatomy_gui::base {
namespace internal {

/**
 * @brief  Shared flag tracking WeakPtr liveness state.
 *
 * The Owner (WeakPtrFactory) and all WeakPtr instances share the same
 * WeakReferenceFlag instance. When the owner is destroyed (or explicitly
 * calls InvalidateWeakPtrs), the alive_ flag is set to false, causing all
 * WeakPtr::Get() calls to return nullptr.
 *
 * @ingroup none
 *
 * @note   The alive_ flag uses std::atomic<bool> for thread-safety.
 *         Invalidate() and IsAlive() are thread-safe operations.
 *
 * @warning WeakPtr should only be used on the same thread where it was
 *          created. The "check + dereference" two-step operation between
 *          IsAlive() and actual access is not atomic.
 *
 * @code
 * // Internal use only - created by WeakPtrFactory
 * auto flag = std::make_shared<WeakReferenceFlag>();
 * if (flag->IsAlive()) {
 *     // Safe to access
 * }
 * flag->Invalidate();  // All WeakPtrs now return nullptr
 * @endcode
 */
class WeakReferenceFlag {
  public:
    /// @brief Default constructor: creates a flag in the alive state.
    WeakReferenceFlag() = default;

    /// @brief Copy constructor: deleted. Flag is shared via shared_ptr.
    WeakReferenceFlag(const WeakReferenceFlag&) = delete;

    /// @brief Copy assignment: deleted.
    WeakReferenceFlag& operator=(const WeakReferenceFlag&) = delete;

    /// @brief Move constructor: deleted.
    WeakReferenceFlag(WeakReferenceFlag&&) = delete;

    /// @brief Move assignment: deleted.
    WeakReferenceFlag& operator=(WeakReferenceFlag&&) = delete;

    /**
     * @brief  Checks if the owner is still alive.
     *
     * @return true if the owner is alive, false if invalidated.
     * @throws None
     * @note   Uses memory_order_acquire for proper synchronization.
     * @warning None
     * @since  N/A
     * @ingroup none
     */
    [[nodiscard]] bool IsAlive() const noexcept {
        return alive_.load(std::memory_order_acquire);
    }

    /**
     * @brief  Marks the flag as invalid.
     *
     * After calling this method, all existing and future IsAlive() calls
     * returns false.
     *
     * @throws None
     * @note   Uses memory_order_release for proper synchronization.
     * @warning None
     * @since  N/A
     * @ingroup none
     */
    void Invalidate() noexcept {
        alive_.store(false, std::memory_order_release);
    }

  private:
    std::atomic<bool> alive_{true};
};

/// @brief Shared pointer type for WeakReferenceFlag.
using WeakReferenceFlagPtr = std::shared_ptr<WeakReferenceFlag>;

} // namespace internal
} // namespace anatomy_gui::base
