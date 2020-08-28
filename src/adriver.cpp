#include "adriver.h"

ADriver::ADriver()
{

}

QString ADriver::connectionInfo() const
{
    return m_info;
}

void ADriver::setConnectionInfo(const QString &info)
{
    m_info = info;
}
