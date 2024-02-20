#include <faabric/util/bytes.h>
#include <faabric/util/delta.h>

#include <zstd.h>

#include <algorithm>
#include <cassert>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace faabric::util {

DeltaSettings::DeltaSettings(const std::string& definition)
  : usePages(false)
  , pageSize(4096)
  , xorWithOld(false)
  , useZstd(false)
  , zstdLevel(1)
{
    std::stringstream ss(definition);
    std::string part;
    while (std::getline(ss, part, ';')) {
        if (part.size() < 1) {
            continue;
        }
        if (std::string_view pfx = "pages="; part.find(pfx.data(), 0) == 0) {
            this->usePages = true;
            this->pageSize = std::stoul(part.substr(pfx.size()));
        } else if (part == "xor") {
            this->xorWithOld = true;
        } else if (std::string_view pfx = "zstd=";
                   part.find(pfx.data(), 0) == 0) {
            this->useZstd = true;
            this->zstdLevel = std::stoi(part.substr(pfx.size()));
        } else {
            throw std::invalid_argument(
              std::string("Invalid DeltaSettings configuration argument: ") +
              part);
        }
    }
}

std::string DeltaSettings::toString() const
{
    std::stringstream ss;
    if (this->usePages) {
        ss << "pages=" << this->pageSize << ';';
    }
    if (this->xorWithOld) {
        ss << "xor;";
    }
    if (this->useZstd) {
        ss << "zstd=" << this->zstdLevel << ';';
    }
    return ss.str();
}

