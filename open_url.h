#pragma once

/// Platform-independent functionality to open a URL in the default browser

#include <string>
#include <boost/url/urls.hpp>

void open_url(boost::urls::url const &url);
void open_url(std::string const &url);
