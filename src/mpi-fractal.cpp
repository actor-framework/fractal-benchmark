#include <map>
#include <limits>
#include <string>
#include <chrono>
#include <cstring>
#include <iostream>
#include <type_traits>

#include "caf/config.hpp"

CAF_PUSH_WARNINGS

#include <boost/mpi.hpp>
#include <boost/serialization/string.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <QFile>
#include <QBuffer>
#include <QByteArray>

CAF_POP_WARNINGS

#include "config.hpp"
#include "fractal_request.hpp"
#include "calculate_fractal.hpp"
#include "fractal_request_stream.hpp"

using std::cout;
using std::endl;

#define VERBOSE(unused) static_cast<void>(0)

//#define VERBOSE(output) cout << ouptut << endl;

namespace boost {
namespace serialization {

template <class Archive>
typename ::std::enable_if<Archive::is_saving::value>::type
serialize(Archive& ar, QByteArray& arr, const unsigned) {
  VERBOSE("serialize QByteArray");
  auto count = static_cast<uint32_t>(arr.size());
  ar << count;
  ar.save_binary(arr.constData(), count);
}

template <class Archive>
typename ::std::enable_if<Archive::is_loading::value>::type
serialize(Archive& ar, QByteArray& arr, const unsigned) {
  VERBOSE("deserialize QByteArray");
  uint32_t count;
  ar >> count;
  arr.resize(count);
  ar.load_binary(arr.data(), count);
}

} // namespace serialization
} // namespace boost

namespace mpi = boost::mpi;

struct job_msg {

  uint32_t width;
  uint32_t height;
  float_type min_re;
  float_type max_re;
  float_type min_im;
  float_type max_im;
  uint32_t iterations;
  int32_t image_id;

  job_msg() = default;

  job_msg(const fractal_request& fr, uint32_t iters, int32_t id)
      : width(::width(fr)),
        height(::height(fr)),
        min_re(::min_re(fr)),
        max_re(::max_re(fr)),
        min_im(::min_im(fr)),
        max_im(::max_im(fr)),
        iterations(iters),
        image_id(id) {
    // nop
  }

  template <class Archive>
  void serialize(Archive& ar, const unsigned int) {
    mpi::communicator world;
    ar & width;
    ar & height;
    ar & min_re;
    ar & max_re;
    ar & min_im;
    ar & max_im;
    ar & iterations;
    ar & image_id;
  }
};

BOOST_IS_MPI_DATATYPE(job_msg)
BOOST_IS_BITWISE_SERIALIZABLE(job_msg)
BOOST_CLASS_TRACKING(job_msg, track_never)

BOOST_CLASS_TRACKING(QByteArray, track_never)

using namespace std;

typedef int rank_type;
typedef int tag_type;

namespace {

const tag_type done_tag = std::numeric_limits<tag_type>::max();
const size_t max_pending_worker_sends = 3;
const size_t max_pending_tasks_per_worker = 3;

} // namespace <anonymous>

void worker(mpi::communicator& world) {
  job_msg msg;
  vector<QColor> palette;
  size_t pending_sends = 0;
  std::array<mpi::request, max_pending_worker_sends> send_reqs;
  QByteArray ba;
  for (;;) {
    auto st = world.probe();
    if (st.tag() == done_tag) {
      // make sure all messages are transmitted
      if (pending_sends > 0) {
        auto first = send_reqs.begin();
        auto last = first + pending_sends;
        mpi::wait_all(first, last);
        VERBOSE("### " << pending_sends << " sents completed @" << world.rank());
      }
      return;
    }
    // wait for pending sends to complete if reached max. pending sends
    if (pending_sends == max_pending_worker_sends) {
      auto first = send_reqs.begin();
      auto last = first + pending_sends;
      auto pos = mpi::wait_some(first, last);
      VERBOSE("### " << std::distance(pos, last) << " sents completed @" << world.rank());
      pending_sends = std::distance(first, pos);
    }
    world.recv(0, st.tag(), msg);
    ba.clear();
    calculate_mandelbrot(ba, palette, msg.width, msg.height,
                         msg.iterations, msg.min_re, msg.max_re,
                         msg.min_im, msg.max_im, false);
    send_reqs[pending_sends++] = world.isend(0, st.tag(), ba);
  }
}

struct mandelbrot_task {
  int worker;
  job_msg out;
  QByteArray in;
  mpi::request out_req;
  //mpi::request in_req;
};

void client(mpi::communicator& world) {
  tag_type id = 0;
  size_t received_images = 0;
  size_t sent_images = 0;
  fractal_request_stream frs;
  map<tag_type, mandelbrot_task> tasks;
  vector<mpi::request> in_requests;
  frs.init(default_width, default_height, default_min_real, default_max_real,
           default_min_imag, default_max_imag, default_zoom);
  auto send_task = [&](int worker) {
    if (!frs.next()) {
      cerr << "*** frs.next() failed ***" << endl;
      abort();
    }
    if (sent_images == max_images) {
      return;
    }
    ++sent_images;
    auto tag = ++id;
    VERBOSE("--> task " << tag << " to worker " << worker);
    auto& task = tasks[tag];
    task.worker = worker;
    task.out = job_msg{frs.request(), default_iterations, tag};
    task.out_req = world.isend(worker, tag, task.out);
    //task.in_req = world.irecv(worker, tag, task.in);
    //in_requests.push_back(task.in_req);
    in_requests.push_back(world.irecv(worker, tag, task.in));
  };
  // distribute tasks (initially 3 per worker)
  for (size_t j = 0; j < max_pending_tasks_per_worker; ++j) {
    for (int i = 1; i < world.size(); ++i) {
      send_task(i);
    }
  }
  vector<mpi::status> statuses;
  while (received_images < max_images) {
    auto ipair = mpi::wait_some(in_requests.begin(), in_requests.end(),
                                std::back_inserter(statuses));
    in_requests.erase(ipair.second, in_requests.end());
    for (auto s : statuses) {
      VERBOSE("<-- task with tag " << s.tag() << " completed");
      auto i = tasks.find(s.tag());
      if (i == tasks.end()) {
        cerr << "*** received message with unexpected tag ***" << endl;
        return;
      }
      ++received_images;
      auto worker = i->second.worker;
      // remove task from cache
      tasks.erase(i);
      // send next task
      send_task(worker);
    }
    statuses.clear();
  }
  vector<mpi::request> done_requests;
  for (int i = 1; i < world.size(); ++i) {
    done_requests.push_back(world.isend(i, done_tag));
  }
  mpi::wait_all(done_requests.begin(), done_requests.end());
}

int main(int argc, char** argv) {
  mpi::environment env(argc, argv);
  mpi::communicator world;
  if (world.rank() == 0) {
    auto t1 = std::chrono::high_resolution_clock::now();
    client(world);
    auto t2 = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    cout << diff << endl;
  } else {
    worker(world);
  }
  return 0;
}
