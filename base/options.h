// base/options.h - Container for passing around options
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_OPTIONS_H
#define BASE_OPTIONS_H

#include <memory>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

namespace base {

// OptionsType is an empty base class to mark option types.
struct OptionsType {};

// Options is a type-safe container, holding one instance each of various
// option classes (each option class marked by subclassing OptionsType).
//
// Thread-safety: the usual C++11 rules apply; methods marked |const| may be
//                called concurrently with each other, but must not be called
//                concurrently with any non-|const| methods.
//
class Options {
 private:
  template <typename T>
  using is_option = typename std::is_base_of<OptionsType, T>::type;

 public:
  // Options is default constructible.
  Options() = default;

  // Options is copyable.
  Options(const Options& other);
  Options& operator=(const Options& other);

  // Options is moveable.
  Options(Options&& other) noexcept = default;
  Options& operator=(Options&& other) noexcept = default;

  // Accesses the value for the given option class.
  template <typename T, typename SFINAE =
                            typename std::enable_if<is_option<T>::value>::type>
  T& get() {
    using U = typename std::decay<T>::type;
    auto& holder = map_[std::type_index(typeid(U))];
    if (!holder) holder.reset(new Holder<U>);
    return *static_cast<U*>(holder->pointer());
  }

  // Accesses the value for the given option class. [const]
  template <typename T, typename SFINAE =
                            typename std::enable_if<is_option<T>::value>::type>
  const T& get() const {
    using U = typename std::decay<T>::type;
    auto it = map_.find(std::type_index(typeid(U)));
    if (it == map_.end()) {
      static const U& ref = *new U;
      return ref;
    }
    return *static_cast<const U*>(it->second->pointer());
  }

  // Alias for get<T>().
  template <typename T, typename SFINAE =
                            typename std::enable_if<is_option<T>::value>::type>
  operator T&() {
    return get<T>();
  }

  // Alias for get<T>(). [const]
  template <typename T, typename SFINAE =
                            typename std::enable_if<is_option<T>::value>::type>
  operator const T&() const {
    return get<T>();
  }

 private:
  struct HolderBase {
    virtual ~HolderBase() noexcept = default;
    virtual void* pointer() noexcept = 0;
    virtual std::unique_ptr<HolderBase> copy() = 0;
  };

  template <typename T>
  struct Holder : public HolderBase {
    T value;

    void* pointer() noexcept override { return &value; }

    std::unique_ptr<HolderBase> copy() override {
      return std::unique_ptr<HolderBase>(new Holder(*this));
    }
  };

  std::unordered_map<std::type_index, std::unique_ptr<HolderBase>> map_;
};

// Returns the default Options. Thread-safe.
Options default_options();

// Changes the default Options. Thread-safe.
void set_default_options(Options opts);

}  // namespace base

#endif  // BASE_OPTIONS_H
