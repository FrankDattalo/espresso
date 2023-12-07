#ifndef ESPRESSO_SYSTEM_HH_
#define ESPRESSO_SYSTEM_HH_

#include <cstddef>

class System {
public:
  virtual ~System() = default;
  virtual void* Allocate(std::size_t sizeInBytes) = 0;
  virtual void Free(void* ptr, std::size_t sizeInBytes) = 0;
  virtual void Write(char* msg, std::size_t length) = 0;
  virtual std::size_t Read(char* buffer, std::size_t length) = 0;
};

class DefaultSystem : public System {
public:
  DefaultSystem() = default;
  virtual ~DefaultSystem() = default;
  void* Allocate(std::size_t sizeInBytes) override;
  void Free(void* ptr, std::size_t sizeInBytes) override;
  void Write(char* msg, std::size_t length) override;
  std::size_t Read(char* buffer, std::size_t length) override;
};

#endif // ESPRESSO_SYSTEM_HH_