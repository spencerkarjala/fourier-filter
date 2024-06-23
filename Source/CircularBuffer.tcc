#include <cassert>

template<typename SampleType>
CircularBuffer<SampleType>::CircularBuffer()
  : m_buffer(0)
{
}

template<typename SampleType>
CircularBuffer<SampleType>::CircularBuffer(uint32_t size)
  : m_buffer(size)
{
}

template<typename SampleType>
SampleType CircularBuffer<SampleType>::getSample(uint32_t index)
{
    m_readWriteLock.lock();
    SampleType sample = m_buffer[index];
    m_readWriteLock.unlock();

    return sample;
}

template<typename SampleType>
void CircularBuffer<SampleType>::write(const SampleType& value)
{
    m_readWriteLock.lock();

    m_buffer[m_writeIndex] = value;

    if (m_writeIndex == m_buffer.size() - 1) {
        m_isFilled = true;
    }

    m_writeIndex = (m_writeIndex + 1) % static_cast<uint32_t>(m_buffer.size());

    m_readWriteLock.unlock();
}

template<typename SampleType>
void CircularBuffer<SampleType>::resize(uint32_t size)
{
    m_readWriteLock.lock();

    m_buffer.clear();
    m_buffer.resize(size);
    m_writeIndex = 0;
    m_isFilled = false;

    m_readWriteLock.unlock();
}

template<typename SampleType>
void CircularBuffer<SampleType>::copyTo(std::vector<SampleType>& destination)
{
    assert(destination.size() == m_buffer.size());

    m_readWriteLock.lock();
    memcpy(&destination[0], &m_buffer[0], sizeof(SampleType) * m_buffer.size());
    m_readWriteLock.unlock();
}

template<typename SampleType>
bool CircularBuffer<SampleType>::isFilled()
{
    m_readWriteLock.lock();
    bool isFilled = m_isFilled;
    m_readWriteLock.unlock();

    return isFilled;
}
