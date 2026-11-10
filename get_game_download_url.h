#pragma once
#include <boost/url/urls.hpp>
#include <nlohmann/json.hpp>

boost::urls::url get_game_download_url(nlohmann::json const &game_info);
