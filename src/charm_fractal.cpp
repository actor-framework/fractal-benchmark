
#include <chrono>
#include <iostream>

//#include <QFile>
//#include <QBuffer>
//#include <QByteArray>

#include "fractal_request.hpp"
#include "calculate_fractal.hpp"
#include "fractal_request_stream.hpp"

#include "charm++.h"
#include "charm_fractal.decl.h"

using namespace std;

class main : public CBase_main {
public:
  main(CkArgMsg* m) {
    auto t1 = chrono::high_resolution_clock::now();



    std::cout << "Hello world" << std::endl;
    delete m;
    CkExit();
  }
};

#include "charm_fractal.def.h"
