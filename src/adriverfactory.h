#ifndef ADRIVERFACTORY_H
#define ADRIVERFACTORY_H

#include <adriver.h>
#include <aqsqlexports.h>

class ASQL_EXPORT ADriverFactory
{
public:
    ADriverFactory();
    virtual ~ADriverFactory();

    virtual ADriver *createRawDriver() const;
    virtual std::shared_ptr<ADriver> createDriver() const;
    virtual ADatabase database() const;
};

#endif // ADRIVERFACTORY_H
