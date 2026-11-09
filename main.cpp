#include <iostream>
#include <thread>
#include <boost/property_tree/ini_parser.hpp>
#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include "vectorstorm/vector/vector2.h"
#include "init_graphics.h"
#include "launcher_state.h"
#include "load_config.h"

void draw_window(launcher_state &state) {
  ImGuiViewport const &viewport{*ImGui::GetMainViewport()};
  ImGui::SetNextWindowPos(viewport.Pos);
  ImGui::SetNextWindowSize(viewport.Size);
  ImGuiWindowFlags window_flags{ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove};

  if(!ImGui::Begin("Lost Engines", nullptr, window_flags)) {
    ImGui::End();
    return;
  }

  switch(state.launcher_update_state) {
  case launcher_state::update_state::not_tried:
    // TODO: launch update check
    break;
  case launcher_state::update_state::loading:
    ImGui::TextUnformatted("Checking for launcher updates...");
    break;
  case launcher_state::update_state::failed_to_connect:
    ImGui::TextUnformatted("Failed to connect to Github, updates and downloads unavailable");
    break;
  case launcher_state::update_state::outdated:
    ImGui::TextUnformatted("Launcher update available");
    // TODO: pop up a window saying there's an update available, update now?  Always update?
    break;
  case launcher_state::update_state::downloading:
    ImGui::TextUnformatted("Downloading launcher update...");
    break;
  case launcher_state::update_state::updating:
    ImGui::TextUnformatted("Updating launcher...");
    break;
  case launcher_state::update_state::up_to_date:
    ImGui::TextUnformatted("Launcher up to date");
    // TODO: we don't need to show anything for this state really, this is just for the WIP
    break;
  }

  switch(state.game_list_update_state) {
  case launcher_state::update_state::not_tried:
    // TODO: initialise fetch for latest game list
    break;
  case launcher_state::update_state::failed_to_connect:
    if(state.game_list.empty()) {
      ImGui::TextUnformatted("Failed to connect to loader repository, no game list available!");
    } else {
      ImGui::TextUnformatted("Failed to connect to loader repository, game list updates unavailable");
    }
    break;
  case launcher_state::update_state::downloading:
    ImGui::TextUnformatted("Downloading game list...");
    break;
  case launcher_state::update_state::up_to_date:
    ImGui::TextUnformatted("Game list loaded");
    break;
  }

  ImGui::End();
}

auto main()->int {
  std::cout << "Lost Engines Launcher" << std::endl;

  launcher_state state{};

  load_config(state);

  if(std::filesystem::exists(state.game_list_file)) {
    state.game_list = nlohmann::ordered_json::parse(std::ifstream{state.game_list_file});
  }
  std::cout << state.game_list["games"].size() << " games in current list at " << state.game_list_file << std::endl;

  std::cout << "Starting " << state.config.get<unsigned int>("network_threads") << " network threads" << std::endl;
  std::vector<std::jthread> network_threads;
  for(unsigned int i{0}; i != state.config.get<unsigned int>("network_threads"); ++i) {
    network_threads.emplace_back([&]{
      state.io_context.run();
    });
  }

  init_graphics("Lost Engines");

  GLFWwindow *window{glfwGetCurrentContext()};
  while(!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    if(glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {                      // no window rendering while minimised
      ImGui_ImplGlfw_Sleep(10);
      continue;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    draw_window(state);

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
