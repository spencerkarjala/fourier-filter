#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <mutex>
#include <vector>

template<typename SampleType>
class CircularBuffer
{
  public:
    CircularBuffer();
    CircularBuffer(uint32_t size);

    SampleType getSample(uint32_t index);

    void write(const SampleType& value);
    void resize(uint32_t size);
    void copyTo(std::vector<SampleType>& destination);

    bool isFilled();

  private:
    std::vector<SampleType> m_buffer;

    unsigned int m_writeIndex = 0;
    bool m_isFilled = false;

    std::mutex m_readWriteLock;
};

#include "CircularBuffer.tcc"

#endif
