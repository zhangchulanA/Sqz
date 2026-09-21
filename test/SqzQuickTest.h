#ifndef SQZQUICKTEST_H
#define SQZQUICKTEST_H

#include <QObject>
#include <SqzQuick.h>

using namespace Sqz;
class SqzQuickTest : public SqzQuick
{
    Q_OBJECT
public:
    explicit SqzQuickTest(QObject *parent = nullptr);

public slots:
    void QuitApp();

signals:

};

#endif // SQZQUICKTEST_H
