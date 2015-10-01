#include "erl_nif.h"

#include <QColor>

#include <cstddef>

#include "config.hpp"
#include "calculate_fractal.hpp"

namespace {

constexpr unsigned chunk_size = 10240; // 10kb chunk size

using color_vector = std::vector<QColor>;

__thread std::vector<QColor>* palette = nullptr;

class erl_binary_writer {
public:
  erl_binary_writer(ErlNifEnv* env) : env_(env), pos_(0) {
    enif_alloc_binary(chunk_size * 10, &bin_); // 100kb initial capacity
  }

  void push_back(char value) {
    if (pos_ >= bin_.size)
      enif_realloc_binary(&bin_, bin_.size + chunk_size);
    bin_.data[pos_++] = value;
  }

  ErlNifBinary* bin() {
    return &bin_;
  }

  uint32_t size() {
    return pos_;
  }

  void shrink_to_fit() {
    enif_realloc_binary(&bin_, pos_);
  }

private:
  uint32_t pos_;
  ErlNifBinary bin_;
  ErlNifEnv* env_;
};

class fractal_nif_impl {
public:
  fractal_nif_impl(ErlNifEnv* env) : env_(env) {
    // nop
  }

  ERL_NIF_TERM operator()(int argc, const ERL_NIF_TERM argv[]) {
    erl_binary_writer storage{env_};
    if (! palette)
      palette = new color_vector;
    calculate_mandelbrot(storage, *palette,
                         getu(argv[0]), getu(argv[1]), getu(argv[2]),
                         getf(argv[3]), getf(argv[4]),
                         getf(argv[5]), getf(argv[6]), getb(argv[7]));
    storage.shrink_to_fit();
    return enif_make_binary(env_, storage.bin());
  }

private:
  uint32_t getu(ERL_NIF_TERM arg) {
    uint32_t x;
    if (! enif_get_uint(env_, arg, &x))
      throw std::logic_error("not an integer");
    return x;
  }

  float getf(ERL_NIF_TERM arg) {
    double x;
    if (! enif_get_double(env_, arg, &x))
      throw std::logic_error("not a double");
    return static_cast<float>(x);
  }

  bool getb(ERL_NIF_TERM arg) {
    char buf[10];
    if (! enif_get_atom(env_, arg, buf, 10, ERL_NIF_LATIN1))
      throw std::logic_error("not a boolean");
    buf[9] = '\0';
    const char* true_str = "true";
    return strcmp(buf, true_str) == 0;
  }

  ErlNifEnv* env_;
};

int upgrade_nif(ErlNifEnv*, void**, void**, ERL_NIF_TERM) {
  return 0;
}

ERL_NIF_TERM fractal_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
  fractal_nif_impl f{env};
  return f(argc, argv);
}

ErlNifFunc nif_funcs[] = {
  {"compute", 8, fractal_nif}
};

} // namespace <anonymous>

ERL_NIF_INIT(fractal, nif_funcs, NULL, NULL, upgrade_nif, NULL)
