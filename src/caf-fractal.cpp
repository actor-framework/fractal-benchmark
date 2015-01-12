#include <map>
#include <limits>
#include <string>
#include <cstring>
#include <iostream>
#include <type_traits>

#include <QFile>
#include <QBuffer>
#include <QByteArray>

#include "caf/all.hpp"
#include "cppa/cppa.hpp"
#include "caf/io/all.hpp"

#include "config.hpp"
#include "fractal_request.hpp"
#include "q_byte_array_info.hpp"
#include "calculate_fractal.hpp"
#include "fractal_request_stream.hpp"

using namespace std;
using namespace caf;

std::vector<std::string> split(const std::string& str, char delim,
                               bool keep_empties = false) {
  using namespace std;
  vector<string> result;
  stringstream strs{str};
  string tmp;
  while (getline(strs, tmp, delim)) {
    if (!tmp.empty() || keep_empties)
      result.push_back(std::move(tmp));
  }
  return result;
}

class worker : public event_based_actor {
 public:
  behavior make_behavior() {
    return {
      [=](uint32_t width, uint32_t height, float_type min_re,
         float_type max_re, float_type min_im, float_type max_im,
         uint32_t iterations) {
        auto img = calculate_mandelbrot(m_palette, width, height, iterations,
                                        min_re, max_re, min_im, max_im, false);
        QByteArray ba;
        QBuffer buf{&ba};
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, image_format);
        buf.close();
        return make_message(ba, this);
      }
    };
  }

 private:
  vector<QColor> m_palette;
};

class client : public event_based_actor {
 public:
  client() : m_sent_images(0), m_received_images(0) {
    m_frs.init(default_width, default_height, default_min_real,
               default_max_real, default_min_imag, default_max_imag,
               default_zoom);
  }

  behavior make_behavior() {
    return {
      [=](const actor& new_worker) {
        monitor(new_worker);
        // distribute initial tasks (initially 3 per worker)
        for (int i = 0; i < 3; ++i) {
          send_job(new_worker);
        }
      },
      [=](const down_msg&) {
        cerr << "*** critical: worker failed ***" << endl;
        abort();
      },
      [=](const QByteArray&, const actor& worker) {
        if (++m_received_images == max_images) {
          quit();
        } else {
          send_job(worker);
        }
      }
    };
  }
 private:
  void send_job(const actor& worker) {
    if (m_sent_images == max_images) {
      return;
    }
    if (!m_frs.next()) {
      cerr << "*** frs.next() failed ***" << endl;
      abort();
    }
    ++m_sent_images;
    auto fr = m_frs.request();
    send(worker, width(fr), height(fr), min_re(fr), max_re(fr), min_im(fr),
         max_im(fr), default_iterations);
  }

  uint32_t m_sent_images;
  uint32_t m_received_images;
  fractal_request_stream m_frs;
};

int main(int argc, char** argv) {
  // announce some messaging types
  announce(typeid(QByteArray), uniform_type_info_ptr{new q_byte_array_info});
  // read command line options
  bool is_worker = false;
  uint16_t port = 20283;
  string nodes_str;
  options_description desc;
  bool args_valid = match_stream<string>(argv + 1, argv + argc) (
    // general options
    on_opt0('w', "worker", &desc, "run in worker mode", "general")     >> set_flag(is_worker),
    on_opt0('h', "help", &desc, "print this text", "general")          >> print_desc_and_exit(&desc),
    // worker options
    on_opt1('p', "port", &desc, "set port (default: 20283)", "worker") >> rd_arg(port),
    // client options
    on_opt1('n', "nodes", &desc, "set worker nodes", "client")         >> rd_arg(nodes_str)
  );
  if (!args_valid) {
    print_desc_and_exit(&desc)();
  }
  // run either as worker or client
  if (is_worker) {
    io::publish(spawn<worker>(), port);
  } else {
    auto nodes = split(nodes_str, ',');
    if (nodes.empty()) {
      cerr << "not started as worker but nodes list is empty" << endl;
      return -1;
    }
    auto c = spawn<client>();
    for (auto& node : nodes) {
      auto entry = split(node, ':');
      if (entry.size() != 2) {
        cerr << "expected node format 'HOST:PORT', found: " << node << endl;
        return -2;
      }
      try {
        auto w = remote_actor(entry[0],
                              static_cast<uint16_t>(atoi(entry[1].c_str())));
        anon_send(c, w);
      }
      catch (std::exception& e) {
        cerr << "exception while connection to " << node << endl
             << "e.what():" << e.what() << endl;
        return -2;
      }
    }
  }
  await_all_actors_done();
  shutdown();
}