std::vector<uint8_t> serializeDelta(
  const DeltaSettings& cfg,
  std::span<const uint8_t> oldData,
  std::span<const uint8_t> newData,
  std::span<const std::pair<uint32_t, uint32_t>> excludedPtrLens)
{
    std::vector<uint8_t> outb;
    outb.reserve(16384);
    outb.push_back(DELTA_PROTOCOL_VERSION);
    outb.push_back(DELTACMD_TOTAL_SIZE);
    appendBytesOf(outb, uint32_t(newData.size()));
    auto encodeChangedRegionRaw =
      [&](size_t startByte, size_t newLength, auto& recursiveCall) {
          if (newLength == 0) {
              return;
          }
          assert(startByte <= newData.size());
          size_t endByte = startByte + newLength;
          assert(endByte <= newData.size());
          assert(startByte <= oldData.size());
          assert(endByte <= oldData.size());
          for (const auto& [xptr, xlen] : excludedPtrLens) {
              auto xend = xptr + xlen;
              bool startExcluded = startByte >= xptr && startByte < xend;
              bool endExcluded = endByte > xptr && endByte <= xend;
              bool startEndOutside = startByte < xptr && endByte > xend;
              if (startExcluded && endExcluded) {
                  return;
              } else if (startExcluded) {
                  return recursiveCall(xend, endByte - xend, recursiveCall);
              } else if (endExcluded) {
                  return recursiveCall(
                    startByte, xptr - startByte, recursiveCall);
              } else if (startEndOutside) {
                  recursiveCall(startByte, xptr - startByte, recursiveCall);
                  return recursiveCall(xend, endByte - xend, recursiveCall);
              }
          }
          if (cfg.xorWithOld) {
              outb.push_back(DELTACMD_DELTA_XOR);
              appendBytesOf(outb, uint32_t(startByte));
              appendBytesOf(outb, uint32_t(newLength));
              size_t xorStart = outb.size();
              outb.insert(outb.end(),
                          newData.begin() + startByte,
                          newData.begin() + endByte);
              auto xorBegin = outb.begin() + xorStart;
              std::transform(&oldData[startByte],
                             &oldData[endByte],
                             xorBegin,
                             xorBegin,
                             std::bit_xor<uint8_t>());
          } else {
              outb.push_back(DELTACMD_DELTA_OVERWRITE);
              appendBytesOf(outb, uint32_t(startByte));
              appendBytesOf(outb, uint32_t(newLength));
              outb.insert(outb.end(),
                          newData.begin() + startByte,
                          newData.begin() + endByte);
          }
      };
    auto encodeChangedRegion = [&](size_t startByte, size_t newLength) {
        return encodeChangedRegionRaw(
          startByte, newLength, encodeChangedRegionRaw);
    };
    auto encodeNewRegionRaw = [&](size_t newStart,
                                  size_t newLength,
                                  auto& recursiveCall) {
        assert(newStart <= newData.size());
        while (newLength > 0 && newData[newStart] == uint8_t(0)) {
            newStart++;
            newLength--;
        }
        while (newLength > 0 &&
               newData[newStart + newLength - 1] == uint8_t(0)) {
            newLength--;
        }
        size_t newEnd = newStart + newLength;
        assert(newEnd <= newData.size());
        if (newLength == 0) {
            return;
        }
        for (const auto& [xptr, xlen] : excludedPtrLens) {
            auto xend = xptr + xlen;
            bool startExcluded = newStart >= xptr && newStart < xend;
            bool endExcluded = newEnd > xptr && newEnd <= xend;
            bool startEndOutside = newStart < xptr && newEnd > xend;
            if (startExcluded && endExcluded) {
                return;
            } else if (startExcluded) {
                return recursiveCall(xend, newEnd - xend, recursiveCall);
            } else if (endExcluded) {
                return recursiveCall(newStart, xptr - newStart, recursiveCall);
            } else if (startEndOutside) {
                recursiveCall(newStart, xptr - newStart, recursiveCall);
                return recursiveCall(xend, newEnd - xend, recursiveCall);
            }
        }
        outb.push_back(DELTACMD_DELTA_OVERWRITE);
        appendBytesOf(outb, uint32_t(newStart));
        appendBytesOf(outb, uint32_t(newLength));
        outb.insert(
          outb.end(), newData.begin() + newStart, newData.begin() + newEnd);
    };
    auto encodeNewRegion = [&](size_t newStart, size_t newLength) {
        return encodeNewRegionRaw(newStart, newLength, encodeNewRegionRaw);
    };
    if (cfg.usePages) {
        for (size_t pageStart = 0; pageStart < newData.size();
             pageStart += cfg.pageSize) {
            size_t pageEnd = pageStart + cfg.pageSize;
            bool startInBoth = (pageStart < oldData.size());
            bool endInBoth =
              (pageEnd <= newData.size()) && (pageEnd <= oldData.size());
            if (startInBoth && endInBoth) {
                bool anyChanges = !std::equal(newData.begin() + pageStart,
                                              newData.begin() + pageEnd,
                                              oldData.begin());
                if (anyChanges) {
                    encodeChangedRegion(pageStart, cfg.pageSize);
                }
            } else {
                encodeNewRegion(pageStart,
                                std::min(pageEnd, newData.size()) - pageStart);
            }
        }
    } else {
        if (newData.size() >= oldData.size()) {
            encodeChangedRegion(0, oldData.size());
            encodeNewRegion(oldData.size(), newData.size() - oldData.size());
        } else {
            encodeChangedRegion(0, newData.size());
            // Discard longer old data
        }
    }
    outb.push_back(DELTACMD_END);
    outb.shrink_to_fit();
    if (!cfg.useZstd) {
        return outb;
    } else {
        std::vector<uint8_t> compressBuffer;
        size_t compressBound = ZSTD_compressBound(outb.size());
        compressBuffer.reserve(compressBound + 19);
        compressBuffer.push_back(DELTA_PROTOCOL_VERSION);
        compressBuffer.push_back(DELTACMD_ZSTD_COMPRESSED_COMMANDS);
        size_t idxComprLen = compressBuffer.size();
        appendBytesOf(compressBuffer, uint64_t(0xDEAD)); // to be filled in
        appendBytesOf(compressBuffer, uint64_t(outb.size()));
        size_t idxCDataStart = compressBuffer.size();
        compressBuffer.insert(compressBuffer.end(), compressBound, uint8_t(0));
        auto zstdResult = ZSTD_compress(compressBuffer.data() + idxCDataStart,
                                        compressBuffer.size() - idxCDataStart,
                                        outb.data(),
                                        outb.size(),
                                        cfg.zstdLevel);
        if (ZSTD_isError(zstdResult)) {
            auto error = ZSTD_getErrorName(zstdResult);
            throw std::runtime_error(std::string("ZSTD compression error: ") +
                                     error);
        } else {
            compressBuffer.resize(idxCDataStart + zstdResult);
        }
        {
            uint64_t comprLen = zstdResult;
            std::copy_n(reinterpret_cast<uint8_t*>(&comprLen),
                        sizeof(uint64_t),
                        compressBuffer.data() + idxComprLen);
        }
        compressBuffer.push_back(DELTACMD_END);
        compressBuffer.shrink_to_fit();
        return compressBuffer;
    }
}

