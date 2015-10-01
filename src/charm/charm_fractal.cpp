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

#define WRITE_IMAGES

using namespace std;

void operator|(PUP::er& p, QByteArray& c) {
  int32_t size = c.size();
  p | size;
  if (p.isUnpacking()) {
    c.resize(size);
    for (auto i = 0; i < size; ++i) {
      char value;
      p | value;
      c[i] = value;
    }
  } else {
    for (auto i = 0; i < size; ++i) {
      char value = c.at(i);
      p | value;
    }
  }
}

class MyFixedSizeMsg : public CMessage_MyFixedSizeMsg {
public:
  QByteArray data;
  size_t worker_id;
  MyFixedSizeMsg(QByteArray x0, size_t x1) : data(x0), worker_id(x1) {
    // nop
  }
};


class worker : public CBase_worker {
public:
  worker() {
    // nop
  }

  void calculate(const CkCallback& cb, size_t my_id, uint32_t width, uint32_t height,
                 float_type min_re, float_type max_re, float_type min_im,
                 float_type max_im, uint32_t iterations) {
    QByteArray ba;
    calculate_mandelbrot(ba, m_palette, width, height, iterations, min_re,
                         max_re, min_im, max_im, false);
    cb.send(new MyFixedSizeMsg(ba, my_id));
  }

private:
  vector<QColor> m_palette;
};

class client : public CBase_client {
 public:
  using tstamp = decltype(std::chrono::high_resolution_clock::now());

  client() : m_sent_images(0), m_received_images(0) {
    m_frs.init(default_width, default_height, default_min_real,
               default_max_real, default_min_imag, default_max_imag,
               default_zoom);
    QByteArray dummy1;
    size_t dummy2;
    deliver_cb_ = CkIndex_client::deliver(dummy1, dummy2);
  }

  void run(std::vector<CProxy_worker> workers) {
    init_time_ = std::chrono::high_resolution_clock::now();
    workers_ = std::move(workers);
    for (size_t wid = 0; wid < workers_.size(); ++wid)
      for (int i = 0; i < 3; ++i)
        send_job(wid);
  }

  void deliver(QByteArray ba, size_t worker_id) {
#ifdef WRITE_IMAGES
    cout << "ba.size = " << ba.size() << endl;
    QFile file(QString("fractal-%1.png").arg(m_received_images));
    file.open(QIODevice::WriteOnly);
    file.write(ba);
    file.close();
#endif
    if (++m_received_images == max_images) {
      using namespace std::chrono;
      auto t2 = high_resolution_clock::now();
      auto diff = duration_cast<chrono::milliseconds>(t2 - init_time_).count();
      cout << diff << endl;

      CkExit();
    } else {
      send_job(worker_id);
    }
    static_cast<void>(ba); // avoid unused argument warning
  }

private:
  void send_job(size_t wid) {
    if (m_sent_images == max_images)
      return;
    if (! m_frs.next()) {
      cerr << "*** frs.next() failed ***" << endl;
      abort();
    }
    ++m_sent_images;
    auto fr = m_frs.request();
    workers_[wid].calculate(CkCallback(deliver_cb_, thisProxy), wid,
                            width(fr), height(fr), min_re(fr), max_re(fr),
                            min_im(fr), max_im(fr), default_iterations);
  }

  tstamp init_time_;
  int deliver_cb_;
  uint32_t m_sent_images;
  uint32_t m_received_images;
  fractal_request_stream m_frs;
  std::vector<CProxy_worker> workers_;
};

class master : public CBase_master {
public:
  master(CkArgMsg* m) {
    delete m;
    std::vector<CProxy_worker> workers;
    for (auto i = 0; i < CkNumPes(); ++i)
      if (i != CkMyPe())
        workers.push_back(CProxy_worker::ckNew(i));
    CProxy_client client = CProxy_client::ckNew(CkMyPe());
    client.run(std::move(workers));
  }
};

#include "charm_fractal.def.h"
