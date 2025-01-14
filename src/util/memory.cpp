#include <faabric/util/logging.h>
#include <faabric/util/memory.h>
#include <faabric/util/timing.h>

#include <fcntl.h>
#include <shared_mutex>
#include <stdexcept>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>

namespace faabric::util {

void mergeManyDirtyPages(std::vector<char>& dest,
                         const std::vector<std::vector<char>>& source)
{
    for (const auto& v : source) {
        mergeDirtyPages(dest, v);
    }
}

void mergeDirtyPages(std::vector<char>& dest, const std::vector<char>& source)
{
    // Extend a to fit
    size_t overlap = dest.size();
    if (source.size() > dest.size()) {
        dest.reserve(source.size());
        dest.insert(dest.end(), source.begin() + dest.size(), source.end());
    } else if (source.size() < dest.size()) {
        overlap = source.size();
    }

    std::transform(dest.begin(),
                   dest.begin() + overlap,
                   source.begin(),
                   dest.begin(),
                   std::logical_or<char>());
}

// -------------------------
// Alignment
// -------------------------

bool isPageAligned(const void* ptr)
{
    return (((uintptr_t)(ptr)) % (HOST_PAGE_SIZE) == 0);
}

size_t getRequiredHostPages(size_t nBytes)
{
    // Rounding up
    size_t nHostPages = (nBytes + faabric::util::HOST_PAGE_SIZE - 1) /
                        faabric::util::HOST_PAGE_SIZE;
    return nHostPages;
}

size_t getRequiredHostPagesRoundDown(size_t nBytes)
{
    // Relying on integer division rounding down
    size_t nHostPages = nBytes / faabric::util::HOST_PAGE_SIZE;
    return nHostPages;
}

size_t alignOffsetDown(size_t offset)
{
    size_t nHostPages = getRequiredHostPagesRoundDown(offset);
    return nHostPages * faabric::util::HOST_PAGE_SIZE;
}

AlignedChunk getPageAlignedChunk(long offset, long length)
{
    // Calculate the page boundaries
    auto nPagesOffset =
      (long)faabric::util::getRequiredHostPagesRoundDown(offset);
    auto nPagesUpper =
      (long)faabric::util::getRequiredHostPages(offset + length);
    long nPagesLength = nPagesUpper - nPagesOffset;

    long nBytesLength = nPagesLength * faabric::util::HOST_PAGE_SIZE;

    long nBytesOffset = nPagesOffset * faabric::util::HOST_PAGE_SIZE;

    // This value is the offset from the base of the new region
    long shiftedOffset = offset - nBytesOffset;

    AlignedChunk c{
        .originalOffset = offset,
        .originalLength = length,
        .nBytesOffset = nBytesOffset,
        .nBytesLength = nBytesLength,
        .nPagesOffset = nPagesOffset,
        .nPagesLength = nPagesLength,
        .offsetRemainder = shiftedOffset,
    };

    return c;
}

// UserfaultFd wrapper
std::pair<int, uffdio_api> UserfaultFd::release()
{
    int oldFd = fd;
    uffdio_api oldApi = api;
    fd = -1;
    api = {};
    return std::make_pair(oldFd, oldApi);
}

void UserfaultFd::create(int flags, bool sigbus)
{
    clear();
    int result = syscall(SYS_userfaultfd, flags);
    if (result < 0) {
        errno = -result;
        perror("Error creating userfaultfd");
        throw std::runtime_error("Couldn't create userfaultfd");
    }
    fd = result;
    api.api = UFFD_API;
    api.features = UFFD_FEATURE_THREAD_ID;
    if (sigbus) {
        api.features |= UFFD_FEATURE_SIGBUS;
    }
    api.ioctls = 0;
    result = ioctl(fd, UFFDIO_API, &api);
    if (result < 0) {
        throw std::runtime_error("Couldn't handshake userfaultfd api");
    }
}

void UserfaultFd::registerAddressRange(size_t startPtr,
                                       size_t length,
                                       bool modeMissing,
                                       bool modeWriteProtect)
{
    checkFd();
    uffdio_register r = {};
    if (!(modeMissing || modeWriteProtect)) {
        throw std::invalid_argument(
          "UFFD register call must have at least one mode enabled");
    }
    if (modeMissing) {
        r.mode |= UFFDIO_REGISTER_MODE_MISSING;
    }
    if (modeWriteProtect) {
        if ((api.features & UFFD_FEATURE_PAGEFAULT_FLAG_WP) == 0) {
            throw std::runtime_error("WriteProtect mode on UFFD not supported");
        }
        r.mode |= UFFDIO_REGISTER_MODE_WP;
    }
    r.range.start = startPtr;
    r.range.len = length;
    if (ioctl(fd, UFFDIO_REGISTER, &r) < 0) {
        perror("UFFDIO_REGISTER error");
        throw std::runtime_error(
          "Couldn't register an address range with UFFD");
    }
}

void UserfaultFd::unregisterAddressRange(size_t startPtr, size_t length)
{
    checkFd();
    uffdio_range r = {};
    r.start = startPtr;
    r.len = length;
    if (ioctl(fd, UFFDIO_UNREGISTER, &r) < 0) {
        perror("UFFDIO_UNREGISTER error");
        throw std::runtime_error(
          "Couldn't unregister an address range from UFFD");
    }
}

std::optional<uffd_msg> UserfaultFd::readEvent()
{
    checkFd();
    uffd_msg ev;
retry:
    int result = read(fd, (void*)&ev, sizeof(uffd_msg));
    if (result < 0) {
        if (errno == EAGAIN) {
            goto retry;
        } else if (errno == EWOULDBLOCK) {
            return std::nullopt;
        } else {
            perror("read from UFFD error");
            throw std::runtime_error("Error reading from the UFFD");
        }
    }
    return ev;
}
void UserfaultFd::writeProtectPages(size_t startPtr,
                                    size_t length,
                                    bool preventWrites,
                                    bool dontWake)
{
    checkFd();
    if ((api.features & UFFD_FEATURE_PAGEFAULT_FLAG_WP) == 0) {
        throw std::runtime_error("Write-protect pages not supported by "
                                 "UFFD on this kernel version");
    }
    uffdio_writeprotect wp = {};
    if (preventWrites) {
        wp.mode |= UFFDIO_WRITEPROTECT_MODE_WP;
    }
    if (dontWake) {
        wp.mode |= UFFDIO_WRITEPROTECT_MODE_DONTWAKE;
    }
    wp.range.start = startPtr;
    wp.range.len = length;
retry:
    if (ioctl(fd, UFFDIO_WRITEPROTECT, &wp) < 0) {
        if (errno == EAGAIN) {
            goto retry;
        }
        if (errno == EEXIST) {
            return;
        }
        perror("UFFDIO_WRITEPROTECT error");
        throw std::runtime_error(
          "Couldn't write-protect-modify an address range through UFFD");
    }
}

void UserfaultFd::zeroPages(size_t startPtr, size_t length, bool dontWake)
{
    checkFd();
    uffdio_zeropage zp = {};
    if (dontWake) {
        zp.mode |= UFFDIO_ZEROPAGE_MODE_DONTWAKE;
    }
    zp.range.start = startPtr;
    zp.range.len = length;
retry:
    if (ioctl(fd, UFFDIO_ZEROPAGE, &zp) < 0) {
        if (errno == EAGAIN) {
            goto retry;
        }
        if (errno == EEXIST) {
            return;
        }
        perror("UFFDIO_ZEROPAGE error");
        throw std::runtime_error(
          "Couldn't zero-page an address range through UFFD");
    }
}

void UserfaultFd::copyPages(size_t targetStartPtr,
                            size_t length,
                            size_t sourceStartPtr,
                            bool writeProtect,
                            bool dontWake)
{
    checkFd();
    uffdio_copy cp = {};
    if (dontWake) {
        cp.mode |= UFFDIO_COPY_MODE_DONTWAKE;
    }
    if (writeProtect) {
        cp.mode |= UFFDIO_COPY_MODE_WP;
    }
    cp.src = sourceStartPtr;
    cp.len = length;
    cp.dst = targetStartPtr;
retry:
    if (ioctl(fd, UFFDIO_COPY, &cp) < 0) {
        if (errno == EAGAIN) {
            goto retry;
        }
        if (errno == EEXIST) {
            return;
        }
        perror("UFFDIO_COPY error");
        throw std::runtime_error("Couldn't copy an address range through UFFD");
    }
}

void UserfaultFd::wakePages(size_t startPtr, size_t length)
{
    checkFd();
    uffdio_range wr = {};
    wr.start = startPtr;
    wr.len = length;
retry:
    if (ioctl(fd, UFFDIO_WAKE, &wr) < 0) {
        if (errno == EAGAIN) {
            goto retry;
        }
        perror("UFFDIO_WAKE error");
        throw std::runtime_error("Couldn't wake an address range through UFFD");
    }
}

// -------------------------
// Allocation
// -------------------------

MemoryRegion doAlloc(size_t size, int prot, int flags)
{
    auto deleter = [size](uint8_t* u) { munmap(u, size); };
    MemoryRegion mem((uint8_t*)::mmap(nullptr, size, prot, flags, -1, 0),
                     deleter);

    if (mem.get() == MAP_FAILED) {
        SPDLOG_ERROR("Allocating memory with mmap failed: {} ({})",
                     errno,
                     ::strerror(errno));
        throw std::runtime_error("Allocating memory failed");
    }

    return mem;
}

MemoryRegion allocatePrivateMemory(size_t size)
{
    return doAlloc(size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS);
}

MemoryRegion allocateSharedMemory(size_t size)
{
    return doAlloc(size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS);
}

MemoryRegion allocateVirtualMemory(size_t size)
{
    return doAlloc(size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS);
}

void claimVirtualMemory(std::span<uint8_t> region)
{
    int protectRes =
      ::mprotect(region.data(), region.size(), PROT_READ | PROT_WRITE);
    if (protectRes != 0) {
        SPDLOG_ERROR("Failed claiming virtual memory: {}", strerror(errno));
        throw std::runtime_error("Failed claiming virtual memory");
    }
}

void mapMemory(std::span<uint8_t> target, int fd, int flags)
{
    if (!faabric::util::isPageAligned((void*)target.data())) {
        SPDLOG_ERROR("Mapping memory to non page-aligned address");
        throw std::runtime_error("Mapping memory to non page-aligned address");
    }

    if (fd <= 0) {
        SPDLOG_ERROR("Mapping invalid or zero fd ({})", fd);
        throw std::runtime_error("Invalid fd for mapping");
    }

    void* mmapRes = ::mmap(
      target.data(), target.size(), PROT_READ | PROT_WRITE, flags, fd, 0);

    if (mmapRes == MAP_FAILED) {
        SPDLOG_ERROR("mapping memory to fd {} failed: {} ({})",
                     fd,
                     errno,
                     ::strerror(errno));
        throw std::runtime_error("mmapping memory failed");
    }
}

void mapMemoryPrivate(std::span<uint8_t> target, int fd)
{
    mapMemory(target, fd, MAP_PRIVATE | MAP_FIXED);
}

void mapMemoryShared(std::span<uint8_t> target, int fd)
{
    mapMemory(target, fd, MAP_SHARED | MAP_FIXED);
}

void resizeFd(int fd, size_t size)
{
    int ferror = ::ftruncate(fd, size);
    if (ferror != 0) {
        SPDLOG_ERROR("ftruncate call failed with error {}", ferror);
        throw std::runtime_error("Failed writing memory to fd (ftruncate)");
    }
}

void writeToFd(int fd, off_t offset, std::span<const uint8_t> data)
{
    // Seek to the right point
    off_t lseekRes = ::lseek(fd, offset, SEEK_SET);
    if (lseekRes == -1) {
        SPDLOG_ERROR("Failed to set fd {} to offset {}", fd, offset);
        throw std::runtime_error("Failed changing fd size");
    }

    // Write the data
    ssize_t werror = ::write(fd, data.data(), data.size());
    if (werror == -1) {
        SPDLOG_ERROR("Write call failed with error {}", werror);
        throw std::runtime_error("Failed writing memory to fd (write)");
    }

    // Set back to end
    ::lseek(fd, 0, SEEK_END);
}

int createFd(size_t size, const std::string& fdLabel)
{
    // Create new fd
    int fd = ::memfd_create(fdLabel.c_str(), 0);
    if (fd == -1) {
        SPDLOG_ERROR("Failed to create file descriptor: {}", strerror(errno));
        throw std::runtime_error("Failed to create file descriptor");
    }

    // Make the fd big enough
    resizeFd(fd, size);

    return fd;
}

void appendDataToFd(int fd, std::span<uint8_t> data)
{
    off_t oldSize = ::lseek(fd, 0, SEEK_END);
    if (oldSize == -1) {
        SPDLOG_ERROR("lseek to get old size failed: {}", strerror(errno));
        throw std::runtime_error("Failed seeking existing size of fd");
    }

    if (data.empty()) {
        return;
    }

    // Extend the fd
    off_t newSize = oldSize + data.size();
    int ferror = ::ftruncate(fd, newSize);
    if (ferror != 0) {
        SPDLOG_ERROR("Extending with ftruncate failed with error {}", ferror);
        throw std::runtime_error("Failed appending data to fd (ftruncate)");
    }

    // Skip to the end of the old data
    off_t seekRes = ::lseek(fd, oldSize, SEEK_SET);
    if (seekRes == -1) {
        SPDLOG_ERROR("lseek call failed with error {}", strerror(errno));
        throw std::runtime_error("Failed appending data to fd");
    }

    // Write the data
    ssize_t werror = ::write(fd, data.data(), data.size());
    if (werror == -1) {
        SPDLOG_ERROR("Appending with write failed with error {}", werror);
        throw std::runtime_error("Failed appending memory to fd (write)");
    }
}
}
