#include <QCoreApplication>
#include <QString>
#include <QChar>
#include <QDebug>

bool containsChineseCharacters(const QString &path) {
    // 检查字符串是否包含中文字符
    // 中文字符的Unicode范围：0x4E00-0x9FFF (基本汉字)
    // 以及一些扩展的中文字符范围
    for (const QChar &ch : path) {
        ushort unicode = ch.unicode();
        // 基本汉字范围
        if (unicode >= 0x4E00 && unicode <= 0x9FFF) {
            return true;
        }
        // 扩展汉字范围A
        if (unicode >= 0x3400 && unicode <= 0x4DBF) {
            return true;
        }
        // 扩展汉字范围B
        if (unicode >= 0x20000 && unicode <= 0x2A6DF) {
            return true;
        }
        // 扩展汉字范围C
        if (unicode >= 0x2A700 && unicode <= 0x2B73F) {
            return true;
        }
        // 扩展汉字范围D
        if (unicode >= 0x2B740 && unicode <= 0x2B81F) {
            return true;
        }
        // 扩展汉字范围E
        if (unicode >= 0x2B820 && unicode <= 0x2CEAF) {
            return true;
        }
        // 扩展汉字范围F
        if (unicode >= 0x2CEB0 && unicode <= 0x2EBEF) {
            return true;
        }
        // 扩展汉字范围G
        if (unicode >= 0x30000 && unicode <= 0x3134F) {
            return true;
        }
    }
    return false;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    
    // 测试用例
    QStringList testPaths = {
        "C:\\Projects\\MMNET",                    // 纯英文路径
        "D:\\Work\\MMNET",                        // 纯英文路径
        "C:\\Users\\张三\\Documents\\MMNET",      // 包含中文路径
        "D:\\我的项目\\MMNET",                    // 包含中文路径
        "C:\\Projects\\测试\\MMNET",              // 包含中文路径
        "E:\\Work\\MMNET_Project",                // 纯英文路径
        "F:\\我的工作\\MMNET项目",                // 包含中文路径
        "C:\\Projects\\MMNET\\data\\gene",        // 纯英文路径
        "D:\\工作\\MMNET\\data\\phen",            // 包含中文路径
    };
    
    qDebug() << "中文路径检测测试结果：";
    qDebug() << "================================";
    
    for (const QString &path : testPaths) {
        bool hasChinese = containsChineseCharacters(path);
        qDebug() << "路径:" << path;
        qDebug() << "包含中文:" << (hasChinese ? "是" : "否");
        qDebug() << "---";
    }
    
    return 0;
} 