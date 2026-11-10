#include <iostream>
#include <boost/property_tree/ini_parser.hpp>
#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include "vectorstorm/vector/vector2.h"
#include "init_graphics.h"
#include "launcher_state.h"
#include "load_config.h"
#include "resource_loader.h"
#include "open_url.h"
#include "stream_string.h"
#include "get_game_download_url.h"

void draw_window(launcher_state &state, resource_loader &loader) {
  ImGuiViewport const &viewport{*ImGui::GetMainViewport()};
  ImGui::SetNextWindowPos(viewport.Pos);
  ImGui::SetNextWindowSize(viewport.Size);
  ImGuiWindowFlags window_flags{ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove};

  if(!ImGui::Begin("Lost Engines", nullptr, window_flags)) {
    ImGui::End();
    return;
  }

  // TODO: draw a nice logo banner
  // TODO: menu popout button in top right corner

  switch(state.launcher_update_state.load(std::memory_order_acquire)) {
  case launcher_state::update_state::not_tried:
    // TODO: launch update check
    break;
  case launcher_state::update_state::loading:
    ImGui::TextUnformatted("Checking for launcher updates...");
    break;
  case launcher_state::update_state::failed:
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

  switch(state.game_list_update_state.load(std::memory_order_acquire)) {
  case launcher_state::update_state::not_tried:
    loader.fetch_game_list();
    break;
  case launcher_state::update_state::failed:
    if(state.game_list.get().empty()) {
      ImGui::TextUnformatted("Failed to connect to loader repository, no game list available!");
      // TODO: more helpful diagnostics notes, a link to check access, a link to report a bug
    } else {
      ImGui::TextUnformatted("Failed to connect to loader repository, game list updates unavailable");
      // TODO: more helpful diagnostics notes
    }
    break;
  case launcher_state::update_state::downloading:
    ImGui::TextUnformatted("Downloading game list...");
    break;
  case launcher_state::update_state::updating:
    ImGui::TextUnformatted("Updating game list...");
    break;
  case launcher_state::update_state::up_to_date:
    //ImGui::TextUnformatted("Game list loaded");
    // nothing to draw, game list shown below
    break;
  }

  if(ImGui::BeginTable("Games", 7, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Name");
    ImGui::TableSetupColumn("Build",   ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Run",     ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Docs",    ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Source",  ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Explore", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Config",  ImGuiTableColumnFlags_WidthFixed, 40.0f);
    ImGui::TableHeadersRow();

    for(auto const &game : state.game_list->operator[]("games")) {
      auto const &name{game["name"].get<std::string>()};
      auto &selected_game_version{state.selected_game_versions[name]};

      ImGui::PushID(name.c_str());
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(name.c_str());

      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(-FLT_MIN);                                        // stretch combo to column width
      if(ImGui::BeginCombo("##version_select", selected_game_version.c_str())) {
        for(auto const &[version_name, this_version] : game["versions"].items()) {
          bool const is_selected{version_name == selected_game_version};
          if(ImGui::Selectable(version_name.c_str(), is_selected)) selected_game_version = version_name;
          {
            std::ostringstream tooltip_ss;
            tooltip_ss << "Build variant: " << version_name << "\n";
            for(auto const &[key, value] : this_version.items()) {              // list all of this version's properties in the tooltip
              tooltip_ss << key << ": " << value.dump(2) << "\n";
            }
            tooltip_ss << "Recommended variant for this game: " << game["preferred_version"].get<std::string>();
            ImGui::SetItemTooltip(tooltip_ss.str().c_str());
          }
          if(is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }
      ImGui::SetItemTooltip("Select a build variant to download and run");

      ImGui::TableNextColumn();
      auto &game_files_state{state.game_files_states[name + selected_game_version]};
      switch(game_files_state.game_download_state) {
      case launcher_state::game_files_state::download_states::not_started:
        if(ImGui::Button("Download", {-ImGui::GetFrameHeight(), 0.0f})) {       // extend the button to fill the frame minus width of the dropdown next to it
          auto const &game_download_url{get_game_download_url(game["versions"][selected_game_version])};
          std::filesystem::path install_destination{state.config.get<std::filesystem::path>("default_data_dir") / selected_game_version};
          // TODO: read game destinations specified manually in the config and override the default here
          loader.install_game(game_download_url, install_destination, game_files_state);
        }
        ImGui::SetItemTooltip("Download game and data to default directories");

        ImGui::SameLine(0.0f, 0.0f);
        if(ImGui::BeginCombo("##download_options", nullptr, ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft)) {
          if(ImGui::Button("Download game and data to default directories")) {
            // TODO
            ImGui::CloseCurrentPopup();
          }
          ImGui::SetItemTooltip("Use the default download location for game files and data");

          if(ImGui::Button("Choose download destination")) {
            // TODO
            ImGui::CloseCurrentPopup();
          }
          ImGui::SetItemTooltip("Choose this if you want the game files to be downloaded to a specific directory other than the default");

          // TODO: hide the below if the game doesn't have data files to download
          /*
          if(ImGui::Button("Use existing game install")) {
            // TODO
            ImGui::CloseCurrentPopup();
          }
          ImGui::SetItemTooltip("Choose this if you already have a copy of the original game installed, and want to use its data files");
          */

          if(ImGui::Button("Fetch game package manually")) {
            open_url(get_game_download_url(game["versions"][selected_game_version]));
            ImGui::CloseCurrentPopup();
          }
          ImGui::SetItemTooltip("Manually open the game package download URL in your browser");
          ImGui::EndCombo();
        }
        ImGui::SetItemTooltip("Download options");
        break;
      case launcher_state::game_files_state::download_states::in_progress:
        ImGui::ProgressBar(game_files_state.download_progress / 100.0f, ImVec2(-FLT_MIN, 0.0f), "Downloading game");
        if(game_files_state.download_progress.load(std::memory_order_relaxed) >= 0.0f) {
          ImGui::SetItemTooltip(stream_string{} << "Game files are downloading, progress " << game_files_state.download_progress << "%");
        } else {
          ImGui::SetItemTooltip("Game files are downloading");
        }
        // TODO: small cancel button
        break;
      case launcher_state::game_files_state::download_states::done:
        switch(game_files_state.data_download_state) {
        case launcher_state::game_files_state::download_states::not_started:
          ImGui::TextUnformatted("Installing game");
          break;
        case launcher_state::game_files_state::download_states::in_progress:
          ImGui::ProgressBar(game_files_state.download_progress / 100.0f, ImVec2(-FLT_MIN, 0.0f), "Downloading data");
          if(game_files_state.download_progress.load(std::memory_order_relaxed) >= 0.0f) {
            ImGui::SetItemTooltip(stream_string{} << "Game data files are downloading, progress " << game_files_state.download_progress << "%");
          } else {
            ImGui::SetItemTooltip("Game data files are downloading");
          }
          // TODO: small cancel button
          break;
        case launcher_state::game_files_state::download_states::done:
        case launcher_state::game_files_state::download_states::not_needed:
          if(ImGui::Button("Run", {-FLT_MIN, 0.0f})) {
            // TODO
          }
          ImGui::SetItemTooltip("Run the game at ");
          // TODO: binary path here
          break;
        case launcher_state::game_files_state::download_states::failed:
          if(ImGui::Button("Download failed")) {
            // TODO
          }
          ImGui::SetItemTooltip("Failed to download game data - press to try again, check logs for details");
          // TODO
          break;
        }

        break;
      case launcher_state::game_files_state::download_states::not_needed:
        assert(false || "this branch should never be reached for game files");
        // TODO: later we might want to add links to play in browser in this branch for games that need no download
        break;
      case launcher_state::game_files_state::download_states::failed:
        if(ImGui::Button("Download failed")) {
          // TODO
        }
        ImGui::SetItemTooltip("Failed to download game files - press to try again, check logs for details");
        // TODO
        break;
      }

      ImGui::TableNextColumn();
      // TODO: dropdown with docs links

      ImGui::TableNextColumn();
      if(ImGui::Button("Source code")) open_url(game["versions"][selected_game_version]["repo_url"].get<std::string>());
      // TODO, link emoji
      ImGui::SetItemTooltip("Explore the open source code of this build on Github");

      ImGui::TableNextColumn();
      if(ImGui::Button("Analysis")) open_url(game["analysis_url"].get<std::string>());
      // TODO, link emoji
      ImGui::SetItemTooltip("Explore game resource browsers and read the in-depth reverse engineering analysis for this game");

      ImGui::TableNextColumn();
      ImGui::Button("...");
      // TODO: popup menu
      ImGui::SetItemTooltip("More options");
      ImGui::PopID();
    }

    ImGui::EndTable();
  }

  // TODO: catch exceptions thrown by json parsing and report them in a friendly way to the user, advise them to make sure the game list and launcher are both up to date

  ImGui::End();
}

auto main()->int {
  std::cout << "Lost Engines Launcher" << std::endl;

  launcher_state state{};

  load_config(state);

  if(std::filesystem::exists(state.game_list_file)) {
    *state.game_list = nlohmann::ordered_json::parse(std::ifstream{state.game_list_file});
  }
  std::cout << state.game_list->operator[]("games").size() << " games in current list at " << state.game_list_file << std::endl;

  resource_loader loader{state};

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

    draw_window(state, loader);

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
