#ifndef SINGLETON_H
#define SINGLETON_H

#include <QMutex>
#include <QMutexLocker>
#include "SqzGlobal.h"
//        Q_GLOBAL_STATIC(OfflineASR,OfflineASR)
namespace Sqz::Utils{
template <typename T>
class Singleton
{
public:
    static T* instance(){

        if(m_instance == nullptr)
        {
            QMutexLocker locker(&m_mutex);
            if(m_instance == nullptr){
                m_instance = new T();
            }
        }
        return m_instance;
    }

    static void release(){
        QMutexLocker locaker(&m_mutex);
        if(m_instance){
            delete  m_instance;
            m_instance = nullptr;
        }
    }
protected:


    Singleton() = default;
    ~Singleton() = default;
    Singleton(const Singleton&) = delete ;
    Singleton& operator=(Singleton&&) = delete;

private:
    static T * m_instance;
    static QMutex m_mutex;
};

template <typename T>
T* Singleton<T>::m_instance = nullptr;

template <typename T>
QMutex Singleton<T>:: m_mutex;
}

#endif // SINGLETON_H
