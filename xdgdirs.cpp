/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * Razor - a lightweight, Qt based, desktop toolset
 * http://razor-qt.org
 *
 * Copyright: 2010-2011 Razor team
 * Authors:
 *   Alexander Sokoloff <sokoloff.a@gmail.com>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */


#include "xdgdirs.h"
#include <stdlib.h>
#include <QDir>
#include <QStringBuilder> // for the % operator
#include <QDebug>
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#include <QStandardPaths>
#endif

static const QString userDirectoryString[8] =
{
    "Desktop",
    "Download",
    "Templates",
    "Publicshare",
    "Documents",
    "Music",
    "Pictures",
    "Videos"
};

// Helper functions prototypes
void fixBashShortcuts(QString &s);
void removeEndingSlash(QString &s);
QString createDirectory(const QString &dir);

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
void cleanAndAddPostfix(QStringList &dirs, const QString& postfix);
#endif

#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
QString xdgSingleDir(const QString &envVar, const QString &def, bool createDir);
QStringList xdgDirList(const QString &envVar, const QString &postfix);
#endif

/************************************************
 Helper func.
 ************************************************/
void fixBashShortcuts(QString &s)
{
    if (s.startsWith(QLatin1Char('~')))
        s = QString(getenv("HOME")) + (s).mid(1);
}

void removeEndingSlash(QString &s)
{
    // We don't check for empty strings. Caller must check it.

    // Remove the ending slash, except for root dirs.
    if (s.length() > 1 && s.endsWith(QLatin1Char('/')))
        s.chop(1);
}

QString createDirectory(const QString &dir)
{
    QDir d(dir);
    if (!d.exists())
    {
        if (!d.mkpath("."))
        {
            qWarning() << QString("Can't create %1 directory.").arg(d.absolutePath());
        }
    }
    QString r = d.absolutePath();
    removeEndingSlash(r);
    return r;
}

void cleanAndAddPostfix(QStringList &dirs, const QString& postfix)
{
    const int N = dirs.count();
    for(int i = 0; i < N; ++i)
    {
        fixBashShortcuts(dirs[i]);
        removeEndingSlash(dirs[i]);
        dirs[i].append(postfix);
    }
}

/************************************************
 Helper func.
 ************************************************/
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
QString xdgSingleDir(const QString &envVar, const QString &def, bool createDir)
{
    QString s(getenv(envVar.toAscii()));

    if (!s.isEmpty())
        fixBashShortcuts(s);
    else
        s = QString("%1/%2").arg(getenv("HOME"), def);

    if (createDir)
        return createDirectory(s);

    removeEndingSlash(s);
    return s;
}
#endif

/************************************************
 Helper func.
 ************************************************/
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
QStringList xdgDirList(const QString &envVar, const QString &postfix)
{
    QStringList dirs = QString(getenv(envVar.toAscii())).split(':', QString::SkipEmptyParts);

    QMutableStringListIterator i(dirs);
    while(i.hasNext()) {
        i.next();
        QString s = i.value();
        if (s.isEmpty()) {
            i.remove();
        } else {
            fixBashShortcuts(s);
            removeEndingSlash(s);
            i.setValue(s % postfix);
        }
    }
    return dirs;
}
#endif
/************************************************

 ************************************************/
QString XdgDirs::userDir(XdgDirs::UserDirectory dir)
{
    // possible values for UserDirectory
    if (dir < 0 || dir > 7)
        return QString();

    QString folderName = userDirectoryString[dir];

    QString fallback;
    if (getenv("HOME") == NULL)
        return QString("/tmp");
    else if (dir == XdgDirs::Desktop)
        fallback = QString("%1/%2").arg(getenv("HOME")).arg("Desktop");
    else
        fallback = QString(getenv("HOME"));

    QString configDir(configHome());
    QFile configFile(configDir + "/user-dirs.dirs");
    if (!configFile.exists())
        return fallback;

    if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return fallback;

    QString userDirVar("XDG_" + folderName.toUpper() + "_DIR");
    QTextStream in(&configFile);
    QString line;
    while (!in.atEnd())
    {
        line = in.readLine();
        if (line.contains(userDirVar))
        {
            configFile.close();

            // get path between quotes
            line = line.section(QLatin1Char('"'), 1, 1);
            line.replace(QLatin1String("$HOME"), QLatin1String("~"));
            fixBashShortcuts(line);
            return line;
        }
    }

    configFile.close();
    return fallback;
}


/************************************************

 ************************************************/
