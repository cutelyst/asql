/*
 * SPDX-FileCopyrightText: (C) 2020-2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <acoroexpected.h>
#include <adatabase.h>
#include <asql_migrations_export.h>

#include <QObject>

namespace ASql {

class AMigrationsPrivate;
class ASQL_MIGRATIONS_EXPORT AMigrations : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(AMigrations)
public:
    using AExpectedMigration = ACoroExpected<void>;

    explicit AMigrations(QObject *parent = nullptr);
    virtual ~AMigrations();

    /*!
     * \brief load migration information from the asql_migrations table using the specified
     * migration \p name.
     */
    [[nodiscard]] AExpectedMigration
        load(ADatabase db, QString name, ADatabase noTransactionDB = {});

    /*!
     * \brief active version of this migration, only valid after \l load() succeeds
     */
    int active() const;

    /*!
     * \brief latest version available
     */
    int latest() const;

    bool fromFile(const QString &filename);
    void fromString(const QString &text);

    QString sqlFor(int versionFrom, int versionTo) const;
    QStringList sqlListFor(int versionFrom, int versionTo) const;

    /*!
     * \brief migrate from \l active() toward \p targetVersion (or \l latest() when \c -1).
     */
    [[nodiscard]] AExpectedMigration migrate(int targetVersion = -1, bool dryRun = false);

private:
    static ACoroTerminator loadCoroutine(std::shared_ptr<ACoroData<void>> data,
                                         AMigrations *q,
                                         ADatabase db,
                                         QString name,
                                         ADatabase noTransactionDB);
    static ACoroTerminator migrateCoroutine(std::shared_ptr<ACoroData<void>> data,
                                            AMigrations *q,
                                            int targetVersion,
                                            bool dryRun);

    AMigrationsPrivate *d_ptr;
};

} // namespace ASql