void applyDelta(std::span<const uint8_t> delta,
                std::function<void(uint32_t)> setDataSize,
                std::function<uint8_t*()> getDataPointer)
{
    size_t deltaLen = delta.size();
    if (deltaLen < 1) {
        throw std::runtime_error("Delta too short to be valid");
    }
    if (delta[0] != DELTA_PROTOCOL_VERSION) {
        throw std::runtime_error("Unsupported delta version");
    }
    size_t readIdx = 1;
    while (readIdx < deltaLen) {
        uint8_t cmd = delta[readIdx];
        readIdx++;
        switch (cmd) {
            case DELTACMD_TOTAL_SIZE: {
                uint32_t totalSize{};
                readIdx = readBytesOf(delta, readIdx, &totalSize);
                setDataSize(totalSize);
                break;
            }
            case DELTACMD_ZSTD_COMPRESSED_COMMANDS: {
                uint64_t compressedSize{}, decompressedSize{};
                readIdx = readBytesOf(delta, readIdx, &compressedSize);
                readIdx = readBytesOf(delta, readIdx, &decompressedSize);
                if (readIdx + compressedSize > deltaLen) {
                    throw std::range_error(
                      "Delta compressed commands block goes out of range:");
                }
                std::vector<uint8_t> decompressedCmds(decompressedSize, 0);
                auto zstdResult = ZSTD_decompress(decompressedCmds.data(),
                                                  decompressedCmds.size(),
                                                  delta.data() + readIdx,
                                                  compressedSize);
                if (ZSTD_isError(zstdResult)) {
                    auto error = ZSTD_getErrorName(zstdResult);
                    throw std::runtime_error(
                      std::string("ZSTD compression error: ") + error);
                } else if (zstdResult != decompressedCmds.size()) {
                    throw std::runtime_error(
                      "Mismatched decompression sizes in the NDP delta");
                }
                applyDelta(decompressedCmds, setDataSize, getDataPointer);
                readIdx += compressedSize;
                break;
            }
            case DELTACMD_DELTA_OVERWRITE: {
                uint32_t offset{}, length{};
                readIdx = readBytesOf(delta, readIdx, &offset);
                readIdx = readBytesOf(delta, readIdx, &length);
                if (readIdx + length > deltaLen) {
                    throw std::range_error(
                      "Delta overwrite block goes out of range");
                }
                uint8_t* data = getDataPointer();
                std::copy_n(delta.data() + readIdx, length, data + offset);
                readIdx += length;
                break;
            }
            case DELTACMD_DELTA_XOR: {
                uint32_t offset{}, length{};
                readIdx = readBytesOf(delta, readIdx, &offset);
                readIdx = readBytesOf(delta, readIdx, &length);
                if (readIdx + length > deltaLen) {
                    throw std::range_error("Delta XOR block goes out of range");
                }
                uint8_t* data = getDataPointer();
                std::transform(delta.data() + readIdx,
                               delta.data() + readIdx + length,
                               data + offset,
                               data + offset,
                               std::bit_xor<uint8_t>());
                readIdx += length;
                break;
            }
            case DELTACMD_END: {
                readIdx = deltaLen;
                break;
            }
        }
    }
}

}