bool XdgDirs::setUserDir(XdgDirs::UserDirectory dir, const QString& value, bool createDir)
{
    // possible values for UserDirectory
    if (dir < 0 || dir > 7)
        return false;

    if (!(value.startsWith(QLatin1String("$HOME"))
                           || value.startsWith(QLatin1String("~/"))
                           || value.startsWith(QString(getenv("HOME")))))
        return false;

    QString folderName = userDirectoryString[dir];

    QString configDir(configHome());
    QFile configFile(configDir % QLatin1String("/user-dirs.dirs"));

    // create the file if doesn't exist and opens it
    if (!configFile.open(QIODevice::ReadWrite | QIODevice::Text))
        return false;

    QTextStream stream(&configFile);
    QVector<QString> lines;
    QString line;
    bool foundVar = false;
    while (!stream.atEnd())
    {
        line = stream.readLine();
        if (line.indexOf(QLatin1String("XDG_") + folderName.toUpper() + QLatin1String("_DIR")) == 0)
        {
            foundVar = true;
            QString path = line.section(QLatin1Char('"'), 1, 1);
            line.replace(path, value);
            lines.append(line);
        }
        else if (line.indexOf(QLatin1String("XDG_")) == 0)
        {
            lines.append(line);
        }
    }

    stream.reset();
    configFile.resize(0);
    if (!foundVar)
        stream << QString("XDG_%1_DIR=\"%2\"\n").arg(folderName.toUpper()).arg(value);

    for (QVector<QString>::iterator i = lines.begin(); i != lines.end(); ++i)
        stream << *i << "\n";

    configFile.close();

    if (createDir) {
        QString path = QString(value).replace(QLatin1String("$HOME"), QLatin1String("~"));
        fixBashShortcuts(path);
        QDir().mkpath(path);
    }

    return true;
}


/************************************************

 ************************************************/
QString XdgDirs::dataHome(bool createDir)
{
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    QString s = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    fixBashShortcuts(s);
    if (createDir)
        return createDirectory(s);

   removeEndingSlash(s);
   return s;
#else
    return xdgSingleDir("XDG_DATA_HOME", QLatin1String(".local/share"), createDir);
#endif
}


/************************************************

 ************************************************/
QString XdgDirs::configHome(bool createDir)
{
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    QString s = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    fixBashShortcuts(s);
    if (createDir)
        return createDirectory(s);

   removeEndingSlash(s);
   return s;
#else
    return xdgSingleDir("XDG_CONFIG_HOME", QLatin1String(".config"), createDir);
#endif
}


/************************************************

 ************************************************/
QStringList XdgDirs::dataDirs(const QString &postfix)
{
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    QString d = QFile::decodeName(qgetenv("XDG_DATA_DIRS"));
    QStringList dirs = d.split(QLatin1Char(':'), QString::SkipEmptyParts);

    QMutableListIterator<QString> it(dirs);
    while (it.hasNext()) {
        const QString dir = it.next();
        if (!dir.startsWith(QLatin1Char('/')))
            it.remove();
    }

    dirs.removeDuplicates();
    cleanAndAddPostfix(dirs, postfix);
    return dirs;
#else
    QStringList dirs = xdgDirList("XDG_DATA_DIRS", postfix);
    if (dirs.isEmpty())
    {
        dirs << QLatin1String("/usr/local/share") % postfix;
        dirs << QLatin1String("/usr/share") % postfix;
    }

    return dirs;
#endif

}


/************************************************

 ************************************************/
QStringList XdgDirs::configDirs(const QString &postfix)
{
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    QStringList dirs;
    const QString env = QFile::decodeName(qgetenv("XDG_CONFIG_DIRS"));
    if (env.isEmpty())
        dirs.append(QString::fromLatin1("/etc/xdg"));
    else
        dirs = env.split(QLatin1Char(':'), QString::SkipEmptyParts);

    cleanAndAddPostfix(dirs, postfix);
    return dirs;
#else
    QStringList dirs = xdgDirList("XDG_CONFIG_DIRS", postfix);
    if (dirs.isEmpty())
    {
        dirs << QLatin1String("/etc/xdg") % postfix;
    }

    return dirs;
#endif
}


/************************************************

 ************************************************/
QString XdgDirs::cacheHome(bool createDir)
{
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    QString s = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    fixBashShortcuts(s);
    if (createDir)
        return createDirectory(s);

    removeEndingSlash(s);
    return s;
#else
    return xdgSingleDir("XDG_CACHE_HOME", QLatin1String(".cache"), createDir);
#endif

}


/************************************************

 ************************************************/
QString XdgDirs::runtimeDir()
{
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    QString result = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    fixBashShortcuts(result);
    removeEndingSlash(result);
    return result;
#else
    QString result(getenv("XDG_RUNTIME_DIR"));
    fixBashShortcuts(result);
    return result;
#endif
}


/************************************************

 ************************************************/
QString XdgDirs::autostartHome(bool createDir)
{
    QString s = QString("%1/autostart").arg(configHome(createDir));
    fixBashShortcuts(s);

    if (createDir)
        return createDirectory(s);

     QDir d(s);
     QString r = d.absolutePath();
     removeEndingSlash(r);
     return r;
}


/************************************************

 ************************************************/
QStringList XdgDirs::autostartDirs(const QString &postfix)
{
    QStringList dirs;
    QStringList s = configDirs();
    foreach(QString dir, s)
        dirs << QString("%1/autostart").arg(dir) + postfix;

    return dirs;
}
