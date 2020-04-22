#include <QCoreApplication>

#include "ImageRsc.h"
#include "TextureProvider.h"

#include <QString>

#include <iostream>
using namespace std;

#include <thread>
#include <unistd.h>

#include <QVector>

int main(int argc, char* argv[])
{
    QCoreApplication a(argc, argv);

    //    QVector<char *> d;

    //    d.push_back("sd");

    //    if(d[1] != nullptr) {
    //        cout << "not null" << endl;
    //    } else {
    //        cout << "null" << endl;
    //    }

    std::cout << "!!" << std::endl;

    //    TextureProvider::CreateInstance("image.rsc", std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    //    TextureProvider::CreateInstance("image.rsc", 1186139614, 0);
    TextureProvider::CreateInstance("/home/ldg/Project/temp/TEST_TEX_PROVIDER/image.rsc", 1186139614, 0);

    cout << ::size_table << "!" << endl;

    for (int i = 0; i < ::size_table / 5; i++) {
        cout << i << endl;
        //        if(i % 100 == 0)
        //            sleep(1);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if (QString(::tableImg[i].nameImage).split(".").last() == "pkm") {
            if (TextureProvider::instance().requestTexture(::tableImg[i].nameImage, nullptr, {})) {
                cout << "success" << endl;
            } else {
                cout << "fail" << endl;
            }

        } else if (QString(::tableImg[i].nameImage).split(".").last() == "png") {
            if (TextureProvider::instance().requestTexture(::tableImg[i].nameImage, nullptr, {})) {
                cout << "success" << endl;
            } else {
                cout << "fail" << endl;
            }
        } else {
        }
    }

    cout << "end.." << endl;

    while (1) {
    }

    return a.exec();
}
