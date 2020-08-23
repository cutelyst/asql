#include "adriver.h"

ADriver::ADriver()
{

}

QUrl ADriver::connectionInfo() const
{
    return m_info;
}

void ADriver::setConnectionInfo(const QUrl &info)
{
    m_info = info;
}
