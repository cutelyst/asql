#include "adatabase.h"

#include "adriver.h"
#include "adriverpg.h"

#include <QLoggingCategory>

class ADatabasePrivate
{
public:
    ADatabasePrivate(const QString &ci);
    ~ADatabasePrivate();
    QString connectionInfo;
    ADriver *driver;
};
