#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QJsonDocument>
#include "ProtocolSchema.h"
#include "SqzApplication.h"

using namespace Sqz;
std::tuple<bool, int, int, int, int> getDMSFromLongitude(const double &longitude) {
    /**
     * @brief 东西经标记，true为东经
     */
    const bool flag = longitude >= 0;
    /**
     * @brief 用于计算的经度对象
     */
    double lon = qAbs(longitude);
    /**
     * @brief 度
     */
    int degree = lon * 1000000;
    if ((degree < 0) || (degree >= 180000000)) {
        // 度超出范围则清空数据
        degree = 0;
        lon  = 0.0;
    } else {
        degree = (int)lon;
    }
    /**
     * @brief 去掉度后的经度信息
     */
    const double minorg = lon - degree;
    /**
     * @brief 度
     */
    int minute = minorg * 60.0;
    if ((minute < 0) || (minute >= 60)) {
        // 度超出范围的清空
        minute = 0;
    }
    /**
     * @brief 去掉度分后的秒
     */
    const double secorg = (minorg * 60.0 - minute) * 60.0;
    /**
     * @brief 秒信息
     */
    int second = secorg;
    if ((second < 0) || (second >= 60)) {
        // 超出范围则清空数据
        second = 0;
    }
    /**
     * @brief 剩余的小数部分
     */
    const double ddSec = secorg - second;
    /**
     * @brief 小数秒
     */
    int dSec = ddSec * 10000.0;
    // 处理系统误差
    int nTemp = (int)(ddSec * 100000.0);
    if ((nTemp % 10) >= 5) {
        dSec ++;
    }
    if (dSec > 9999) {
        dSec = 0;
        second ++;
    }
    if (second >= 60) {
        second = second % 60;
        minute ++;
    }
    if (minute >= 60) {
        minute = minute % 60;
        degree ++;
    }
    if (degree >= 180) {
        degree = 0;
        minute = 0;
        second = 0;
        dSec   = 0;
    }

    logdebug << "==================================";
    logdebug <<"输入原始lon"<<longitude;
    logdebug <<"lon(abs)后"<<lon;
    logdebug <<"degree（度）"<<degree;
    logdebug <<"monorg（lon-degree）"<<minorg;
    logdebug <<"minute(分)"<<minute;
    logdebug <<"secorg"<<secorg;
    logdebug <<"secorg(秒整数)"<<second;
    logdebug <<"ddSec（秒小数原始double）"<<ddSec;
    logdebug <<"ddSec*10000"<<ddSec*10000.0;
    logdebug <<"dSec（计算后）"<<dSec;
    logdebug <<"最终输出元组 flag:"<<flag<<"deg:"<<degree<<"min:"<<minute<<"sec:"<<second<<"dSec:"<<dSec;;

    return std::tuple {flag, degree, minute, second, dSec*10};
}

QString lon2String(const double &lon) {
    const auto [sign, d, m, s, ds] = getDMSFromLongitude(lon);
    return QString("%1%2°%3'%4.%5\"")
           .arg(sign ? "E" : "W")
           .arg(d, 1)
           .arg(m, 2)
           .arg(s, 2)
           .arg(ds, 4);
}




int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    Logger::instance().init(".","log");
    SqzApplication sq;
//    sq.Init();
//    sq.LogRegClass();
QString ch = "中华";
QByteArray utf8 = ch.toUtf8();
logdebug << utf8.size();


char buf[601];
const int bufsize = sizeof (buf);
logdebug << bufsize;

QString testText;int maxCount = 0;
for(int i=0;i<200;++i){
    testText.append("测");
    QByteArray temp = testText.toUtf8();
    if(temp.size() >= bufsize){
        break;
    }
    maxCount++;
}
logdebug << maxCount;
logdebug <<testText.toUtf8().size();


//auto ot = getDMSFromLongitude(116.40362983333333);
//logdebug <<std::get<1>(ot)<<std::get<2>(ot)<<std::get<3>(ot)<<std::get<4>(ot);

//logdebug<<lon2String(116.40362983333333);
//    SqzApp->OpenView("SqzTest");

    return app.exec();
}
