//#include <chrono>
#include <vector>
#include <iostream>

#include <QFile>
#include <QBuffer>
#include <QByteArray>

#include "fractal_request.hpp"
#include "calculate_fractal.hpp"
#include "fractal_request_stream.hpp"

#include "charm++.h"
#include "charm_fractal.decl.h"

using namespace std;

class QByteArray_serialized : public CBase_QByteArray_serialized {
public:
  QByteArray_serialized() = default;
  QByteArray_serialized(const QByteArray& ba) : m_ba(ba), m_size(ba.size()) {
    // nop
  }
  QByteArray_serialized(CkMigrateMessage* m) {
    // nop
  }

  void pup(PUP::er &p) {
    p | m_size;
    if (p.isUnpacking())
      m_ba.resize(m_size);
    PUParray(p, m_ba.data(), m_size);
  }

private:
  int m_size;
  QByteArray m_ba;
};

class worker : public CBase_worker {
public:

  void calculate(uint32_t width, uint32_t height, float_type min_re,
                 float_type max_re, float_type min_im, float_type max_im,
                 uint32_t iterations) {
      QByteArray ba;
      calculate_mandelbrot(ba, m_palette, width, height, iterations,
                           min_re, max_re, min_im, max_im, false);
      cout << "ba.size = " << ba.size() << endl;
      m_master.deliver_byte_array(QByteArray_serialized{ba}, thisProxy);
  }

private:
  vector<QColor> m_palette;
  CProxy_client  m_master;
};

class client : public CBase_client {
 public:
  client(uint32_t workers) : m_sent_images(0), m_received_images(0) {
    m_frs.init(default_width, default_height, default_min_real,
               default_max_real, default_min_imag, default_max_imag,
               default_zoom);
    for (int i = 0; i < workers; ++i) {
      CProxy_worker w = CProxy_worker::ckNew();
      initial_tasks(w);
    }
  }

  void initial_tasks(CProxy_worker& w) {
      for (int i = 0; i < 3; ++i) {
        send_job(w);
      }
  }

  void deliver_byte_array(QByteArray_serialized& ba, CProxy_worker& w) {
#ifdef WRITE_IMAGES
    cout << "ba.size = " << ba.size() << endl;
    QFile file(QString("fractal-%1.png").arg(m_received_images));
    file.open(QIODevice::WriteOnly);
    file.write(ba);
    file.close();
#endif
    if (++m_received_images == max_images) {
      CkExit();
    } else {
      send_job(w);
    }
    static_cast<void>(ba); // avoid unused argument warning
  }

private:
  void send_job(CProxy_worker& worker) {
    if (m_sent_images == max_images) {
      return;
    }
    if (!m_frs.next()) {
      cerr << "*** frs.next() failed ***" << endl;
      abort();
    }
    ++m_sent_images;
    auto fr = m_frs.request();
    cout << "send(worker, " << width(fr) << ", " << height(fr) << ", "
         << min_re(fr) << ", " << max_re(fr) << ", " << min_im(fr) << ","
         << max_im(fr) << ", " << default_iterations << ");"
         << endl;
    worker.calculate(width(fr), height(fr), min_re(fr), max_re(fr), min_im(fr),
         max_im(fr), default_iterations);
  }

  uint32_t m_sent_images;
  uint32_t m_received_images;
  fractal_request_stream m_frs;
};

class main : public CBase_main {
public:
  main(CkArgMsg* m) {
    CProxy_client client = CProxy_client::ckNew(4);
    //auto t1 = chrono::high_resolution_clock::now();
    delete m;
    //auto t2 = chrono::high_resolution_clock::now();
    //auto diff = chrono::duration_cast<chrono::milliseconds>(t2 - t1).count();
    //cout << diff << endl;
  }
};

#include "charm_fractal.def.h"
