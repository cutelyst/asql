#ifndef ASQL_EXPORT_H
#define ASQL_EXPORT_H

#include <QtCore/QtGlobal>

#if defined(ASqlQt5_EXPORTS)
#define ASQL_EXPORT Q_DECL_EXPORT
#else
#define ASQL_EXPORT Q_DECL_IMPORT
#endif

#endif
