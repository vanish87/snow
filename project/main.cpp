/**************************************************************************
**
**   SNOW - CS224 BROWN UNIVERSITY
**
**   main.cpp
**   Author: mliberma
**   Created: 6 Apr 2014
**
**************************************************************************/


#define devMode true

#if devMode == true

#include <QApplication>
#include "ui/mainwindow.h"
#include "tests/tests.h"
#include <iostream>
/*
 *
 * Run with '-test' as an argument to run tests defined in tests.cpp. Run with no argument to run with GUI
 *
 */
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    if(argc < 2)  {
        w.show();
        return a.exec();
    }
    else if (!strcmp(argv[1],"-test"))  {
        Tests::runTests();
    }

    else  {
        printf("unknown argument %s, only support '-test' as an argument. Run with empty argument list to run with gui.",argv[1]);
    }
    return 0;
}


#else

extern "C"
void runTestsEric();

int main(int argc, char *argv[])
{
    std::cout << "hello world!" << std::endl;
}


#endif