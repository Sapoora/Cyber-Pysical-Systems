#ifndef USART_H
#define USART_H

#include <CPS4042/Protocols/Protocol.h>
#include <CPS4042/Units/Bit.h>
#include <CPS4042/Units/Byte.h>
#include <queue>

namespace Protocols
{
namespace USART
{

class Receiver
{
public:
    void
    push(Bit bit)
    {
        switch(m_state)
        {
            case State::Idle :
                if(bit == Bit::Zero)
                {
                    m_state = State::Data;
                    m_value = 0;
                    m_index = 0;
                }
                break;

            case State::Data :
                if(bit == Bit::One)
                    m_value |= static_cast<UByte>(1U << m_index);

                ++m_index;
                if(m_index == bitWidth<Byte>()) m_state = State::Stop;
                break;

            case State::Stop :
                if(bit == Bit::One) m_buffer.push(static_cast<Byte>(m_value));
                reset();
                break;
        }
    }

    bool
    isDataAvailable() const
    {
        return !m_buffer.empty();
    }

    Byte
    read()
    {
        Byte byte = m_buffer.front();
        m_buffer.pop();
        return byte;
    }

private:
    enum class State
    {
        Idle,
        Data,
        Stop
    };

    void
    reset()
    {
        m_state = State::Idle;
        m_value = 0;
        m_index = 0;
    }

private:
    State            m_state {State::Idle};
    UByte            m_value {0};
    std::uint8_t     m_index {0};
    std::queue<Byte> m_buffer;
};

template <typename TxPin>
void
writeFrame(TxPin& tx, Byte byte)
{
    tx.write(Bit::Zero);

    auto value = static_cast<UByte>(byte);
    for(std::uint8_t i = 0; i < bitWidth<Byte>(); ++i)
    {
        tx.write(takeNthBit(value, i));
    }

    tx.write(Bit::One);
}

}    // namespace USART
}    // namespace Protocols

#endif    // USART_H
