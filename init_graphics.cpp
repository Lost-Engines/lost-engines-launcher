#include <iostream>
#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include "vectorstorm/vector/vector2.h"

void init_graphics(std::string const &window_title) {
  std::cout << "Creating window..." << std::endl;

  glfwSetErrorCallback([](int error, const char* description){
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
  });
  if(!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  vec2ui constexpr window_size{600, 800};

  float const main_scale{ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor())};
  GLFWwindow *window{glfwCreateWindow(static_cast<int>(window_size.x * main_scale), static_cast<int>(window_size.y * main_scale), window_title.c_str(), nullptr, nullptr)};
  if(window == nullptr) throw std::runtime_error("Failed to create window");

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);                                                          // enable vsync

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &imgui_io{ImGui::GetIO()};
  imgui_io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;                   // enable Keyboard Controls
  imgui_io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;                    // enable Gamepad Controls

  ImGui::StyleColorsDark();

  ImGuiStyle &imgui_style{ImGui::GetStyle()};
  imgui_style.ScaleAllSizes(main_scale);                                        // bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
  imgui_style.FontScaleDpi = main_scale;                                        // set initial font scale. (in docking branch: using imgui_io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();
}
