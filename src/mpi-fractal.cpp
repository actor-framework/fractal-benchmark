#include <map>
#include <limits>
#include <string>
#include <cstring>
#include <iostream>
#include <type_traits>

#include <boost/mpi.hpp>
#include <boost/serialization/string.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <QFile>
#include <QBuffer>
#include <QByteArray>

#include "config.hpp"
#include "fractal_request.hpp"
#include "calculate_fractal.hpp"
#include "fractal_request_stream.hpp"

namespace boost {
namespace serialization {

template<class Archive>
typename ::std::enable_if<Archive::is_saving::value>::type
serialize(Archive& ar, QByteArray& arr, const unsigned) {
    auto count = static_cast<uint32_t>(arr.size());
    ar << count;
    ar.save_binary(arr.constData(), count);
}

template<class Archive>
typename ::std::enable_if<Archive::is_loading::value>::type
serialize(Archive& ar, QByteArray& arr, const unsigned) {
    uint32_t count;
    ar >> count;
    arr.resize(count);
    ar.load_binary(arr.data(), count);
}

} // namespace serialization
} // namespace boost

struct job_msg {

    uint32_t   width;
    uint32_t   height;
    float_type min_re;
    float_type max_re;
    float_type min_im;
    float_type max_im;
    uint32_t   iterations;
    int   image_id;

    job_msg() = default;

    job_msg(const fractal_request& fr, uint32_t iters, int id)
    : width(::width(fr)), height(::height(fr))
    , min_re(::min_re(fr)), max_re(::max_re(fr))
    , min_im(::min_im(fr)), max_im(::max_im(fr))
    , iterations(iters), image_id(id) { }

    template<class Archive>
    void serialize(Archive & ar, const unsigned int)
    {
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

namespace boost { namespace mpi {
  template <>
  struct is_mpi_datatype<job_msg> : mpl::true_ { };
} }

BOOST_IS_BITWISE_SERIALIZABLE(job_msg)

using namespace std;
namespace mpi = boost::mpi;

typedef int rank_type;
typedef int tag_type;

namespace { const tag_type done_tag = std::numeric_limits<tag_type>::max(); }

int main(int argc, char** argv) {
    mpi::environment env(argc, argv);
    mpi::communicator world;
    if (argc == 1) {
        std::cout << "usage: [number of pictures] [keep-pngs]"             << std::endl
                  << "       number of pictures as decimal (default: 778)" << std::endl
                  << "       keep-pngs as string (default: false)"         << std::endl;
        exit(-1);
    }

    bool save_imgs = false;
    int max_imgs   = 778;
    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "keep-pngs", std::strlen(argv[i])) == 0) {
            save_imgs = true;
        } else {
            try {
                max_imgs = std::stoi(argv[i]);
            } catch(std::exception&) {
                std::cout << "invalid argument: " << argv[i] << std::endl;
            }
        }
    }

    if (world.rank() == 0) {
        tag_type id = 0;
        size_t awaited = 0;
        fractal_request_stream frs;
        map<tag_type,job_msg> out;
        map<tag_type,QByteArray> in;
        vector<mpi::request> requests;
        frs.init(default_width, default_height,
                 default_min_real, default_max_real,
                 default_min_imag, default_max_imag,
                 default_zoom);
        auto send_job = [&](int worker) {
            if (frs.next()) {
                if (id >= max_imgs) {
                   // end
                   return;
                }
                ++awaited;
                auto t = ++id;
                auto r1 = out.insert(make_pair(t, job_msg{frs.request(), default_iterations, t}));
                requests.push_back(world.isend(worker, t, r1.first->second));
                auto r2 = in.insert(make_pair(t, QByteArray()));
                requests.push_back(world.irecv(worker, t, r2.first->second));
            }
        };
        // distribute initial tasks (initially 3 per worker)
        for (int i = 1; i < world.size(); ++i) {
            for (size_t j = 0; j < 3; ++j) {
                 send_job(i);
            }
        }
        vector<mpi::status> statuses;
        while (awaited > 0) {
            auto ipair = mpi::wait_some(requests.begin(), requests.end(), std::back_inserter(statuses));
            requests.erase(ipair.second, requests.end());
            for (auto s : statuses) {
                if (s.source() == 0) {
                    // outgoing message (clear buffer)
                    out.erase(s.tag());
                }
                else {
                    --awaited;
                    if (save_imgs) {
                        // store image to disc
                        auto img = QImage::fromData(in[s.tag()], image_format);
                        std::ostringstream fname;
                        fname.width(4);
                        fname.fill('0');
                        fname.setf(ios_base::right);
                        fname << s.tag() << image_file_ending;
                        QFile f{fname.str().c_str()};
                        if (!f.open(QIODevice::WriteOnly)) {
                            cerr << "could not open file: " << fname.str() << endl;
                        }
                        else img.save(&f, image_format);
                    }
                    in.erase(s.tag());
                    // enqueue next job
                    send_job(s.source());
                }
            }
            statuses.clear();
        }
        for (int i = 1; i < world.size(); ++i) world.send(i, done_tag);
    } else {
        job_msg msg;
        vector<QColor> palette;
        mpi::request req;
        QByteArray ba;
        for (;;) {
            auto st = world.probe();
            if (st.tag() == done_tag) return 0;
            world.recv(0, st.tag(), msg);
            auto img = calculate_mandelbrot(palette, msg.width, msg.height, msg.iterations,
                                            msg.min_re, msg.max_re, msg.min_im, msg.max_im, false);
            req.wait(); // wait for last send to finish
            ba.clear();
            if (save_imgs) {
              QBuffer buf{&ba};
              buf.open(QIODevice::WriteOnly);
              img.save(&buf, image_format);
              buf.close();
            }
            req = world.isend(0, st.tag(), ba);
        }
    }
}
