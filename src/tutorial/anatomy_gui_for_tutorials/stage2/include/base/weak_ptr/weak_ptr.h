/**
 * @file    base/include/base/weak_ptr/weak_ptr.h
 * @brief   Non-owning weak reference for exclusively owned resources.
 *
 * Unlike std::weak_ptr, WeakPtr does not participate in reference counting.
 * The resource is exclusively owned by an owner (typically holding
 * WeakPtrFactory), and WeakPtr acts as a "claim ticket" that becomes
 * invalid when the owner disappears.
 *
 * @author  N/A
 * @date    N/A
 * @version N/A
 * @since   N/A
 * @ingroup none
 */

#pragma once

#include "private/weak_ptr_internals.h"

#include <cassert>
#include <type_traits>

namespace anatomy_gui::base {

// Forward declaration
template <typename T> class WeakPtrFactory;

/**
 * @brief  Non-owning weak reference for exclusively owned resources.
 *
 * @details Unlike std::weak_ptr, WeakPtr does not participate in reference
 *          counting. The resource is exclusively owned by an owner (typically
 *          holding WeakPtrFactory), and WeakPtr acts as a "claim ticket" that
 *          becomes invalid when the owner disappears.
 *
 * @tparam T Type of the referenced object.
 *
 * @ingroup none
 *
 * @note   Supports covariance: WeakPtr<Derived> can be implicitly converted
 *         to WeakPtr<Base>.
 *
 * @warning WeakPtr should only be used on the same thread (sequence) where it
 *          was created. The "check + dereference" two-step operation is not
 *          atomic.
 *
 * @code
 * class ThemeManager {
 * public:
 *     ...
 * private:
 *     WeakPtrFactory<ThemeManager> weak_factory_{this}; // Declare last
 * };
 *
 * WeakPtr<ThemeManager> ref = tm.GetWeakPtr();
 * if (ref) {               // or ref.IsValid()
 *     ref->ApplyTheme();   // Safe access
 * }
 * @endcode
 */
template <typename T> class WeakPtr {
  public:
    // ------------------------------------------------------------
    // Construction
    // ------------------------------------------------------------

    /**
     * @brief  Default constructor: creates an empty weak reference.
     * @throws None
     * @note   None
     * @warning None
     * @since  N/A
     * @ingroup none
     */
    constexpr WeakPtr() noexcept = default;

    /**
     * @brief  Constructs an empty weak reference from nullptr.
     * @param[in] nullptr_t The nullptr literal.
     * @throws None
     * @note   None
     * @warning None
     * @since  N/A
     * @ingroup none
     */
    constexpr WeakPtr(std::nullptr_t) noexcept {}

    /**
     * @brief  Copy constructor.
     * @throws None
     * @note   None
     * @warning None
     * @since  N/A
     * @ingroup none
     */
    WeakPtr(const WeakPtr&) noexcept = default;

    /**
     * @brief  Copy assignment operator.
     * @param[in] other Source weak pointer.
     * @return        Reference to this WeakPtr.
     * @throws        None
     * @note          None
     * @warning       None
     * @since         N/A
     * @ingroup       none
     */
    WeakPtr& operator=(const WeakPtr&) noexcept = default;

    /**
     * @brief  Move constructor.
     * @throws None
     * @note   None
     * @warning None
     * @since  N/A
     * @ingroup none
     */
    WeakPtr(WeakPtr&&) noexcept = default;

    /**
     * @brief  Move assignment operator.
     * @param[in] other Source weak pointer.
     * @return        Reference to this WeakPtr.
     * @throws        None
     * @note          None
     * @warning       None
     * @since         N/A
     * @ingroup       none
     */
    WeakPtr& operator=(WeakPtr&&) noexcept = default;

    /**
     * @brief  Covariant copy constructor from WeakPtr<Derived> to
     * WeakPtr<Base>.
     *
     * @tparam  U Derived type that is convertible to T.
     * @param[in] other Source weak pointer.
     * @throws None
     * @note   None
     * @warning None
     * @since  N/A
     * @ingroup none
     */
    template <typename U,
              typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    WeakPtr(const WeakPtr<U>& other) noexcept
        : ptr_(other.ptr_), flag_(other.flag_) {}

    /**
     * @brief  Covariant copy assignment from WeakPtr<Derived> to WeakPtr<Base>.
     *
     * @tparam  U Derived type that is convertible to T.
     * @param[in] other Source weak pointer.
     * @return        Reference to this WeakPtr.
     * @throws        None
     * @note          None
     * @warning       None
     * @since         N/A
     * @ingroup       none
     */
    template <typename U,
              typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    WeakPtr& operator=(const WeakPtr<U>& other) noexcept {
        ptr_ = other.ptr_;
        flag_ = other.flag_;
        return *this;
    }

    /**
     * @brief  Destructor: does not affect Owner lifetime.
     * @throws None
     * @note   None
     * @warning None
     * @since  N/A
     * @ingroup none
     */
    ~WeakPtr() = default;

    // ------------------------------------------------------------
    // Access
    // ------------------------------------------------------------

    /**
     * @brief  Gets the raw pointer if the owner is still alive.
     *
     * @return Raw pointer to the owned object, or nullptr if owner is gone.
     * @throws None
     * @note   None
     * @warning None
     * @since  N/A
     * @ingroup none
     */
    [[nodiscard]] T* Get() const noexcept {
        if (flag_ && flag_->IsAlive())
            return ptr_;
        return nullptr;
    }

    /**
     * @brief  Dereferences the weak pointer.
     *
     * @return Raw pointer to the owned object.
     * @throws None (assertion failure if invalid).
     * @note   Asserts that the WeakPtr is valid before dereferencing.
     * @warning Undefined behavior if called on an invalid WeakPtr.
     * @since  N/A
     * @ingroup none
     */
    T* operator->() const noexcept {
        assert(IsValid() && "Dereferencing an invalid WeakPtr");
        return ptr_;
    }

    /**
     * @brief  Dereferences the weak pointer.
     *
     * @return Reference to the owned object.
     * @throws None (assertion failure if invalid).
     * @note   Asserts that the WeakPtr is valid before dereferencing.
     * @warning Undefined behavior if called on an invalid WeakPtr.
     * @since  N/A
     * @ingroup none
     */
    T& operator*() const noexcept {
        assert(IsValid() && "Dereferencing an invalid WeakPtr");
        return *ptr_;
    }

    // ------------------------------------------------------------
    // State Query
    // ------------------------------------------------------------

    /**
     * @brief  Checks if the owner is still alive.
     *
     * @return true if the owner is alive, false otherwise.
     * @throws None
     * @note   None
     * @warning None
     * @since  N/A
     * @ingroup none
     */
    [[nodiscard]] bool IsValid() const noexcept { return Get() != nullptr; }

    /**
     * @brief  Boolean conversion operator for validity check.
     *
     * @return true if the owner is alive, false otherwise.
     * @throws None
     * @note   None
     * @warning None
     * @since  N/A
     * @ingroup none
     */
    explicit operator bool() const noexcept { return IsValid(); }

    // ------------------------------------------------------------
    // Reset
    // ------------------------------------------------------------

    /**
     * @brief  Resets the weak pointer to empty state.
     *
     * @throws None
     * @note   After reset, IsValid() returns false.
     * @warning None
     * @since  N/A
     * @ingroup none
     */
    void Reset() noexcept {
        ptr_ = nullptr;
        flag_ = nullptr;
    }

    // ------------------------------------------------------------
    // Comparison
    // ------------------------------------------------------------

    /**
     * @brief  Equality comparison with another WeakPtr.
     *
     * @param[in] other Other WeakPtr to compare with.
     * @return        true if both weak pointers point to the same object.
     * @throws        None
     * @note          None
     * @warning       None
     * @since         N/A
     * @ingroup       none
     */
    bool operator==(const WeakPtr& other) const noexcept {
        return Get() == other.Get();
    }

    /**
     * @brief  Inequality comparison with another WeakPtr.
     *
     * @param[in] other Other WeakPtr to compare with.
     * @return        true if weak pointers point to different objects.
     * @throws        None
     * @note          None
     * @warning       None
     * @since         N/A
     * @ingroup       none
     */
    bool operator!=(const WeakPtr& other) const noexcept {
        return !(*this == other);
    }

    /**
     * @brief  Equality comparison with nullptr.
     *
     * @return true if the WeakPtr is invalid (empty).
     * @throws None
     * @note   None
     * @warning None
     * @since  N/A
     * @ingroup none
     */
    bool operator==(std::nullptr_t) const noexcept { return !IsValid(); }

    /**
     * @brief  Inequality comparison with nullptr.
     *
     * @return true if the WeakPtr is valid.
     * @throws None
     * @note   None
     * @warning None
     * @since  N/A
     * @ingroup none
     */
    bool operator!=(std::nullptr_t) const noexcept { return IsValid(); }

    // ------------------------------------------------------------
    // Dynamic Cast (Base -> Derived conversion)
    // ------------------------------------------------------------

    /**
     * @brief  Dynamically converts WeakPtr<Base> to WeakPtr<Derived>.
     *
     * @details Performs a dynamic_cast on the underlying pointer.
     *          Returns a valid WeakPtr<Derived> if the cast succeeds,
     *          otherwise returns an invalid WeakPtr.
     *
     * @tparam  Source Source type (base class).
     * @param[in]  other  WeakPtr to the source object.
     *
     * @return        WeakPtr<Derived> pointing to the same object if
     *                the object is of type Derived, otherwise invalid.
     *
     * @throws        None
     * @note          The returned WeakPtr shares the same weak reference
     *                flag as the source.
     * @warning       Always check the returned WeakPtr's validity before use.
     * @since         N/A
     * @ingroup       none
     *
     * @code
     * WeakPtr<BaseClass> basePtr = ...;
     * auto derivedPtr = WeakPtr<DerivedClass>::DynamicCast(basePtr);
     * if (derivedPtr) {
     *     derivedPtr->DerivedMethod();
     * }
     * @endcode
     */
    template <typename Source>
    static WeakPtr DynamicCast(const WeakPtr<Source>& other) noexcept {
        if (!other.flag_ || !other.flag_->IsAlive()) {
            return WeakPtr();
        }
        T* casted = dynamic_cast<T*>(other.ptr_);
        if (casted) {
            return WeakPtr(casted, other.flag_);
        }
        return WeakPtr();
    }

  private:
    // Only WeakPtrFactory can construct valid WeakPtr instances

    /**
     * @brief  Private constructor for creating valid WeakPtr instances.
     *
     * @param[in] ptr Raw pointer to the owned object.
     * @param[in] flag Weak reference flag for lifetime tracking.
     * @throws        None
     * @note          Only accessible by WeakPtrFactory.
     * @warning       None
     * @since         N/A
     * @ingroup       none
     */
    explicit WeakPtr(T* ptr, internal::WeakReferenceFlagPtr flag) noexcept
        : ptr_(ptr), flag_(std::move(flag)) {}

    /// Raw pointer to the owned object. Ownership: observer; may be nullptr.
    T* ptr_ = nullptr;

    /// Weak reference flag for lifetime tracking. Ownership: shared; may be
    /// nullptr.
    internal::WeakReferenceFlagPtr flag_ = nullptr;

    /**
     * @brief  Friend declaration for covariance support.
     *
     * @tparam U Type parameter for the befriended WeakPtr.
     *
     * @since  N/A
     * @ingroup none
     */
    template <typename U> friend class WeakPtr;

    /**
     * @brief  Friend declaration for factory access.
     *
     * @tparam U Type parameter for the befriended WeakPtrFactory.
     *
     * @since  N/A
     * @ingroup none
     */
    template <typename U> friend class WeakPtrFactory;
};

} // namespace anatomy_gui::base
