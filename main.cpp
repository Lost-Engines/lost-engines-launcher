#include <iostream>
#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include "vectorstorm/vector/vector2.h"

void draw_window() {
  ImGuiViewport const &viewport{*ImGui::GetMainViewport()};
  ImGui::SetNextWindowPos(viewport.Pos);
  ImGui::SetNextWindowSize(viewport.Size);
  ImGuiWindowFlags window_flags{ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove};

  if(!ImGui::Begin("Lost Engines", nullptr, window_flags)) {
    ImGui::End();
    return;
  }

  ImGui::End();
}

auto main()->int {
  glfwSetErrorCallback([](int error, const char* description){
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
  });
  if(!glfwInit()) return EXIT_FAILURE;

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  vec2ui constexpr window_size{600, 800};

  float const main_scale{ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor())};
  GLFWwindow *window{glfwCreateWindow(static_cast<int>(window_size.x * main_scale), static_cast<int>(window_size.y * main_scale), "Lost Engines", nullptr, nullptr)};
  if(window == nullptr) return EXIT_FAILURE;

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
  const char *glsl_version{nullptr};
  ImGui_ImplOpenGL3_Init(glsl_version);

  while(!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    if(glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {                      // no window rendering while minimised
      ImGui_ImplGlfw_Sleep(10);
      continue;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    draw_window();

    ImGui::Render();
    vec2i display_size;
    glfwGetFramebufferSize(window, &display_size.x, &display_size.y);
    glViewport(0, 0, display_size.x, display_size.y);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return EXIT_SUCCESS;
}
