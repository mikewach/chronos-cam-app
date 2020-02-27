/*
 * This file was NOT generated by qdbusxml2cpp version 0.7
 * Command line WOULD HAVE BEEN qdbusxml2cpp -p chronosControlInterface control.xml
 *
 * qdbusxml2cpp is Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 *
 * This is NOT an auto-generated file.
 * Edit this file manually!
 */

#ifndef CHRONOSCONTROLINTERFACE_H_1517442513
#define CHRONOSCONTROLINTERFACE_H_1517442513

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtDBus/QtDBus>

/*
 * Proxy class for interface ca.krontech.chronos.control
 */
class CaKrontechChronosControlInterface: public QDBusAbstractInterface
{
    Q_OBJECT
public:
    static inline const char *staticInterfaceName()
	{ return "ca.krontech.chronos.control"; }

public:
	CaKrontechChronosControlInterface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = 0);
	~CaKrontechChronosControlInterface();

public Q_SLOTS: // METHODS

	// manually entered for NEWAPI
	// in/out methdods:
	inline QDBusPendingReply<QVariantMap> startRecording(const QVariantMap &args)
	{
		QList<QVariant> argumentList;
		argumentList << qVariantFromValue(args);
		return asyncCallWithArgumentList(QLatin1String("startRecording"), argumentList);
	}

	inline QDBusPendingReply<QVariantMap> stopRecording()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QLatin1String("stopRecording"), argumentList);
	}

	inline QDBusPendingReply<QVariantMap> recordfile(const QVariantMap &settings)
	{
		QList<QVariant> argumentList;
		argumentList << qVariantFromValue(settings);
		return asyncCallWithArgumentList(QLatin1String("startFilesave"), argumentList);
	}

	inline QDBusPendingReply<QVariantMap> testResolution(const QVariantMap &args)
	{
		QList<QVariant> argumentList;
		argumentList << qVariantFromValue(args);
		return asyncCallWithArgumentList(QLatin1String("getResolutionTimingLimits"), argumentList);
	}

	inline QDBusPendingReply<QVariantMap> reboot(const QVariantMap &args)
	{
		QList<QVariant> argumentList;
		argumentList << qVariantFromValue(args);
		return asyncCallWithArgumentList(QLatin1String("reboot"), argumentList);
	}

	inline QDBusPendingReply<QVariantMap> startAutoWhiteBalance(const QVariantMap &args)
	{
		QList<QVariant> argumentList;
		argumentList << qVariantFromValue(args);
		return asyncCallWithArgumentList(QLatin1String("startAutoWhiteBalance"), argumentList);
	}

	inline QDBusPendingReply<QVariantMap> get(const QStringList &names)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(names);
		return asyncCallWithArgumentList(QLatin1String("get"), argumentList);
	}

	inline QDBusPendingReply<QVariantMap> set(const QVariantMap &args)
	{
		QList<QVariant> argumentList;
		argumentList << qVariantFromValue(args);
		return asyncCallWithArgumentList(QLatin1String("set"), argumentList);
	}

	inline QDBusPendingReply<QVariantMap> revertAutoWhiteBalance(const QVariantMap &args)
	{
		QList<QVariant> argumentList;
		argumentList << qVariantFromValue(args);
		return asyncCallWithArgumentList(QLatin1String("revertAutoWhiteBalance"), argumentList);
	}

	inline QDBusPendingReply<QVariantMap> startCalibration(const QVariantMap &args)
	{
		QList<QVariant> argumentList;
		argumentList << qVariantFromValue(args);
		return asyncCallWithArgumentList(QLatin1String("startCalibration"), argumentList);
	}

	inline QDBusPendingReply<QVariantMap> exportCalData(const QVariantMap &args)
	{
		QList<QVariant> argumentList;
		argumentList << qVariantFromValue(args);
		return asyncCallWithArgumentList(QLatin1String("exportCalData"), argumentList);
	}

	inline QDBusPendingReply<QVariantMap> importCalData(const QVariantMap &args)
	{
		QList<QVariant> argumentList;
		argumentList << qVariantFromValue(args);
		return asyncCallWithArgumentList(QLatin1String("importCalData"), argumentList);
	}
	// out only methods


	inline QDBusPendingReply<QVariantMap> status(const QVariantMap &args)
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QLatin1String("status"), argumentList);
	}

	inline QDBusPendingReply<QVariantMap> availableKeys(const QVariantMap &args)
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QLatin1String("availableKeys"), argumentList);
	}

	inline QDBusPendingReply<QVariantMap> availableCalls(const QVariantMap &args)
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QLatin1String("availableCalls"), argumentList);
	}

Q_SIGNALS: // SIGNALS
};

/*
 * Proxy class for interface org.freedesktop.DBus.Introspectable
 */
class OrgFreedesktopDBusIntrospectableInterface: public QDBusAbstractInterface
{
    Q_OBJECT
public:
    static inline const char *staticInterfaceName()
    { return "org.freedesktop.DBus.Introspectable"; }

public:
    OrgFreedesktopDBusIntrospectableInterface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = 0);

    ~OrgFreedesktopDBusIntrospectableInterface();

public Q_SLOTS: // METHODS
    inline QDBusPendingReply<QString> Introspect()
    {
        QList<QVariant> argumentList;
        return asyncCallWithArgumentList(QLatin1String("Introspect"), argumentList);
    }

Q_SIGNALS: // SIGNALS
};

/*
 * Proxy class for interface org.freedesktop.DBus.Properties
 */
class OrgFreedesktopDBusPropertiesInterface: public QDBusAbstractInterface
{
    Q_OBJECT
public:
    static inline const char *staticInterfaceName()
    { return "org.freedesktop.DBus.Properties"; }

public:
    OrgFreedesktopDBusPropertiesInterface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = 0);

    ~OrgFreedesktopDBusPropertiesInterface();

public Q_SLOTS: // METHODS
    inline QDBusPendingReply<QDBusVariant> Get(const QString &interface, const QString &propname)
    {
        QList<QVariant> argumentList;
        argumentList << qVariantFromValue(interface) << qVariantFromValue(propname);
        return asyncCallWithArgumentList(QLatin1String("Get"), argumentList);
    }

    inline QDBusPendingReply<QVariantMap> GetAll(const QString &interface)
    {
        QList<QVariant> argumentList;
        argumentList << qVariantFromValue(interface);
        return asyncCallWithArgumentList(QLatin1String("GetAll"), argumentList);
    }

    inline QDBusPendingReply<> Set(const QString &interface, const QString &propname, const QDBusVariant &value)
    {
        QList<QVariant> argumentList;
        argumentList << qVariantFromValue(interface) << qVariantFromValue(propname) << qVariantFromValue(value);
        return asyncCallWithArgumentList(QLatin1String("Set"), argumentList);
    }

Q_SIGNALS: // SIGNALS
};

namespace com {
  namespace krontech {
    namespace chronos {
	  typedef ::CaKrontechChronosControlInterface control;
    }
  }
}
namespace org {
  namespace freedesktop {
    namespace DBus {
      typedef ::OrgFreedesktopDBusIntrospectableInterface Introspectable;
      typedef ::OrgFreedesktopDBusPropertiesInterface Properties;
    }
  }
}
#endif
