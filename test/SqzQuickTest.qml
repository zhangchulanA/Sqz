import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12  // 必须新增这一行
Window {
    width: 800
    height: 500
    visible: true
    title: "测试窗口"

    Button {
        text: "退出程序"
        width: 120
        height: 40
        anchors.centerIn: parent
        onClicked: {
            This.QuitApp()
        }
    }
}
