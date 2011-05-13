#include <QtCore/QCoreApplication>

#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QtCore/QSettings>

/**
  @brief The tulip application is a startup process meant to be used as a underleying controller for the tulip_app process.
  This application performs the following operations:
  @list
  @li Check for plugins waiting to be installed.
  @li Start the tulip_app process
  @list
  This process is meant to work as a daemon, meaning that no visual element should be persistent the user's desktop environement.
*/
int main(int argc,char **argv) {
  QCoreApplication tulip(argc,argv);

  // Check for elements to be installed

  // Build up the tulip_app process
  QProcess tulip_app;
  // Setup environment
  QProcessEnvironment env(QProcessEnvironment::systemEnvironment());
  QString basePath(QCoreApplication::applicationDirPath() + "/../");
#if defined(__APPLE__)
  env.insert("DYLD_LIBRARY_PATH", QDir::toNativeSeparators(basePath) + "/Frameworks" + ":" + env.value("DYLD_LIBRARY_PATH"));
#elif defined(__linux__)
  env.insert("LD_LIBRARY_PATH",QDir::toNativeSeparators(basePath) + "/lib" + ":" + env.value("LD_LIBRARY_PATH"));
#endif

  qWarning(env.value("LD_LIBRARY_PATH").toStdString().c_str());

  tulip_app.setReadChannelMode(QProcess::ForwardedChannels);
  bool restart=true;

  QStringList arguments;
  for (int i=1;i<argc;++i)
    arguments << argv[i];

  while (restart) {
    QSettings settings("TulipSoftware","Tulip");
    settings.remove("app/need_restart");
    settings.sync();

    tulip_app.setProcessEnvironment(env);
    tulip_app.start(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("tulip_app4"),arguments);
    tulip_app.waitForFinished(-1);

    settings.sync();
    restart = settings.value("app/need_restart",false).toBool();
  }

  return 0;
}
