#include "adatabase.h"

#include "adriver.h"
#include "adriverpg.h"

#include <QLoggingCategory>

class ADatabasePrivate
{
public:
    ADatabasePrivate(const QUrl &ci);
    ~ADatabasePrivate();
    QUrl connectionInfo;
    ADriver *driver;
};
