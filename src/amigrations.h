#ifndef AMIGRATIONS_H
#define AMIGRATIONS_H

#include <QObject>
#include <adatabase.h>

class AMigrationsPrivate;
class ASQL_EXPORT AMigrations : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(AMigrations)
public:
    explicit AMigrations(QObject *parent = nullptr);
    virtual ~AMigrations();

    /*!
     * \brief load migration information from the asql_migrations table using the specified migration \p name.
     * \param db a valid dabase option
     * \param name of the migration
     */
    void load(const ADatabase &db, const QString &name);

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
     * \brief migrate Migrate from "active" to the latest version.
     * All version numbers need to be positive, with version 0 representing an empty database.
     *
     * \sa finished() signal is emitted with the result.
     * \param cb callback function that is called to inform the result
     * \param dryRun if set will rollback the transaction instead of committing
     */
    void migrate(std::function<void(bool error, const QString &errorString)> cb, bool dryRun = false);

    /*!
     * \brief migrate Migrate from "active" to different version, up or down.
     * All version numbers need to be positive, with version 0 representing an empty database.
     *
     * \sa finished() signal is emitted with the result.
     * \param version to try to apply changes
     * \param cb callback function that is called to inform the result
     * \param dryRun if set will rollback the transaction instead of committing
     */
    void migrate(int version, std::function<void(bool error, const QString &errorString)> cb, bool dryRun = false);

Q_SIGNALS:
    void ready(bool error, const QString &errorString);

private:
    AMigrationsPrivate *d_ptr;
};

#endif // AMIGRATIONS_H
