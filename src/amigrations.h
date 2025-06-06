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
    explicit AMigrations(QObject *parent = nullptr);
    virtual ~AMigrations();

    /*!
     * \brief load migration information from the asql_migrations table using the specified
     * migration \p name. \param db a valid dabase option \param name of the migration \param
     * noTransactionDB a database object in case some script must run outside a transaction block
     */
    ACoroTerminator load(ADatabase db, QString name, ADatabase noTransactionDB = {});

    /*!
     * \brief active version of this migration, only valid after ready has been emitted
     * \return
     */
    int active() const;

    /*!
     * \brief latest version available
     * \return
     */
    int latest() const;

    /*!
     * \brief fromFile extract migration data from file
     * \param filename
     * \return
     */
    bool fromFile(const QString &filename);

    /*!
     * \brief fromFile extract migration data from string
     * \param filename
     * \return
     */
    void fromString(const QString &text);

    /*!
     * \brief sqlFor Get SQL to migrate from one version to another, up or down.
     * \param versionA
     * \param versionB
     * \return
     */
    QString sqlFor(int versionFrom, int versionTo) const;

    /*!
     * \brief sqlFor Get SQL to migrate from one version to another, up or down.
     * \param versionA
     * \param versionB
     * \return
     */
    QStringList sqlListFor(int versionFrom, int versionTo) const;

    /*!
     * \brief migrate Migrate from "active" to the latest version.
     * All version numbers need to be positive, with version 0 representing an empty database.
     *
     * \sa finished() signal is emitted with the result.
     * \param cb callback function that is called to inform the result
     * \param dryRun if set will rollback the transaction instead of committing, this option
     * diverges from regular operation as it will perform a single transaction block with all
     * up/down steps at once, which depending on the operation will fail.
     */
    void migrate(std::function<void(bool error, const QString &errorString)> cb,
                 bool dryRun = false);

    /*!
     * \brief migrate Migrate from "active" to different version, up or down.
     * All version numbers need to be positive, with version 0 representing an empty database.
     *
     * \sa finished() signal is emitted with the result.
     * \param targetVersion to try to apply changes
     * \param cb callback function that is called to inform the result
     * \param dryRun if set will rollback the transaction instead of committing, this option
     * diverges from regular operation as it will perform a single transaction block with all
     * up/down steps at once, which depending on the operation will fail.
     */
    ACoroTerminator migrate(int targetVersion,
                            std::function<void(bool error, const QString &errorString)> cb,
                            bool dryRun = false);

Q_SIGNALS:
    void ready(bool error, const QString &errorString);

private:
    AMigrationsPrivate *d_ptr;
};

} // namespace ASql
