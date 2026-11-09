#include <filesystem>
#include <iostream>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/url/urls.hpp>
#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <nlohmann/json.hpp>
#include "vectorstorm/vector/vector2.h"
#include "platform_paths.h"

struct launcher_state {
  enum class update_state {
    not_tried,
    loading,
    failed_to_connect,
    outdated,
    downloading,
    updating,
    up_to_date,
  };
  update_state launcher_update_state{update_state::not_tried};
  update_state game_list_update_state{update_state::not_tried};

  std::filesystem::path config_dir{platform_paths::get_user_config_dir() / "lost-engines"};
  std::filesystem::path game_list_file;
  boost::urls::url game_list_url;

  nlohmann::json game_list;

  boost::property_tree::ptree config;
};

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

  std::cout << "Configuration directory: " << state.config_dir << std::endl;

  auto config_path{state.config_dir / "config.ini"};

  if(std::filesystem::exists(config_path)) {
    std::cout << "Loading configuration from " << config_path << std::endl;
    boost::property_tree::ini_parser::read_ini(config_path, state.config);
  } else {
    if(!std::filesystem::exists(state.config_dir)) {
      std::cout << "Creating configuration directory " << state.config_dir << std::endl;
      std::filesystem::create_directories(state.config_dir);
    }
    std::cout << "Creating default configuration in " << config_path << std::endl;
  }

  if(state.config.count("default_data_dir") == 0) {
    std::filesystem::path default_data_dir{platform_paths::get_user_data_dir() / "lost-engines"};
    state.config.put("default_data_dir", default_data_dir.string());
  }
  if(state.config.count("cache_dir") == 0) {
    std::filesystem::path cache_dir{platform_paths::get_user_cache_dir() / "lost-engines"};
    state.config.put("cache_dir", cache_dir.string());
  }
  if(state.config.count("launcher_repo_url") == 0) {
    state.config.put("launcher_repo_url", "https://github.com/Lost-Engines/lost-engines-launcher");
  }
  if(state.config.count("launcher_repo_raw_url") == 0) {
    state.config.put("launcher_repo_raw_url", "https://raw.githubusercontent.com/Lost-Engines/lost-engines-launcher/refs/heads/master");
  }
  state.game_list_file = state.config.get<std::filesystem::path>("cache_dir") / "game_list.json";
  state.game_list_url = boost::urls::url{state.config.get<std::string>("launcher_repo_raw_url")};
  state.game_list_url.segments().push_back("game_list.json");
  boost::property_tree::ini_parser::write_ini(config_path, state.config);

  std::cout << "Default data dir: " << state.config.get<std::filesystem::path>("default_data_dir") << std::endl;
  std::cout << "Launcher repository: " << state.config.get<std::string>("launcher_repo_url") << std::endl;
  std::cout << "Game list download: " << state.game_list_url << std::endl;
  std::cout << "Game list cache: " << state.game_list_file << std::endl;

  if(std::filesystem::exists(state.game_list_file)) {
    state.game_list = nlohmann::ordered_json::parse(std::ifstream{state.game_list_file});
  }

  std::cout << state.game_list["games"].size() << " games in current list at " << state.game_list_file << std::endl;

  std::cout << "Creating window..." << std::endl;

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
  ImGui_ImplOpenGL3_Init();

  std::cout << "Ready" << std::endl;

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
