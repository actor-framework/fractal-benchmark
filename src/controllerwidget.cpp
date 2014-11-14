
#include <sstream>
#include <QString>
#include <utility>
#include <iterator>
#include <algorithm>

#include "caf/all.hpp"

#include "include/controllerwidget.hpp"

using namespace std;
using namespace caf;

ControllerWidget::ControllerWidget(QWidget* parent, Qt::WindowFlags f)
    : super(parent, f),
      m_cpu_slider(nullptr),
      m_gpu_slider(nullptr),
      m_resolution_slider(nullptr),
      m_res_current(nullptr),
      m_time_current(nullptr),
      m_drop_down_fractal_type(nullptr),
      m_resolutions{make_pair(800, 450),   make_pair(1024, 576),
                    make_pair(1280, 720),  make_pair(1680, 945),
                    make_pair(1920, 1080), make_pair(2560, 1440)} {
  set_message_handler ([=](local_actor* self) -> message_handler {
    return {
      on(atom("max_cpu"), arg_match) >> [=](size_t max_cpu) {
        aout(self) << "max_cpu = " << max_cpu << endl;
        set_cpu_max(max_cpu);
      },
      on(atom("max_gpu"), arg_match) >> [=](size_t max_gpu) {
        aout(self) << "max_gpu = " << max_gpu << endl;
        set_gpu_max(max_gpu);
      },
      on(atom("fps"), arg_match) >> [=](uint32_t fps) {
        aout(self) << "fps = " << fps << endl;
      },
      [](const exit_msg& msg) {
        cout << "[!!!] master died" << endl;
      },
      on(atom("fraclist"), arg_match) >> [=](const map<string, atom_value>& fractal_types) {
        set_fractal_types(fractal_types);
      },
      others() >> [=]{
          cout << "[!!!] controller ui received unexpected message: "
               << to_string(self->last_dequeued())
               << endl;
      }
    };
  });
  for (auto& p : m_resolutions) {
    m_res_strings.emplace_back(QString::number(p.first) + "x"
                               + QString::number(p.second));
  }
}

void ControllerWidget::initialize() {
  resolution_slider()->setRange(0,m_resolutions.size()-1);
  resolution_slider()->setValue(m_resolutions.size()-1);
  resolution_slider()->setTickInterval(1);
  resolution_slider()->setTickPosition(QSlider::TicksBelow);
  cpu_slider()->setTickInterval(1);
  cpu_slider()->setTickPosition(QSlider::TicksBelow);
  gpu_slider()->setTickInterval(1);
  gpu_slider()->setTickPosition(QSlider::TicksBelow);
}


void ControllerWidget::adjustGPULimit(int newLimit) {
  if(m_controller) {
    send_as(as_actor(), m_controller, atom("limit"), atom("opencl"), static_cast<uint32_t>(newLimit));
  }
}

void ControllerWidget::adjustCPULimit(int newLimit) {
  if(m_controller) {
    send_as(as_actor(), m_controller, atom("limit"), atom("normal"), static_cast<uint32_t>(newLimit));
  }
}

void ControllerWidget::adjustResolution(int idx) {
  if(m_controller) {
    send_as(as_actor(), m_controller, atom("resize"), m_resolutions[idx].first,
            m_resolutions[idx].second);
    res_current()->setText(m_res_strings[idx]);
  }
}

void ControllerWidget::adjustFractals(const QString& fractal) {
  if(m_controller) {
    send_as(as_actor(), m_controller, atom("changefrac"),
            m_valid_fractal[fractal.toStdString()]);
  }
}
