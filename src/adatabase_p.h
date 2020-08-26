#include "adatabase.h"

#include "adriver.h"
#include "adriverpg.h"

#include <QLoggingCategory>

class ADatabasePrivate
{
public:
    ADatabasePrivate(const QUrl &ci);
    ~ADatabasePrivate() = default;
    QUrl connectionInfo;
    ADriver *driver;
};
